#pragma once

// stl.
#include <set>
#include <memory>
#include <map>
#include <iosfwd>
#include <string>
#include <vector>
#include <assert.h>
#include <iostream>

// 3rd party.
#include <petsc.h>
#include <Eigen/Dense>
#include <Element/Element.h>

// forward decl.
class Options;
class ExodusModel;

using std::unique_ptr;

class Mesh {

  std::set<std::tuple<PetscInt,PetscInt>> mBndPts;

  /** Keeps track of all the fields defined in the mesh. **/
  std::set<std::string> mMeshFields;

  /** Keeps track of the primary field on each element. **/
  std::vector<std::set<std::string>> mElmFields;

  /** Keeps track of any (possible) coupling fields on each element. **/
  std::map<PetscInt,std::set<std::string>> mPointFields;

  /** < Exodus file from which this mesh (skeleton) was defined. */
  std::string mExodusFileName;

  /** < PETSc distributed mesh defining parallel element layout. */
  DM mDistributedMesh;

  /** < Mesh section describing location of the integration points. */
  PetscSection mMeshSection;

  PetscInt mNumberElementsLocal; /** < Num of elements on this processor. */
  PetscInt mNumDim;    /** < Num of dimensions of the mesh. */
  PetscInt mNumberSideSets;      /** < Num of flagged boundaries. */
  PetscInt int_tstep;            /** < Timestep number. */

 protected:

  /** < List of field names on global dof ("u","v",etc) */
  std::vector<std::string> mGlobalFields;

  /** < Holds information used to dump field values to disk. */
  PetscViewer mViewer;

  /** The CFL constant for the time-stepping scheme (Newmark 2nd-order sets 1.0)*/
  double mCFL = 1.0;

  std::map<PetscInt, std::string> mBoundaryIds;
  /** < mapping between boundary id
                                                       and its associated name (e.g.,
                                                       "absorbing boundary" in the
                                                       petsc side set collection) */


  std::map<std::string, std::map<int, std::vector<int>>>
      mBoundaryElementFaces;  /** < list of elements on a boundary and the corresponding
                                    boundary faces. Each boundary has its own list of
                                    elements, and each element has its own list of
                                    faces. */


 public:

  Mesh(const std::unique_ptr<Options> &options);

  /**
   * Factory which returns a `Mesh` based on user defined options.
   * The `Mesh` is really a collection of the global dofs, and the fields defined on these dofs will depend on both
   * the physics of the system under consideration, and the method of time-stepping chosen.
   * @return Some derived mesh class.
   */
  static std::unique_ptr<Mesh> Factory(const std::unique_ptr<Options> &options);

  virtual ~Mesh() {
    if (mMeshSection) { PetscSectionDestroy(&mMeshSection); }
    if (mDistributedMesh) { DMDestroy(&mDistributedMesh); }
  }

  /**
   * Builds a mesh from a list of vertices and cells.
   * @param [in] dim dimension
   * @param [in] numCells number of cells
   * @param [in] numVerts number of vertices
   * @param [in] numVertsPerElem number of vertices per element
   * @param [in] cells The list of cell connectivity
   * @param [in] vertex_coords The list of vertex coordinates
   */
  void read(int dim, int numCells, int numVerts, int numVertsPerElem,
            int* cells, double* vertex_coords);
  
  /**
   * Reads an exodus mesh from a file defined in options.
   * By the time this method is finished, the mesh has been
   * read, and parallelized across processors (via a call to Chaco).
   * @param [in] options The master options struct. TODO: Move the gobbling of options to the constructor.
   */
  void read();

  /**
   * Generate coupling and boundary layers.
   * This function walks through the graph specified by DMPLEX, and uses the information provided in model to
   * generate coupling interfaces and boundary layers.
   * @param model A pointer to the Exodus model.
   * @param options The global options object.
   */
  void setupTopology(unique_ptr<ExodusModel> const &model,
                     unique_ptr<Options> const &options);

