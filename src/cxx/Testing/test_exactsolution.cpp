#include "catch.h"
#include <Eigen/Dense>
#include <petsc.h>
#include <chrono>

//#include <Element/Element.h>
#include <Element/ElementNew.h>
#include <ElementAdapter.h>
#include <Element/HyperCube/Quad/Acoustic.h>
#include <Element/Simplex/Triangle/AcousticTri.h>
#include <Element/HyperCube/Hex/AcousticHex.h>

template <typename ElementVersion>
std::vector<std::shared_ptr<ElementVersion>> initialize_exact(Mesh *mesh,
                                                       ExodusModel *model,
                                                       std::shared_ptr<ElementVersion> reference_element,
                                                       Options options) {

  // Setup the dofs on each mesh point.
  mesh->setupGlobalDof(reference_element->NumDofVtx(),
                       reference_element->NumDofEdg(),
                       reference_element->NumDofFac(),
                       reference_element->NumDofVol(),
                       reference_element->NumDim(),
                       model);

  // Setup boundary conditions from options.
  mesh->setupBoundaries(options);

  // Register all global field required for time stepping.
  for (auto field : mesh->GlobalFields()) {
    mesh->registerFieldVectors(field);
  }

  std::vector<std::shared_ptr<ElementVersion>> elements;
  // Get a list of all local elements.
  for (int i = 0; i < mesh->NumberElementsLocal(); i++) {
    elements.push_back(reference_element->clone());
  }

  // Set up elements.
  int element_number = 0;
  for (auto &element : elements) {

    // Give each element a number starting from zero.
    element->SetNum(element_number++);

    // Get vertex coordinates from the PETSc DMPLEX.
    element->attachVertexCoordinates(mesh->DistributedMesh());

    // Add material parameters (velocity, Cij, etc...).
    element->attachMaterialProperties(model);

    // Set boundary conditions.
    element->setBoundaryConditions(mesh);

    // Assemble the (elemental) mass matrix.
    element->assembleElementMassMatrix(mesh);

    // Prepare stiffness terms
    element->prepareStiffness();

    // setup tests
    element->setupTest(mesh, options);

  }

  // Scatter the mass matrix to the global dofs.
  mesh->assembleLocalFieldToGlobal("m");

  // set initial condition on global vectors
  for (auto &field : reference_element->PullElementalFields()) {
    mesh->setLocalFieldToGlobal(field);
  }

  return elements;

}

template <typename ElementVersion>
double solve_vs_exact(Options options, Mesh *mesh, std::vector<std::shared_ptr<ElementVersion>> &elements) {
  PetscFunctionBegin;
  // Setup values.
  int it = 0;
  double time = 0.0;
  double timeStep = options.TimeStep();
  double duration = options.Duration();

  if (options.SaveMovie()) mesh->setUpMovie(options.OutputMovieFile());

  // Over-allocate matrices to avoid re-allocations.
  int max_dims = 3;
  int int_pnts = elements[0]->NumIntPnt();
  Eigen::MatrixXd f(int_pnts, max_dims);
  Eigen::MatrixXd u(int_pnts, max_dims);
  Eigen::MatrixXd ku(int_pnts, max_dims);
  Eigen::MatrixXd fMinusKu(int_pnts, max_dims);
  Eigen::MatrixXd fMinusKu_boundary(int_pnts, max_dims);

  double max_error = 0.0;
  double max_error_all = 0.0;
  
  while (time < duration) {
    max_error = 0.0;
    // Collect all global fields to the local partitions.
    for (auto &field : elements[0]->PullElementalFields()) {
      mesh->checkOutField(field);
    }

    // Zero all fields to which we will sum.
    for (auto &field : elements[0]->PushElementalFields()) {
      mesh->zeroFields(field);
    }
    std::chrono::duration<double, std::micro> totaltime_Ku(0.0);
    for (auto &element : elements) {

      // Get fields on element, store in successive rows. (e.g.,
      // "u" for acoustic, "ux, uz" for elastic)
      int fitr = 0;
      for (auto &field : element->PullElementalFields()) {
        u.col(fitr) = mesh->getFieldOnElement(field, element->Num(),
                                              element->ClsMap());
        fitr++;
      }
      
      double element_error = element->checkTest(mesh, options, u.block(0, 0, int_pnts, fitr), time+timeStep/2);

      max_error = fmax(max_error,element_error);
      max_error_all = fmax(max_error,max_error_all);

      auto start = std::chrono::high_resolution_clock::now();
      // Compute stiffness, only passing those rows which are occupied.
      ku.block(0, 0, int_pnts, fitr) = element->computeStiffnessTerm(
          u.block(0, 0, int_pnts, fitr));
      auto elapsed = std::chrono::high_resolution_clock::now() - start;
      totaltime_Ku += elapsed;
      
      
      // Compute acceleration.
      fMinusKu.block(0, 0, int_pnts, fitr) = -1 * ku.block(0, 0, int_pnts, fitr).array();

      // Sum fields into local partition.
      fitr = 0;
      for (auto &field : element->PushElementalFields()) {
        mesh->addFieldFromElement(field, element->Num(),
                                  element->ClsMap(),
                                  fMinusKu.col(fitr));
        fitr++;
      }
    }

    if(options.DisplayDiagnostics() && (it % options.DisplayDiagnosticsEvery() == 0 || it == 0)) {
      printf("max_error=%f @ time=%f (%f%%)\n",max_error,time,100*(time/duration));
      printf("Time per Ku on element = %f us\n", totaltime_Ku.count()/(elements.size()));
    }
    // exit(1);
    if (max_error > 5) {
      std::cerr << "ERROR: Solution blowing up!\n";
      exit(1);
    }

    // boundary condition happens after full Ku
    for (auto &element : elements) {
      // we can now apply boundary conditions (after fields are set)
      if (element->BndElm()) {
        // apply boundary condition
        element->applyDirichletBoundaries(mesh,
                                         options,
                                         "a");
      }
    }


    // Sum fields into global partitions.
    for (auto &field : elements[0]->PushElementalFields()) {
      mesh->checkInFieldBegin(field);
      mesh->checkInFieldEnd(field);
    }

    // Take a time step.
    mesh->applyInverseMassMatrix();
    mesh->advanceField(timeStep);

    if (options.SaveMovie() && (it % options.SaveFrameEvery() == 0 || it == 0)) {
      // GlobalFields[0] == "u" for acoustic and == "ux" for elastic
      mesh->saveFrame("a", it);
      // mesh->setLocalFieldToGlobal("u_exact");
      // mesh->saveFrame("u_exact", it);      
      PRINT_ROOT() << "TIME: " << time;
    }

    it++;
    time += timeStep;

  }

  PRINT_ROOT() << "Max Error @ T=end: " << max_error << std::endl;
  PRINT_ROOT() << "Max Error T=1:end: " << max_error_all << std::endl;

  if (options.SaveMovie()) mesh->finalizeMovie();
  // cumulative max error
  return max_error_all;
}

