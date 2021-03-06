#pragma once

// stl.
#include <memory>
#include <vector>

// 3rd party.
#include <mpi.h>
#include <petsc.h>
#include <Eigen/Dense>

class Mesh;
class Model;
class Source;
class Options;
class Receiver;
class ExodusModel;

#include <Source/Source.h>
#include <Receiver/Receiver.h>

class Element {
  /** \class Element
    *
    * \brief Abstract interface class, defining methods which all derived elements must implement.
    *
    * This class defines a 'window' into a finite element, through which the time stepping routines may look. All the
    * below methods must be implemented in one way or another by the derived classes. Keep in mind however, that the
    * actual views into each elements are filtered through the ElementAdapter class, and in practice each of these
    * virtual functions are concretely implemented in ElementAdapter.
    *
    * Note that no instance variables are stored -- the class in simply a pure interface. For this reason we can get
    * away with just having a trivial constructor.
    */
 public:

  /** @name Management.
   * These methods are mainly responsible for memory management, i.e. the creation and desctruction
   * of individual elements.
   */
  ///@{
  /** Cleans up any heap-allocated memoroy. */
  virtual ~Element() {};
  /** Returns a concrete elment type based on command line options */
  static std::unique_ptr<Element> Factory(const std::string &shape,
                                          const std::vector<std::string> &physics_base,
                                          const std::vector<std::string> &physics_couple,
                                          std::unique_ptr<Options> const &options);
  ///@}

  /** @name Element setup.
   * These methods are responsible for setting up each individual element for use within a time loop.
   * Basically, we are building up a complex function object.
   */
  ///@{
  /** Construct the mass matrix on the element, and sum into global DOF.
   * @param [in/out] mesh Mesh instance. Global and local vectors modified.
   */
  virtual Eigen::MatrixXd assembleElementMassMatrix() = 0;
  /** Attach material parameters to the element given some model.
   * @param [in] model Model instance.
   */
  virtual void attachMaterialProperties(std::unique_ptr<ExodusModel> const &model) = 0;
  /** Attach receivers to the element (if required).
   * @param [in/out] receivers Vector of all receivers in the model. Receiver reference coordinates are attached.
   */
  virtual bool attachReceiver(std::unique_ptr<Receiver> &receiver, const bool finalize) = 0;
  /** Attach sources to the element (if required).
   * @param [in] sources Vector of all sources in the model.
   */
  virtual bool attachSource(std::unique_ptr<Source> &source, const bool finalize) = 0;
  /** Attach vertex coordinates to the element.
   * @param [in] distributed_mesh The parallel DM provided by PETSc.
   */
  virtual void attachVertexCoordinates(std::unique_ptr<Mesh> const &mesh) = 0;

  /** Precompute any terms needed on the element level, e.g.,
      jacobians, velocities at nodes, stiffness matrices for triangles
      and tetrahedra.
   */
  virtual void precomputeElementTerms() = 0;
  ///@}

  
  
  /** @name Time loop (pure functions).
   * These functions are called from within the time loop. At this point, the element is a fully constructed
   * function.
   */
  ///@{
  /** Returns the interpolated source for a given time.
   * @ param [in] time Simulation time.
   * @ param [in] time_idx Simulation time index.
   */
  virtual Eigen::MatrixXd computeSourceTerm(const double time, const PetscInt time_idx) = 0;
  /** Returns the action of the stiffness matrix applied to some field (probably displacement).
   * @param [in] u Displacement field.
   */
  virtual Eigen::MatrixXd computeStiffnessTerm(const Eigen::Ref<const Eigen::MatrixXd>& u) = 0;
  /** Computes the surface integral over an element. Note that this is usually zero. */
  virtual Eigen::MatrixXd computeSurfaceIntegral(const Eigen::Ref<const Eigen::MatrixXd>& u) = 0;
  /** Returns the fields which are required from the global DOFs for local operation */
  virtual std::vector<std::string> PullElementalFields() const = 0;
  /** Returns the fields from the global DOFs into which we will sum */
  virtual std::vector<std::string> PushElementalFields() const = 0;
  /* TODO: Check if the following function is in the right place. */
  /** Returns the (real-space) lagrange polynomials evaluated at some point. */
  virtual Eigen::MatrixXd interpolateFieldAtPoint(const Eigen::Ref<const Eigen::VectorXd>& pnt) = 0;
  ///@}

  /** @name Time loop (functions with side effects).
   * These functions are also called from within the time loop, but they have a roll which is slightly more
   * interesting than just transforming input into output. Perhaps we may find a way to get rid of them.
   */
  ///@{
  /** Sets the mesh boundaries to a certain value (i.e. Dirichlet, Neumann, ...). Note that this only refers to
   * the actualy physical boundaries of the mesh. Internal material boundaries are handled implicitly by the
   * elements.
   * @param [in/out] mesh Mesh instance. Local/global boundary values are modified.
   */
  virtual void setBoundaryConditions(std::unique_ptr<Mesh> const &mesh) = 0;
  /** Triggers a record on all receivers which belong to a certain element.
   * @param [in] field The field to record.
   */
  virtual void recordField(const Eigen::Ref<const Eigen::MatrixXd>& field) = 0;
  ///@}

  /** @name Setters/Getters.
   * The time loop occasionally needs access to some instance variables stored in the derived classes. These functions
   * are meant to return directly from those derived classes, so that no instance variables are stored in the interface
   * itself.
   */
  ///@{
  /** Set the element number of the local partition. */
  virtual inline void SetNum(const int num) = 0;
  /** Is this element on a mesh boundary. */
  virtual inline bool BndElm() const = 0;
  /** What is this elements number on the local partition. */
  virtual inline int Num() const = 0;
  /** What is the dimension of this element. */
  virtual inline int NumDim() const = 0;
  /** How many degrees of freedom in this element's volume. */
  virtual inline int NumDofVol() const = 0;
  /** How many degrees of freedom in this element's face. */
  virtual inline int NumDofFac() const = 0;
  /** How many degrees of freedom on this element's edges. */
  virtual inline int NumDofEdg() const = 0;
  /** How many degrees of freedom on this element's verticies. */
  virtual inline int NumDofVtx() const = 0;
  /** How many integration point on this element. */
  virtual inline int NumIntPnt() const = 0;
  /** What is the closure map on this element. */
  virtual inline Eigen::VectorXi ClsMap() const = 0;
  /** What is the order of the element */
  virtual inline int PlyOrd() const = 0;
  /** Vertex coordinates of this element. */
  virtual inline Eigen::MatrixXd VtxCrd() const = 0;
  /** What type of element am I? */
  virtual inline std::string Name() const = 0;
  ///@}

};
