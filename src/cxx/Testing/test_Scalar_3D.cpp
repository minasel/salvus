#include <iostream>
#include <Utilities/Types.h>
#include <Mesh/Mesh.h>
#include <Model/ExodusModel.h>
#include <Utilities/Options.h>
#include <Physics/Scalar.h>
#include <Element/ElementAdapter.h>
#include <Problem/Problem.h>
#include <petscviewerhdf5.h>
#include <Element/HyperCube/Hexahedra.h>
#include <Element/HyperCube/HexP1.h>
#include <Element/Simplex/Tetrahedra.h>
#include <Element/Simplex/TetP1.h>
#include "catch.h"
#include <Utilities/Logging.h>

using namespace std;

template <typename Element>
class TestPlugin: public Element {

 public:
  TestPlugin<Element>(std::unique_ptr<Options> const &options): Element(options) {};

  void setupEigenfunctionTest(std::unique_ptr<Mesh> const &mesh,
                              std::unique_ptr<Options> const &options,
                              std::unique_ptr<Problem> &problem,
                              FieldDict &fields) {

    /* This is hardcoded for the unit test mesh. */
    PetscScalar x0 = 5e4, y0 = 5e4, z0 = 5e4, L = 1e5;
    RealVec pts_x, pts_y, pts_z;
    std::tie(pts_x, pts_y, pts_z) = Element::buildNodalPoints();
    RealVec un =
        (M_PI / L * (pts_x.array() - (x0 + L / 2))).sin() *
        (M_PI/ L * (pts_y.array() - (y0 + L / 2))).sin() *
        (M_PI / L * (pts_z.array() - (z0 + L / 2))).sin();
    RealVec vn = RealVec::Zero(pts_x.size());
    RealVec an = RealVec::Zero(pts_x.size());

    problem->insertElementalFieldIntoMesh("u", Element::ElmNum(), Element::ClsMap(), un,
                                          mesh->DistributedMesh(), mesh->MeshSection(),
                                          fields);
    problem->insertElementalFieldIntoMesh("v", Element::ElmNum(), Element::ClsMap(), vn,
                                          mesh->DistributedMesh(), mesh->MeshSection(),
                                          fields);
    problem->insertElementalFieldIntoMesh("a", Element::ElmNum(), Element::ClsMap(), an,
                                          mesh->DistributedMesh(), mesh->MeshSection(),
                                          fields);

  }

  PetscReal checkEigenfunctionTestNew(std::unique_ptr<Mesh> const &mesh,
                                      std::unique_ptr<Options> const &options,
                                      const PetscScalar time,
                                      std::unique_ptr<Problem> &problem,
                                      FieldDict &fields) {

    PetscScalar x0 = 5e4, y0 = 5e4, z0 = 5e4, L = 1e5;
    RealVec pts_x, pts_y, pts_z;
    std::tie(pts_x, pts_y, pts_z) = Element::buildNodalPoints();
    RealVec un_xyz =
        (M_PI / L * (pts_x.array() - (x0 + L / 2))).sin() *
        (M_PI / L * (pts_y.array() - (y0 + L / 2))).sin() *
        (M_PI / L * (pts_z.array() - (z0 + L / 2))).sin();
    PetscScalar vp = Element::ParAtIntPts("VP").mean();
    PetscScalar un_t = cos(M_PI / L * sqrt(3) * time * vp);
    RealVec exact = un_t * un_xyz;

    RealVec u = problem->getFieldOnElement(
        "u", Element::ElmNum(), Element::ClsMap(),
        mesh->DistributedMesh(), mesh->MeshSection(), fields);

    PetscScalar element_error = (exact - u).array().abs().maxCoeff();
    return element_error;

  }
};

typedef ElementAdapter<TestPlugin<Scalar<Hexahedra<HexP1>>>> test_insert_hex;
typedef TestPlugin<Scalar<Hexahedra<HexP1>>> test_init_hex;
typedef ElementAdapter<Scalar<Hexahedra<HexP1>>> unguard_hex;
typedef Scalar<Hexahedra<HexP1>> raw_hex;

typedef ElementAdapter<TestPlugin<Scalar<Tetrahedra<TetP1>>>> test_insert_tet;
typedef TestPlugin<Scalar<Tetrahedra<TetP1>>> test_init_tet;
typedef ElementAdapter<Scalar<Tetrahedra<TetP1>>> unguard_tet;
typedef Scalar<Tetrahedra<TetP1>> raw_tet;