TEST_CASE("Testing acoustic exact solutions for triangles", "[exact/triangles]") {

 // Mock command line arguments.
 PetscOptionsClear();
 const char *arg[] = {
     "salvus_test",
     "--testing","true",
     "--duration", "0.7071067811865475",
     "--time_step", "0.003",
     "--exodus_file_name", "simple_trimesh_2x2.e",
     "--exodus_model_file_name", "simple_trimesh_2x2.e",
     "--mesh_type", "newmark",
     "--element_shape", "triangle_new",
     "--physics_system", "acoustic",
     "--polynomial_order", "3",
     "--dirichlet-boundaries", "dirichlet",
     "--testIC", "true",
     "--IC-center-x", "0.0",
     "--IC-center-z", "0.0",
     "--IC-square-side-L", "2",
     "--saveMovie","false",
     "--saveFrameEvery","1",
     "--output_movie_file_name","/scratch/salvus/output_files/movie.h5",
     NULL};
 char **argv = const_cast<char **> (arg);
 int argc = sizeof(arg) / sizeof(const char *) - 1;
 PetscOptionsInsert(&argc, &argv, NULL);

 // Set options for exact tests
 Options options;
 options.setOptions();

 // Triangles

 // Get mesh.
 Mesh *mesh = Mesh::factory(options);
 mesh->read(options);

 // Get model.
 ExodusModel *model = new ExodusModel(options);
 model->initializeParallel();

 // Setup reference element.
 std::shared_ptr<ElementNew> reference_element = ElementNew::Factory(options);

 std::vector<std::shared_ptr<ElementNew>> elements = initialize_exact<ElementNew>(
     mesh, model, reference_element, options);

 double error = solve_vs_exact<ElementNew>(options, mesh, elements);

 // allow 10% increase previously found in error, or fail.
 REQUIRE(error < (1.1*0.000183694));

}

TEST_CASE("Testing acoustic exact solutions for quadrilaterals", "[exact/quads]") {

  std::cout << "Testing exact solution quads!\n";
  // Set options for exact tests
  PetscOptionsClear();
  const char *arg[] = {
      "salvus_test",
      "--testing","true",
      "--duration", "0.7071067811865475",
      "--time_step", "0.003",
      "--exodus_file_name", "simple_quadmesh_2x2.e",
      "--exodus_model_file_name", "simple_quadmesh_2x2.e",
      "--mesh_type", "newmark",
      "--element_shape", "quad_new",
      "--physics_system", "acoustic",
      "--polynomial_order", "3",
      "--dirichlet-boundaries", "x0,x1,y0,y1",
      "--testIC", "true",
      "--IC-center-x", "0.0",
      "--IC-center-z", "0.0",
      "--IC-square-side-L", "2",
      "--saveMovie","false",
      "--saveFrameEvery","1",
      "--output_movie_file_name","./test.h5",
      NULL};
  char **argv = const_cast<char **> (arg);
  int argc = sizeof(arg) / sizeof(const char *) - 1;
  PetscOptionsInsert(&argc, &argv, NULL);

  Options options;
  options.setOptions();

  // Get mesh.
  Mesh *mesh = Mesh::factory(options);
  mesh->read(options);

  // Get model.
  ExodusModel *model = new ExodusModel(options);
  model->initializeParallel();

  // Setup reference element.
  std::shared_ptr<ElementNew> reference_element = ElementNew::Factory(options);


  std::vector<std::shared_ptr<ElementNew>> elements = initialize_exact<ElementNew>(mesh, model, reference_element, options);
  double error = solve_vs_exact<ElementNew>(options, mesh, elements);
  // allow 10% increase previously found in error, or fail.
  REQUIRE(error < (1.1*0.000180304));

}

