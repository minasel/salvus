#pragma once

#include <Eigen/Dense>
#include <Model/ExodusModel.h>
#include <Source/Source.h>
#include <Mesh/Mesh.h>
#include <Element/Element.h>

extern "C" {
#include <Triangle/Autogen/p3_triangle.h>
}


/**
 * Base class of an abstract three node triangle. The reference element is set up as below.
 *
 *    (-1,1)
 *     (v2)
 *      | \--                          
 *      |    \-                        
 *      |      \--                     
 *      |         \--                  
 *      |            \--               
 *      |               \--            
 *      |                  \-          
 *      |                    \--       
 *     (v0)----------------------(v1)
 *   (-1,-1)                    (1,-1)
 *
 *  (s)
 *   ^
 *   |
 *   |
 *   |______> (r)
 */

class Triangle: public Element {

 protected:

  /*****************************************************************************
   * STATIC MEMBERS. THESE VARIABLES AND FUNCTIONS SHOULD APPLY TO ALL ELEMENTS.
   *****************************************************************************/

  static int mNumberVertex;
  /** < Num of element vertices. */

  static Eigen::MatrixXd mGradientOperator;
  /** < Derivative of shape function n (col) at pos. m (row) */
  static Eigen::VectorXd mIntegrationWeights;
  /** < Integration weights along epsilon direction. */
  static Eigen::VectorXd mIntegrationCoordinates_r;
  /** < Nodal location direction */
  static Eigen::VectorXd mIntegrationCoordinates_s;  /** < Nodal location direction */

  /** r-derivative of Lagrange poly Phi [row: phi_i, col: phi @ nth point]*/
  static Eigen::MatrixXd mGradientPhi_dr;
  /** s-derivative of Lagrange poly Phi [row: phi_i, col: phi @ nth point]*/
  static Eigen::MatrixXd mGradientPhi_ds;

  /**********************************************************************************
   * OBJECT MEMBERS. THESE VARIABLES AND FUNCTIONS SHOULD APPLY TO SPECIFIC ELEMENTS.
   ***********************************************************************************/

 public:

  /**
   * Factory return the proper element physics based on the command line options.
   * @return Some derived element class.
   */
  static Triangle *factory(Options options);

  /**
   * Constructor.
   * Sets quantities such as number of dofs, among other things, from the options class.
   * @param [in] options Populated options class.
   */
  Triangle(Options options);

  /**
   * Copy constructor.
   * Returns a copy. Use this once the reference element is set up via the constructor, to allocate space for
   * all the unique elements on a processor.
   */
  virtual Triangle *clone() const = 0;

  /**
   * Returns the gll locations for a given polynomial order.
   * @param [in] order The polynmomial order.
   * @returns tuple of quadrature points (r,s).
   */
  static std::tuple<Eigen::VectorXd, Eigen::VectorXd> QuadraturePointsForOrder(const int order);

  /**
   * Returns the quadrature intergration weights for a polynomial order.
   * @param [in] order The polynomial order.
   * @returns Vector of quadrature weights.
   * TODO: Move to autogenerated code.
   */
  static Eigen::VectorXd QuadratureIntegrationWeightForOrder(const int order);

  /**
   * Returns the mapping from the PETSc to Salvus closure.
   * @param [in] order The polynomial order.
   * @param [in] dimension Element dimension.
   * @returns Vector containing the closure mapping (field(closure(i)) = petscField(i))
   */
  static Eigen::VectorXi ClosureMapping(const int order, const int dimension);

  static Eigen::Vector3d interpolateAtPoint(double r, double s);

  // currently not possible
  // /**
  //  * Returns the face mapping from the PETSc to Salvus closure.
  //  * @param [in] order The polynomial order.
  //  * @param [in] dimension Element dimension.
  //  * @returns Vector containing the closure mapping (field(closure(i)) = petscField(i))
  //  */
  // static Eigen::VectorXi FaceClosureMapping(const int order, const int dimension);

  // Attribute gets.

  // virtual Eigen::VectorXi GetFaceClosureMapping() { return mFaceClosureMapping; }

  // virtual Eigen::MatrixXd VtxCrd() { return mVtxCrd; }