PetscReal runEigenFunctionTest3D(std::vector<std::unique_ptr<Element>> test_elements,
                               std::unique_ptr<Mesh> &mesh,
                               std::unique_ptr<ExodusModel> const &model,
                               std::unique_ptr<Options> const &options,
                               std::unique_ptr<Problem> &problem,
                               FieldDict &fields,
                               PetscReal cycle_time,
                               ElementType element_type
                               ) {

  RealVec element_error(test_elements.size()); 
  PetscScalar time = 0;
  PetscInt time_idx = 0;
  PetscReal max_error = 0.0;
  while (true) {

    std::tie(test_elements, fields) =
      problem->assembleIntoGlobalDof(std::move(test_elements),
                                     std::move(fields),
                                     time, time_idx,
                                     mesh->DistributedMesh(),
                                     mesh->MeshSection(),
                                     options);

    fields = problem->applyInverseMassMatrix(std::move(fields));
    std::tie(fields, time) = problem->takeTimeStep
        (std::move(fields), time, options);

    time_idx++;

    PetscInt i = 0;
    for (auto &elm: test_elements) {
      switch(element_type) {
      case ElementType::HEXP1:
        {
          auto validate = static_cast<test_init_hex*>(static_cast<test_insert_hex*>(elm.get()));
          element_error(i++) = validate->checkEigenfunctionTestNew(mesh, options, time, problem, fields);
          break;
        }
      case ElementType::TETP1:
        {
          auto validate = static_cast<test_init_tet*>(static_cast<test_insert_tet*>(elm.get()));
          element_error(i++) = validate->checkEigenfunctionTestNew(mesh, options, time, problem, fields);
          break;
        }
      default:
        ERROR() << "Element type not supported";
      }
    }

    // only saves if '--saveMovie' is set in command line options
    problem->saveSolution(time, {"u","a"}, fields, mesh->DistributedMesh());
    
    max_error = element_error.maxCoeff() > max_error ? element_error.maxCoeff() : max_error;
    VERBOSE() << "t=" << time << " error=" << max_error;
    if (time > cycle_time) break;
    
  }
  return max_error;  
}


TEST_CASE("Test analytic eigenfunction solution for scalar "
              "equation for hexes in 3d", "[hex_eigenfunction]") {

  std::string e_file = "hex_eigenfunction.e";
  char order_str[5];
  for (int poly_order = 3; poly_order < 6; poly_order++) {
    sprintf(order_str,"%d",poly_order);
    PetscOptionsClear(NULL);
    const char *arg[] = {
      "salvus_test",
      "--testing", "true",
      "--mesh-file", e_file.c_str(),
      "--model-file", e_file.c_str(),
      "--time-step", "1e-2",
      "--polynomial-order", order_str,
      "--homogeneous-dirichlet", "x0,x1,y0,y1,z0,z1",
      NULL};
    char **argv = const_cast<char **> (arg);
    int argc = sizeof(arg) / sizeof(const char *) - 1;
    PetscOptionsInsert(NULL, &argc, &argv, NULL);

    std::unique_ptr<Options> options(new Options);
    options->setOptions();

    std::unique_ptr<Problem> problem(Problem::Factory(options));
    std::unique_ptr<ExodusModel> model(new ExodusModel(options));
    std::unique_ptr<Mesh> mesh(Mesh::Factory(options));

    model->read();
    mesh->read();

    /* Setup topology from model and mesh. */
    mesh->setupTopology(model, options);

    /* Setup elements from model and topology. */
    auto elements = problem->initializeElements(mesh, model, options);

    /* Setup global degrees of freedom based on element 0. */
    mesh->setupGlobalDof(elements[0], options);

    std::vector<std::unique_ptr<Element>> test_elements;
    auto fields = problem->initializeGlobalDofs(elements, mesh);
    int it = 0;
    /* Rip apart elements and insert testing mixin. */
    for (auto &e: elements) {

      /* Rip out the master Element class. */
      auto l1 = static_cast<unguard_hex*>(e.release());

      /* Rip out the Element adapter. */
      auto l2 = static_cast<raw_hex*>(l1);

      /* Attach the tester. */
      auto l3 = static_cast<test_init_hex*>(l2);

      l3->setupEigenfunctionTest(mesh, options, problem, fields);

      /* Now we have a class with testing, which is still really an element :) */
      test_elements.emplace_back(static_cast<test_insert_hex*>(l3));
    }

    PetscReal cycle_time = 1.0;
    auto max_error = runEigenFunctionTest3D(std::move(test_elements),mesh,model,options,problem,fields,cycle_time, ElementType::HEXP1);
    
    VERBOSE() << "hex order " << options->PolynomialOrder() << " error: " << max_error;
    PetscScalar eps = 0.01;
    if(options->PolynomialOrder() == 2) {    
      PetscReal regression_error = 0.000636005;
      REQUIRE(max_error <= regression_error * (1 + eps));
    } else if (options->PolynomialOrder() == 3) {
      PetscReal regression_error = 0.00048205;
      REQUIRE(max_error <= regression_error * (1 + eps));
    } else if (options->PolynomialOrder() == 4) {
      PetscReal regression_error = 0.000489815;
      REQUIRE(max_error <= regression_error * (1 + eps));
    } else if (options->PolynomialOrder() == 5) {
      PetscReal regression_error = 0.000486752;
      REQUIRE(max_error <= regression_error * (1 + eps));
    } else {
      PetscReal regression_error = 0.000486752;
      REQUIRE(max_error <= regression_error * (1 + eps));
    }
  }
}

