#include "catch.h"

#include <iostream>

#include <petsc.h>
#include <Mesh/Mesh.h>
#include <Element/Element.h>
#include <Model/ExodusModel.h>
#include <Utilities/Options.h>

using namespace std;

TEST_CASE("test_coupling", "[coupling]") {

  std::string e_file = "../../salvus_data/unit_test_meshes/fluid_layer_over_elastic_cartesian_2D_50s.e";

  // Set up custom command line arguments.
  PetscOptionsClear();
  const char *arg[] = {
      "salvus_test",
      "--testing", "true",
      "--exodus_file_name", e_file.c_str(),
      "--exodus_model_file_name", e_file.c_str(),
      "--mesh_type", "newmark",
      "--element_shape", "quad_new",
      "--polynomial_order", "4", NULL
  };

  char **argv = const_cast<char **> (arg);
  int argc = sizeof(arg) / sizeof(const char *) - 1;
  PetscOptionsInsert(&argc, &argv, NULL);

  Options options;
  options.setOptions();

  ExodusModel *model = new ExodusModel(options);
  model->initializeParallel();

  Mesh *mesh = Mesh::factory(options);
  mesh->read(options);
  mesh->setupGlobalDof(1, 3, 9, 0, 2, model);


  std::vector<std::shared_ptr<Element>> elements;
  for (PetscInt i = 0; i < mesh->NumberElementsLocal(); i++) {
    elements.emplace_back(Element::Factory(mesh->ElementFields(i),
                                           mesh->TotalCouplingFields(i),
                                           options));
  }

  PetscInt elmnum=0;
  for (auto &e: elements) {
    e->SetNum(elmnum++);
    e->attachVertexCoordinates(mesh);
  }

  Eigen::MatrixXd t = Eigen::MatrixXd::Constant(25, 3, 1.0);
  for (auto &e: elements) {
    e->setBoundaryConditions(mesh);
    std::cout << e->computeSurfaceIntegral(t).sum() << std::endl;
  }

}