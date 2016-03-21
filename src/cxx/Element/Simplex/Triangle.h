#pragma once

#include <Eigen/Dense>
#include <Model/ExodusModel.h>
#include <Source/Source.h>
#include <Mesh/Mesh.h>
#include <Element/Element.h>

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

class Triangle : Element2D {

protected:

    /*****************************************************************************
     * STATIC MEMBERS. THESE VARIABLES AND FUNCTIONS SHOULD APPLY TO ALL ELEMENTS.
     *****************************************************************************/

    static const int mNumberVertex = 3;         /** < Number of element vertices. */

    static Eigen::VectorXi mClosureMapping;             /** < Mapping from our element closure
                                                            numbering to PETSc's */

    // currently not possible...
    // static Eigen::VectorXi mFaceClosureMapping;             /** < Mapping from our face closure
    //                                                             numbering to PETSc's */
    
    static Eigen::MatrixXd mGradientOperator;           /** < Derivative of shape function n (col) at pos. m (row) */
    static Eigen::VectorXd mIntegrationWeights;      /** < Integration weights along epsilon direction. */
    static Eigen::VectorXd mIntegrationCoordinates;  /** < Integration points along epsilon direction */

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
    virtual Triangle * clone() const = 0;

    /**
     * Returns the gll locations for a given polynomial order.
     * @param [in] order The polynmomial order.
     * @returns Vector of quadrature points.
     * TODO: Move to autogenerated code.
     */
    static Eigen::VectorXd QuadraturePointsForOrder(const int order);

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

    virtual Eigen::MatrixXd GetVertexCoordinates() { return mVertexCoordinates; }

    /********************************************************
     ********* Inherited methods from Element 2D ************
     ********************************************************/
        
    /**
     * 2x2 Jacobian matrix at a point (eps, eta).
     * This method returns an Eigen::Matrix representation of the Jacobian at a particular point. Since the return
     * value is this Eigen object, we can perform additional tasks on the return value (such as invert, and get
     * determinant).
     * @param [in] eps Epsilon position on the reference element.
     * @param [in] eta Eta position on the reference element.
     * @returns 2x2 statically allocated Eigen matrix object.
     */
    Eigen::Matrix<double,2,2> jacobianAtPoint(PetscReal eps, PetscReal eta) = 0;

    /**
     * 2x2 inverse Jacobian matrix at a point (eps, eta).  This method returns an Eigen::Matrix
     * representation of the inverse Jacobian at a particular point.
     * @param [in] eps Epsilon position on the reference element.
     * @param [in] eta Eta position on the reference element.     
     * @returns (inverse Jacobian matrix,determinant of that matrix) as a `std::tuple`. Tuples can be
     * "destructured" using a `std::tie`.
     */
    std::tuple<Eigen::Matrix2d,PetscReal> inverseJacobianAtPoint(PetscReal eps, PetscReal eta) = 0;

    /**
     * Attaches a material parameter to the vertices on the current element.
     * Given an exodus model object, we use a kD-tree to find the closest parameter to a vertex. In practice, this
     * closest parameter is often exactly coincident with the vertex, as we use the same exodus model for our mesh
     * as we do for our parameters.
     * @param [in] model An exodus model object.
     * @param [in] parameter_name The name of the field to be added (i.e. velocity, c11).
     * @returns A Vector with 4-entries... one for each Element vertex, in the ordering described above.
     */
    Eigen::Vector4d __interpolateMaterialProperties(ExodusModel *model,
                                                            std::string parameter_name) = 0;

    /**
     * Utility function to integrate a field over the element. This could probably be made static, but for now I'm
     * just using it to check some values.
     * @param [in] field The field which to integrate, defined on each of the gll points.
     * @returns The scalar value of the field integrated over the element.
     */
    double integrateField(const Eigen::VectorXd &field) = 0;

    /**
     * Setup the auto-generated gradient operator, and stores the result in mGradientOperator.
     */
    void setupGradientOperator() = 0;

    
    /**
     * Queries the passed DM for the vertex coordinates of the specific element. These coordinates are saved
     * in mVertexCoordiantes.
     * @param [in] distributed_mesh PETSc DM object.
     *
     */
    void attachVertexCoordinates(DM &distributed_mesh) = 0;

    /**
     * Attach source.
     * Given a vector of abstract source objects, this function will query each for its spatial location. After
     * performing a convex hull test, it will perform a quick inverse problem to determine the position of any sources
     * within each element in reference coordinates. These reference coordinates are then saved in the source object.
     * References to any sources which lie within the element are saved in the mSources vector.
     * @param [in] sources A vector of all the sources defined for a simulation run.
     */
    void attachSource(std::vector<Source*> sources) = 0;

    /**
     * Simple function to set the (remembered) element number.
     */
    void SetLocalElementNumber(int element_number) { mElementNumber = element_number; }
    
    /**
     * Sums a field into the mesh (global dofs) owned by the current processor.
     * This function sets up and calls PLEX's DMVecSetClosure for a given element. Remapping is handled implicitly.
     * @param mesh [in] A reference to the mesh to which this element belongs.
     * @param field [in] The values of the field on the element, in Salvus ordering.
     * @param name [in] The name of the global fields where the field will be summed.
     * TODO: Make this function check if the field is valid?
     */
    void checkInFieldElement(Mesh *mesh, const Eigen::VectorXd &field, const std::string name) = 0;
    
    /**
     * Sets a field into the mesh (global dofs) owned by the current processor.
     * This function sets up and calls PLEX's DMVecSetClosure for a given element. Remapping is handled implicitly.
     * @param mesh [in] A reference to the mesh to which this element belongs.
     * @param field [in] The values of the field on the element, in Salvus ordering.
     * @param name [in] The name of the global fields where the field will be summed.
     * TODO: Make this function check if the field is valid?
     */
    void setFieldElement(Mesh *mesh, const Eigen::VectorXd &field, const std::string name) = 0;
    
    /**
     * Queries the mesh for, and returns, a field.
     * This function returns a Matrix (or Vector -- a Eigen::Vector is just a special case of a matrix) from the
     * global dofs owned by the current processor. The returned field will be in Salvus ordering -- remapping is
     * done implicitly. If the field is multi dimensional, the dimensions will be ordered as rows (i.e.
     * row(0)->x, row(1)->z).
     * @param [in] mesh Pointer to the mesh representing the current element.
     * @param [in] name Name of field to check out.
     */
    Eigen::VectorXd checkOutFieldElement(Mesh *mesh, const std::string name) = 0;

    /**
     * Builds nodal coordinates (x,z) on all mesh degrees of freedom.
     * @param mesh [in] The mesh.
     */
    std::tuple<Eigen::VectorXd,Eigen::VectorXd> buildNodalPoints(Mesh* mesh) = 0;

    /**
     * Figure out which dofs (if any) are on the boundary.
     */
    void setBoundaryConditions(Mesh *mesh) = 0;

    
};