TEST_CASE("Test analytic eigenfunction solution for scalar "
              "equation for tets in 3d", "[tet_eigenfunction]") {

  std::string e_file = "tet_eigenfunction.e";
  char order_str[5];
  int poly_order = 3;
  sprintf(order_str,"%d",poly_order);
  PetscOptionsClear(NULL);
  const char *arg[] = {
    "salvus_test",
    "--testing", "true",
    "--mesh-file", e_file.c_str(),
    "--model-file", e_file.c_str(),
    "--time-step", "1e-2",
    "--polynomial-order", order_str,
    "--save-movie", "false",
    "--movie-file-name","movie_tet.h5",
    "--homogeneous-dirichlet", "x0,x1,y0,y1,z0,z1",
    NULL};
  char **argv = const_cast<char **> (arg);
  int argc = sizeof(arg) / sizeof(const char *) - 1;
  PetscOptionsInsert(NULL, &argc, &argv, NULL);

  std::unique_ptr<Options> options(new Options);
  
  options->setOptions();

  std::unique_ptr<Problem> problem(Problem::Factory(options));
  std::unique_ptr<ExodusModel> model(new ExodusModel(options));
  std::unique_ptr<Mesh> mesh(Mesh::Factory(options));

  VERBOSE() << "Reading model";
  model->read();
  VERBOSE() << "Reading mesh";
  mesh->read();

  /* Setup topology from model and mesh. */
  VERBOSE() << "Setting topology";
  mesh->setupTopology(model, options);

  /* Setup elements from model and topology. */
  VERBOSE() << "Building elements";
  auto elements = problem->initializeElements(mesh, model, options);

  VERBOSE() << "Simulating on " << elements.size() << " tetrahedra";
  
  /* Setup global degrees of freedom based on element 0. */
  mesh->setupGlobalDof(elements[0], options);

  std::vector<std::unique_ptr<Element>> test_elements;
  auto fields = problem->initializeGlobalDofs(elements, mesh);
  
  VERBOSE() << "Setting up eigenfunction test";
  /* Rip apart elements and insert testing mixin. */
  for (auto &e: elements) {

    /* Rip out the master Element class. */
    auto l1 = static_cast<unguard_tet*>(e.release());

    /* Rip out the Element adapter. */
    auto l2 = static_cast<raw_tet*>(l1);

    /* Attach the tester. */
    auto l3 = static_cast<test_init_tet*>(l2);

    l3->setupEigenfunctionTest(mesh, options, problem, fields);
    
    /* Now we have a class with testing, which is still really an element :) */
    test_elements.emplace_back(static_cast<test_insert_tet*>(l3));    
  }

  PetscReal cycle_time = 1.0;

  VERBOSE() << "Running eigenfunction test";
  auto max_error = runEigenFunctionTest3D(std::move(test_elements),mesh,model,options,problem,fields,cycle_time, ElementType::TETP1);
  
  VERBOSE() << "max_error = " << max_error;
  PetscReal regression_error = 0.000544468;
  PetscScalar eps = 0.01;
  REQUIRE(max_error <= regression_error * (1 + eps));
}
