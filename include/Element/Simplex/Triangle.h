#pragma once

// stl.
#include <vector>

// 3rd party.
#include <Eigen/Dense>

// salvus
#include <Utilities/Types.h>

// forward decl.
class Mesh;
class Source;
class Options;
class Receiver;
class ExodusModel;


template <typename ConcreteShape>
class Triangle: public ConcreteShape {

  /** \class Triangle
   * Base class of an abstract three node triangle. The reference element is set up as below.
   * 
   *        (-1,1)                          
   *         (v2)                           
   *          | \--                         
   *          |    \-                       
   *          |      \--                    
   *          |         \--                 
   *          |            \--              
   *   (s)    |               \--           
   *    ^     |                  \-         
   *    |     |                    \--      
   *    |    (v0)----------------------(v1) 
   *    |   (-1,-1)                    (1,-1)
   *    |
   *    |--------> (r)
   *
   */
 private:

  /*****************************************************************************
   * STATIC MEMBERS. THESE VARIABLES AND FUNCTIONS SHOULD APPLY TO ALL ELEMENTS.
   *****************************************************************************/

  // Static variables.
  const static int mNumDim = 2;
  const static int mNumVtx = 3;  /** < Num of element vertices. */  

  // Instance variables.
  PetscInt mElmNum;
  int mPlyOrd;
  int mNumIntPnt;
  int mNumDofVtx;
  int mNumDofEdg;
  int mNumDofFac;
  int mNumDofVol;

  // Vertex coordinates.
  Eigen::Matrix<double,mNumVtx,mNumDim> mVtxCrd;

  // Element center.
  Eigen::Vector2d mElmCtr;

  // Closure mapping.
  Eigen::VectorXi mClsMap;

  // Workspace.
  double mDetJac;
  Eigen::VectorXd mParWork;
  Eigen::VectorXd mStiffWork;
  Eigen::MatrixXd mGradWork;

  // On Boundary.
  bool mBndElm;
  std::map<std::string,std::vector<int>> mBnd;
  


  Eigen::MatrixXd mGradientOperator;
  /** < Derivative of shape function n (col) at pos. m (row) */
  Eigen::VectorXd mIntegrationWeights;
  /** < Integration weights along epsilon direction. */
  Eigen::VectorXd mIntegrationCoordinates_r;
  /** < Nodal location direction */
  Eigen::VectorXd mIntegrationCoordinates_s;  /** < Nodal location direction */

  /** r-derivative of Lagrange poly Phi [row: phi_i, col: phi @ nth point]*/
  static Eigen::MatrixXd mGradientPhi_dr;
  /** s-derivative of Lagrange poly Phi [row: phi_i, col: phi @ nth point]*/
  static Eigen::MatrixXd mGradientPhi_ds;

  /**********************************************************************************
   * OBJECT MEMBERS. THESE VARIABLES AND FUNCTIONS SHOULD APPLY TO SPECIFIC ELEMENTS.
   ***********************************************************************************/

  // Material parameters.
  std::map<std::string,Eigen::Vector3d> mPar;

  // Sources and receivers.
  std::vector<std::shared_ptr<Source>> mSrc;
  std::vector<std::shared_ptr<Receiver>> mRec;
  
  // precomputed element stiffness matrix (with velocities)
  Eigen::MatrixXd mElementStiffnessMatrix;

  // precomputed jacobians
  Eigen::Matrix2d mInvJac;
  Eigen::Matrix2d mInvJacT;
  Eigen::Matrix2d mInvJacT_x_invJac;
  
 public:

  /**
   * Constructor.
   * Sets quantities such as number of dofs, among other things, from the options class.
   * @param [in] options Populated options class.
   */
  Triangle<ConcreteShape>(std::unique_ptr<Options> const &options);

  /// All memory should be properly managed already.
  ~Triangle<ConcreteShape>() {};
  
  /**
   * Returns the gll locations for a given polynomial order.
   * @param [in] order The polynmomial order.
   * @returns tuple of quadrature points (r,s).
   */
  static std::tuple<Eigen::VectorXd, Eigen::VectorXd> QuadraturePoints(const int order);

  /**
   * Returns the quadrature intergration weights for a polynomial order.
   * @param [in] order The polynomial order.
   * @returns Vector of quadrature weights.
   * TODO: Move to autogenerated code.
   */
  static Eigen::VectorXd QuadratureIntegrationWeight(const int order);

  /**
   * Returns the mapping from the PETSc to Salvus closure.
   * @param [in] order The polynomial order.
   * @param [in] dimension Element dimension.
   * @returns Vector containing the closure mapping (field(closure(i)) = petscField(i))
   */
  static Eigen::VectorXi ClosureMapping(const int order, const int dimension);

  static Eigen::Vector3d interpolateAtPoint(double r, double s);

  void recordField(const Eigen::MatrixXd &u) { std::cerr << "ERROR: recordField not implemented\n"; exit(1); };

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
   * Compute the gradient of a field at all GLL points.
   * @param [in] field Field to take the gradient of.
   */
  RealMat computeGradient(const Eigen::Ref<const RealVec>& field);

  /**
   * Multiply a field by the gradient of the test functions and integrate.
   * @param [in] f Field to calculate on.
   */
  RealVec applyGradTestAndIntegrate(const Eigen::Ref<const RealMat>& f);
  