TEST_CASE("Testing acoustic exact solutions for hexahedra", "[exact/hexahedra]") {

 std::cout << "Testing exact acoustic hex solution.\n";
 PetscOptionsClear();
 const char *arg[] = {
   "salvus_test",
   "--testing","true",
   "--duration", "0.08838834764831843", // 30 steps
   "--time_step", "0.003",
   "--exodus_file_name", "simple_hexmesh_2x2x2.vp4.e",
   "--exodus_model_file_name", "simple_hexmesh_2x2x2.vp4.e",
   "--mesh_type", "newmark",
   "--element_shape", "hex_new",
   "--physics_system", "acoustic",
   "--polynomial_order", "3",
   "--dirichlet-boundaries", "x0,x1,y0,y1,z0,z1",
   "--testIC", "true",
   "--IC-center-x", "0.0",
   "--IC-center-z", "0.0",
   "--IC-square-side-L", "2",
   "--saveMovie","false",
   "--saveFrameEvery","1",
   "--output_movie_file_name","/scratch/salvus/output_files/movie.h5",
   // "--displayDiagnosticsEvery","1",
   // options.__SetSaveMovie(PETSC_FALSE);
   // options.__SetSaveFrameEvery(1);
   NULL};
 char **argv = const_cast<char **> (arg);
 int argc = sizeof(arg) / sizeof(const char *) - 1;
 PetscOptionsInsert(&argc, &argv, NULL);

 // Set options for exact tests
 Options options;
 options.setOptions();

 // Get mesh.
 Mesh *mesh = Mesh::factory(options);
 mesh->read(options);

 // Get model.
 ExodusModel *model = new ExodusModel(options);
 model->initializeParallel();

 // Setup reference element.
 auto reference_element = ElementNew::Factory(options);

 auto elements = initialize_exact<ElementNew>(mesh, model, reference_element, options);

 double error = solve_vs_exact<ElementNew>(options, mesh,elements);

 // allow 10% increase in previously found error, or fail.
 REQUIRE(error < (1.1*0.000133237));

}


TEST_CASE("Testing acoustic exact solutions for new tetrahedra", "[exact/tetrahedra]") {

 std::cout << "Testing exact acoustic tet solution.\n";
 PetscOptionsClear();
 const char *arg[] = {
   "salvus_test",
   "--testing","true",
   "--duration", "0.08838834764831843", // 98 steps
   // "--duration", "0.004510548978043952", // 5 steps
   // "--duration", "0.0009021097956087903", // 1 step
   // dt=0.0036084391824351613 is stable for V=1
   // dt/4 = 0.0009021097956087903
   "--time_step", "0.0009021097956087903",
   // "--time_step", "0.0001",
   "--exodus_file_name", "simple_tetmesh_2x2x2.vp4.fluid.e",
   "--exodus_model_file_name", "simple_tetmesh_2x2x2.vp4.fluid.e",
   // "--exodus_file_name", "simple_tetmesh_2x2x2_30elements.vp4.fluid.e",
   // "--exodus_model_file_name", "simple_tetmesh_2x2x2_30elements.vp4.fluid.e",
   "--mesh_type", "newmark",
   "--element_shape", "tet_new",
   "--physics_system", "acoustic",
   "--polynomial_order", "3",
   "--dirichlet-boundaries", "x0,x1,y0,y1,z0,z1",
   "--testIC", "true",
   "--IC-center-x", "0.0",
   "--IC-center-y", "0.0",
   "--IC-center-z", "0.0",
   "--IC-square-side-L", "2",
   "--saveMovie","false",
   "--saveFrameEvery","1",
   "--output_movie_file_name","/scratch/salvus/output_files_new/movie.h5",
   "--displayDiagnostics", "true",
   "--displayDiagnosticsEvery", "10",
   NULL};
 char **argv = const_cast<char **> (arg);
 int argc = sizeof(arg) / sizeof(const char *) - 1;
 PetscOptionsInsert(&argc, &argv, NULL);

 // Set options for exact tests
 Options options;
 options.setOptions();

 // Get mesh.
 Mesh *mesh = Mesh::factory(options);
 mesh->read(options);

 // Get model.
 ExodusModel *model = new ExodusModel(options);
 model->initializeParallel();

 // Setup reference element.
 auto reference_element = ElementNew::Factory(options);

 auto elements = initialize_exact<ElementNew>(mesh, model, reference_element, options);

 double error = solve_vs_exact<ElementNew>(options, mesh,elements);

 // allow 10% increase in previously found error, or fail.
 REQUIRE(error < (1.1*0.000304241));

}