  /********************************************************
   ********* Inherited methods from Element 2D ************
   ********************************************************/

  /**
   * Checks whether a given point in realspace (x, z) is within the current element.
   * A simple convex hull algorithm is implemented. Should work as long as the sides of the element are straight
   * lines, but will likely fail for higher order shape functions.
   * @param [in] x X-coordinate in real space.
   * @param [in] z Z-coordinate in real space.
   * @returns True if point is inside, False if not.
   */
  bool mCheckHull(double x, double z);

  /**
   * 2x2 inverse Jacobian matrix at a point (eps, eta).  This method returns an Eigen::Matrix
   * representation of the inverse Jacobian at a particular point.
   * @param [in] eps Epsilon position on the reference element.
   * @param [in] eta Eta position on the reference element.
   * @returns (inverse Jacobian matrix,determinant of that matrix) as a `std::tuple`. Tuples can be
   * "destructured" using a `std::tie`.
   */
  std::tuple<Eigen::Matrix2d, PetscReal> inverseJacobianAtPoint(PetscReal eps, PetscReal eta);

  /**
   * Given a point in realspace, determines the equivalent location in the reference element.
   * Since the shape function is linear, the inverse transform is a simple analytic formula.
   * @param [in] x_real X-coordinate in real space.
   * @param [in] z_real Z-coordinate in real space.
   * @return A Vector (eps, eta) containing the coordinates in the reference element.
   */
  Eigen::Vector2d inverseCoordinateTransform(const double &x_real, const double &z_real);

  /**
   * Attaches a material parameter to the vertices on the current element.
   * Given an exodus model object, we use a kD-tree to find the closest parameter to a vertex. In practice, this
   * closest parameter is often exactly coincident with the vertex, as we use the same exodus model for our mesh
   * as we do for our parameters.
   * @param [in] model An exodus model object.
   * @param [in] parameter_name The name of the field to be added (i.e. velocity, c11).
   * @returns A Vector with 4-entries... one for each Element vertex, in the ordering described above.
   */
  Eigen::Vector3d __attachMaterialProperties(ExodusModel *model,
                                             std::string parameter_name);

  /**
   * Utility function to integrate a field over the element. This could probably be made static, but for now I'm
   * just using it to check some values.
   * @param [in] field The field which to integrate, defined on each of the gll points.
   * @returns The scalar value of the field integrated over the element.
   */
  double integrateField(const Eigen::VectorXd &field);

  /**
   * Setup the auto-generated gradient operator, and stores the result in mGrd.
   */
  void setupGradientOperator();


  /**
   * Queries the passed DM for the vertex coordinates of the specific element. These coordinates are saved
   * in mVertexCoordiantes.
   * @param [in] distributed_mesh PETSc DM object.
   *
   */
  void attachVertexCoordinates(DM &distributed_mesh);

  /**
   * Attach source.
   * Given a vector of abstract source objects, this function will query each for its spatial location. After
   * performing a convex hull test, it will perform a quick inverse problem to determine the position of any sources
   * within each element in reference coordinates. These reference coordinates are then saved in the source object.
   * References to any sources which lie within the element are saved in the mSrc vector.
   * @param [in] sources A vector of all the sources defined for a simulation run.
   */
  void attachSource(std::vector<Source *> sources);

  /**
   * Atttach receiver.
   * Given a vector of abstract receiver objects, this function will query each for its spatial location. After
   * performing a convex hull test, it will perform a quick inverse problem to determine the position of any
   * sources within each element in reference coordiantes. These reference coordinates are then saved in the
   * receiver object. References to any receivers which lie within the element are saved in the mRec vector.
   * @param [in] receivers A vector of all the receivers defined for a simulation run.
   */
  void attachReceiver(std::vector<std::shared_ptr<Receiver>> &receivers);

  /**
   * Simple function to set the (remembered) element number.
   */
  void SetNum(int element_number) { mElmNum = element_number; }

  /**
   * Builds nodal coordinates (x,z) on all mesh degrees of freedom.
   * @param mesh [in] The mesh.
   */
  std::tuple<Eigen::VectorXd, Eigen::VectorXd> buildNodalPoints();

};
