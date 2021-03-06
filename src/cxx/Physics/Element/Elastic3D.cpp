#include <Mesh/Mesh.h>
#include <Utilities/Options.h>
#include <Model/ExodusModel.h>
#include <Physics/Elastic3D.h>
#include <Source/Source.h>
#include <Utilities/Types.h>
#include <Utilities/Logging.h>

using namespace Eigen;

template <typename Element>
Elastic3D<Element>::Elastic3D(std::unique_ptr<Options> const &options): Element(options) {

  int num_stress_cmp = 6;
  int num_strain_cmp = 9;
  mRho.setZero(Element::NumIntPnt());
  mc11.setZero(Element::NumIntPnt());
  mc12.setZero(Element::NumIntPnt());
  mc13.setZero(Element::NumIntPnt());
  mc22.setZero(Element::NumIntPnt());
  mc23.setZero(Element::NumIntPnt());
  mc33.setZero(Element::NumIntPnt());
  mc44.setZero(Element::NumIntPnt());
  mc55.setZero(Element::NumIntPnt());
  mc66.setZero(Element::NumIntPnt());
//  mStiff.setZero(Element::NumIntPnt(), Element::NumDim());
//  mStress.setZero(Element::NumIntPnt(), num_stress_cmp);
//  mStress.setZero(Element::NumIntPnt(), num_strain_cmp);

}

template <typename Element>
void Elastic3D<Element>::attachMaterialProperties(std::unique_ptr<ExodusModel> const &model) {

  Element::attachMaterialProperties(model, "RHO");
  Element::attachMaterialProperties(model, "VPV");
  Element::attachMaterialProperties(model, "VPH");
  Element::attachMaterialProperties(model, "VSV");
  Element::attachMaterialProperties(model, "VSH");
  Element::attachMaterialProperties(model, "ETA");

  mRho = Element::ParAtIntPts("RHO");
  mc11 = mRho * Element::ParAtIntPts("VPH").array().pow(2);
  mc22 = mRho * Element::ParAtIntPts("VPH").array().pow(2);
  mc33 = mRho * Element::ParAtIntPts("VPV").array().pow(2);
  mc44 = mRho * Element::ParAtIntPts("VSV").array().pow(2);
  mc55 = mRho * Element::ParAtIntPts("VSV").array().pow(2);
  mc66 = mRho * Element::ParAtIntPts("VSH").array().pow(2);

  mc12 = mc11 - 2 * mc66;
  mc13 = Element::ParAtIntPts("ETA").array() * (mc11 - 2 * mc44).array();
  mc23 = Element::ParAtIntPts("ETA").array() * (mc11 - 2 * mc44).array();

}

template <typename Element>
std::vector<std::string> Elastic3D<Element>::PullElementalFields() const { return {"ux", "uy", "uz"}; }

template <typename Element>
std::vector<std::string> Elastic3D<Element>::PushElementalFields() const { return {"ax", "ay", "az"}; }

template <typename Element>
MatrixXd Elastic3D<Element>::assembleElementMassMatrix() {

  return Element::applyTestAndIntegrate(Element::ParAtIntPts("RHO"));

}

template <typename Element>
MatrixXd Elastic3D<Element>::computeStiffnessTerm(const Eigen::MatrixXd &u) {

  MatrixXd grad_ux = Element::computeGradient(u.col(0));
  MatrixXd grad_uy = Element::computeGradient(u.col(1));
  MatrixXd grad_uz = Element::computeGradient(u.col(2));

  ArrayXd strain(Element::NumIntPnt(),6);
  strain.col(0) = grad_ux.col(0);
  strain.col(1) = grad_uy.col(1);
  strain.col(2) = grad_uz.col(2);
  strain.col(3) = grad_uy.col(2) + grad_uz.col(1);
  strain.col(4) = grad_ux.col(2) + grad_uz.col(0);
  strain.col(5) = grad_ux.col(1) + grad_uy.col(0);

  /*    0,    1,    2,    3,    4,    5 */
  /* s_xx, s_yy, s_zz, s_yz, s_xz, s_xy */
  Array<double,Dynamic,6> stress = computeStress(strain);
  Matrix<double,Dynamic,3> stress_col(Element::NumIntPnt(), 3), stiff(Element::NumIntPnt(), 3);

  // sigma_x* -> ux
  stress_col.col(0) = stress.col(0); stress_col.col(1) = stress.col(5); stress_col.col(2) = stress.col(4);
  stiff.col(0) = Element::applyGradTestAndIntegrate(stress_col);

  // sigma_y* -> uy
  stress_col.col(0) = stress.col(5); stress_col.col(1) = stress.col(1); stress_col.col(2) = stress.col(3);
  stiff.col(1) = Element::applyGradTestAndIntegrate(stress_col);

  // sigma_z* -> uz
  stress_col.col(0) = stress.col(4); stress_col.col(1) = stress.col(3); stress_col.col(2) = stress.col(2);
  stiff.col(2) = Element::applyGradTestAndIntegrate(stress_col);

  return stiff;

}

template <typename Element>
Array<double,Dynamic,6> Elastic3D<Element>::computeStress(const Eigen::Ref<const Eigen::ArrayXd> &strain) {

  Matrix<double,Dynamic,6> stress(Element::NumIntPnt(),6);
  stress.col(0) = mc11 * strain.col(0) + mc12 * strain.col(1) + mc13 * strain.col(2);
  stress.col(1) = mc12 * strain.col(0) + mc22 * strain.col(1) + mc23 * strain.col(2);
  stress.col(2) = mc13 * strain.col(0) + mc23 * strain.col(1) + mc33 * strain.col(2);
  stress.col(3) = mc44 * strain.col(3);
  stress.col(4) = mc55 * strain.col(4);
  stress.col(5) = mc66 * strain.col(5);

  return stress;

}

template <typename Element>
MatrixXd Elastic3D<Element>::computeSurfaceIntegral(const Eigen::Ref<const Eigen::MatrixXd> &u) {
  return MatrixXd::Zero(Element::NumIntPnt(), Element::NumDim());
}

template <typename Element>
MatrixXd Elastic3D<Element>::computeSourceTerm(const double time, const PetscInt time_idx) {
  MatrixXd s = MatrixXd::Zero(Element::NumIntPnt(), Element::NumDim());
  for (auto &source : Element::Sources()) {
    RealVec3 pnt(source->LocR(), source->LocS(), source->LocT());
    s += (Element::getDeltaFunctionCoefficients(pnt) * source->fire(time, time_idx).transpose() );
  }
  return s;
}

#include <Element/HyperCube/Hexahedra.h>
#include <Element/HyperCube/HexP1.h>
template class Elastic3D<Hexahedra<HexP1>>;