  /**
   * Interpolate a parameter from vertex to GLL point.
   * @param [in] par Parameter to interpolate (i.e. VP, VS).
   */
  RealVec ParAtIntPts(const std::string& par);
  
  /**
   * Attaches a material parameter to the vertices on the current element.
   * Given an exodus model object, we use a kD-tree to find the closest parameter to a vertex. In practice, this
   * closest parameter is often exactly coincident with the vertex, as we use the same exodus model for our mesh
   * as we do for our parameters.
   * @param [in] model An exodus model object.
   * @param [in] parameter_name The name of the field to be added (i.e. velocity, c11).
   */
  void attachMaterialProperties(std::unique_ptr<ExodusModel> const &model,
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
   * in mVertexCoordinates.
   * @param [in] distributed_mesh PETSc DM object.
   *
   */
  void attachVertexCoordinates(std::unique_ptr<Mesh> const &mesh);

  /**
   * Attach source.
   * Given a vector of abstract source objects, this function will query each for its spatial location. After
   * performing a convex hull test, it will perform a quick inverse problem to determine the position of any sources
   * within each element in reference coordinates. These reference coordinates are then saved in the source object.
   * References to any sources which lie within the element are saved in the mSrc vector.
   * @param [in] sources A vector of all the sources defined for a simulation run.
   */
  bool attachSource(std::unique_ptr<Source> &source, const bool finalize);
  
  /**
   * Atttach receiver.
   * Given a vector of abstract receiver objects, this function will query each for its spatial location. After
   * performing a convex hull test, it will perform a quick inverse problem to determine the position of any
   * sources within each element in reference Coordinates. These reference coordinates are then saved in the
   * receiver object. References to any receivers which lie within the element are saved in the mRec vector.
   * @param [in] receivers A vector of all the receivers defined for a simulation run.
   */
  bool attachReceiver(std::unique_ptr<Receiver> &receiver, const bool finalize);

  /**
   * Sets an edge to a particular scalar value (useful for Dirichlet boundaries)
   * @param [in] edg Edge id 0-2
   * @param [in] val Value to set
   * @param [out] f Field to set to `val`
   */
  void setEdgeToValue(const PetscInt edg,
                      const PetscScalar val,
                      Eigen::Ref<RealVec> f);
  
  
  /**
   * If an element is detected to be on a boundary, apply the Dirichlet condition to the
   * dofs on that boundary.
   * @param [in] mesh The mesh instance.
   * @param [in] options The options class.
   * @param [in] fieldname The field to which the boundary must be applied.
   */
  void applyDirichletBoundaries(std::unique_ptr<Mesh> const &mesh, std::unique_ptr<Options> const &options, const std::string &fieldname);

  /**
   * Given some field at the GLL points, interpolate the field to some general point.
   * @param [in] pnt Position in reference coordinates.
   */
  Eigen::MatrixXd interpolateFieldAtPoint(const Eigen::VectorXd &pnt) { std::cerr << "ERROR: Not implemented"; exit(1); }

  /**
   *
   */
  Eigen::VectorXd getDeltaFunctionCoefficients(const Eigen::Ref<RealVec>& pnt);

  Eigen::MatrixXd buildStiffnessMatrix(Eigen::VectorXd velocity);

  double CFL_constant();
  
  /** Return the estimated element radius
   * @return The CFL estimate
   */
  double estimatedElementRadius();
  
  
  // Setters
  inline void SetNumNew(const PetscInt num) { mElmNum = num; }
  inline void SetVtxCrd(const Eigen::Ref<const Eigen::Matrix<double,3,2>> &v) { mVtxCrd = v; }

  /**
   * Multiply a field by the test functions and integrate.
   * @param [in] f Field to calculate on.
   */
  Eigen::VectorXd applyTestAndIntegrate(const Eigen::Ref<const Eigen::VectorXd>& f);

  /**
   * Figure out and set boundaries.
   * @param [in] mesh The mesh instance.
   */
  void setBoundaryConditions(std::unique_ptr<Mesh> const &mesh);

  
  // Getters.
  inline PetscInt ElmNum()        const { return mElmNum; }
  inline bool BndElm()            const { return mBndElm; }
  inline int NumDim()             const { return mNumDim; }
  inline int NumIntPnt()          const { return mNumIntPnt; }
  inline int NumDofVol()          const { return mNumDofVol; }
  inline int NumDofFac()          const { return mNumDofFac; }
  inline int NumDofEdg()          const { return mNumDofEdg; }
  inline int NumDofVtx()          const { return mNumDofVtx; }
  inline Eigen::MatrixXi ClsMap() const { return mClsMap; }
  inline int PlyOrd()             const { return mPlyOrd; }
  inline Eigen::MatrixXd VtxCrd() const { return mVtxCrd; }
  std::vector<std::shared_ptr<Source>> Sources() { return mSrc; }

  
  /**
   * Builds nodal coordinates (x,z) on all mesh degrees of freedom.
   * @param mesh [in] The mesh.
   */
  // Delegates.
  std::tuple<Eigen::VectorXd, Eigen::VectorXd> buildNodalPoints() {
    return ConcreteShape::buildNodalPoints(mIntegrationCoordinates_r, mIntegrationCoordinates_s, mVtxCrd);
  };

};
