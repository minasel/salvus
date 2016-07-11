#include <Mesh/Mesh.h>
#include <Source/Source.h>
#include <Utilities/Options.h>
#include <Model/ExodusModel.h>
#include <Physics/AcousticTri.h>

using namespace Eigen;

template <typename Element>
AcousticTri<Element>::AcousticTri(std::unique_ptr<Options> const &options): Element(options) {

  // Allocate all work arrays.
  mVpSquared.setZero(Element::NumIntPnt());
  mStiff.setZero(Element::NumIntPnt());
  mSource.setZero(Element::NumIntPnt());
  mStress.setZero(Element::NumIntPnt(), 2);
  mStrain.setZero(Element::NumIntPnt(), 2);

}

template <typename Element>
void AcousticTri<Element>::attachMaterialProperties(std::unique_ptr<ExodusModel> const &model) {
  Element::attachMaterialProperties(model, "VP");
}

template <typename Element>
std::vector<std::string> AcousticTri<Element>::PullElementalFields() const { return { "u" }; }

template <typename Element>
std::vector<std::string> AcousticTri<Element>::PushElementalFields() const { return { "a" }; }

template <typename Element>
MatrixXd AcousticTri<Element>::assembleElementMassMatrix() {

  // In this acoustic formulation we just multiply shape functions together.
  return Element::applyTestAndIntegrate(VectorXd::Ones(Element::NumIntPnt()));

}

template <typename Element>
double AcousticTri<Element>::CFL_estimate() {
  double vpMax = Element::ParAtIntPts("VP").maxCoeff();
  return Element::CFL_constant() * Element::estimatedElementRadius() / vpMax;
}


template <typename Element>
MatrixXd AcousticTri<Element>::computeStress(const Ref<const MatrixXd> &strain) {

  // Interpolate the (square) of the velocity at each integration point.
  mVpSquared = Element::ParAtIntPts("VP").array().pow(2);

  // Calculate sigma_ux and sigma_uy.
  mStress.col(0) = mVpSquared.array().cwiseProduct(strain.col(0).array());
  mStress.col(1) = mVpSquared.array().cwiseProduct(strain.col(1).array());
  return mStress;

}

template <typename Element>
MatrixXd AcousticTri<Element>::computeSurfaceIntegral(const Eigen::Ref<const Eigen::MatrixXd> &u) {
  return Eigen::MatrixXd::Zero(Element::NumIntPnt(), 1);
}

template <typename Element>
void AcousticTri<Element>::prepareStiffness() {
  auto velocity = Element::ParAtIntPts("VP");
  mElementStiffnessMatrix = Element::buildStiffnessMatrix(velocity);    
}


template <typename Element>
MatrixXd AcousticTri<Element>::computeStiffnessTerm(
    const MatrixXd &u) {

  mStiff = mElementStiffnessMatrix*u.col(0);
  return mStiff;
  
  // // Calculate gradient from displacement.
  // mStrain = Element::computeGradient(u);

  // // Stress from strain.
  // mStress = computeStress(mStrain);

  // // Complete application of K->u.
  // mStiff = Element::applyGradTestAndIntegrate(mStress);

  // return mStiff;
  
}

template <typename Element>
MatrixXd AcousticTri<Element>::computeSourceTerm(const double time) {
  mSource.setZero();
  for (auto source : Element::Sources()) {
    mSource += (source->fire(time) * Element::getDeltaFunctionCoefficients(
        source->LocR(), source->LocS()));
  }
  return mSource;
}


template <typename Element>
void AcousticTri<Element>::setupEigenfunctionTest(std::unique_ptr<Mesh> const &mesh, std::unique_ptr<Options> const &options) {

  double L, Lx, Ly;
  double x0 = options->IC_Center_x();
  double y0 = options->IC_Center_z();
  L = Lx = Ly = options->IC_SquareSide_L();
  VectorXd pts_x, pts_y;
  std::tie(pts_x, pts_y) = Element::buildNodalPoints();
  VectorXd un = (M_PI/Lx*(pts_x.array()-(x0+L/2))).sin() * (M_PI/Ly*(pts_y.array()-(y0+L/2))).sin();
  VectorXd vn = VectorXd::Zero(pts_x.size());
  VectorXd an = VectorXd::Zero(pts_x.size());
  mesh->setFieldFromElement("u", Element::ElmNum(), Element::ClsMap(), un);
  mesh->setFieldFromElement("v", Element::ElmNum(), Element::ClsMap(), vn);
  mesh->setFieldFromElement("a_", Element::ElmNum(), Element::ClsMap(), an);

}

template <typename Element>
double AcousticTri<Element>::checkEigenfunctionTest(std::unique_ptr<Mesh> const &mesh, std::unique_ptr<Options> const &options,
                                                  const Ref<const MatrixXd>& u, double time) {

  double L, Lx, Ly;
  double x0 = options->IC_Center_x();
  double y0 = options->IC_Center_z();
  L = Lx = Ly = options->IC_SquareSide_L();
  VectorXd pts_x, pts_y;
  std::tie(pts_x,pts_y) = Element::buildNodalPoints();
  VectorXd un_xy = (M_PI/Lx*(pts_x.array()-(x0+L/2))).sin()*(M_PI/Ly*(pts_y.array()-(y0+L/2))).sin();
  double vp = Element::ParAtIntPts("VP").mean();
  double un_t = cos(M_PI/Lx*sqrt(2)*time*vp);
  VectorXd exact = un_t * un_xy;
  double element_error = (exact - u).array().abs().maxCoeff();
  if (!Element::ElmNum()) {
  }

  return element_error;

}


#include <Element/Simplex/Triangle.h>
#include <Element/Simplex/TriP1.h>
template class AcousticTri<Triangle<TriP1>>;