  /**
   * Sets up the dofs across elements and processor boundaries.
   * Specifically, this function defines a `DMPlex section` spread across all elements. It is this section that
   * defines the integration points, and carries information about which of these points are shared across processor
   * boundaries. This function takes a model object because it needs to know about the physics attached
   * to each element. It uses the physics to set an appropriate amount of components belonging to each DOF.
   * @param [in] number_dof_vertex Number of dofs per 0-d mesh component (vertex). 1 for the standard GLL basis.
   * @param [in] number_dof_edge Number of dofs per 1-d mesh component (edge). order-1 for the standard GLL basis.
   * @param [in] number_dof_face Number of dofs per 2-d mesh component (face). (order-1)^2 for the standard GLL basis.
   * @param [in] number_dof_volume Num of dofs per 3-d mesh component (volume). Something something for the
   * standard GLL basis.
   */
  void setupGlobalDof( std::unique_ptr<Element> const &element,
                      unique_ptr<Options> const &options);

  /**
   * Determines which type of mesh we are working with (tri/tet/quad/hex). For now, only supports
   * single element types.
   * @return Element type ("tri","tet","quad","hex").
   */
  std::string baseElementType();

  /**
   * Number of elements owned by the current processors.
   * @ return Num of Elements.
   */
  inline PetscInt NumberElementsLocal() { return mNumberElementsLocal; }

  /**
   * Defines the field to visualize, and the name of the file to dump to.
   * @param [in] movie_filename Where to save the file.
   * TODO: Should add field name here, and interval? Do we need to support other dump options than HDF5?
   */
  void setUpMovie(const std::string &movie_filename);

  /**
   * Commits a movie frame to disk, using the interface defined in setUpMovie.
   */
  void saveFrame(std::string name, PetscInt timestep);

  /**
   * Needs to be called at the end of a time loop if a movie is desired.
   * Cleans up the open HDF5 file, and safely shuts down the output stream.
   */
  void finalizeMovie();

  /**
   * Set a desired vec_struct to zero. Usefull for those terms which we sum into (i.e. element forcing).
   */
  void zeroFields(const std::string &name);

  /**
   * Read and setup boundaries (exodus side sets). Gets list of faces from Petsc and builds
   * corresponding list of elements, which lay on each boundary.
   */
  int setupBoundaries(std::unique_ptr<Options> const &options);

  /**
   * Reads boundaries (exodus side sets) from mesh file and builds appropriate boundary name to
   * petsc id mapping.
   */
  int readBoundaryNames(std::unique_ptr<Options> const &options);

  PetscInt GetNeighbouringElement(const PetscInt interface, const PetscInt this_elm) const;

  /**
   * Get the transitive closure of a coordinate section for a mesh point.
   */
  Eigen::MatrixXd getElementCoordinateClosure(PetscInt elem_num);

  static PetscInt numFieldPerPhysics(std::string physics);

  inline std::vector<std::string> ElementFields(const PetscInt num) {
    return std::vector<std::string> (mPointFields[num].begin(), mPointFields[num].end());
  }

  inline DM &DistributedMesh() { return mDistributedMesh; }
  inline PetscSection &MeshSection() { return mMeshSection; }

  std::map<PetscInt, std::string> &BoundaryIds() { return mBoundaryIds; }

  inline std::set<std::tuple<PetscInt,PetscInt>> BoundaryPoints() { return mBndPts; }

  inline int NumberSideSets() { return mNumberSideSets; }
  inline int NumberDimensions() { return mNumDim; }

  inline std::map<std::string, std::map<int, std::vector<int>>>
  BoundaryElementFaces() { return mBoundaryElementFaces; }
  std::set<std::string> AllFields() const { return mMeshFields; }

  std::vector<std::tuple<PetscInt,std::vector<std::string>>> CouplingFields(const PetscInt elm);
  std::vector<std::string> TotalCouplingFields(const PetscInt elm);
  std::vector<PetscInt> EdgeNumbers(const PetscInt elm);

  inline double CFL() { return mCFL; }
  
};
