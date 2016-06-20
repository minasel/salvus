#pragma once

// stl
#include <ostream>
#include <iostream>
#include <iosfwd>
#include <string>
#include <memory>

// 3rd party.
#include <mpi.h>

#include <Element/Element.h>

class Mesh;
class Options;
class ExodusModel;

class Problem {

 public:

  virtual ~Problem() {};

  static Problem *factory(std::string solver_type);

  virtual void solve(Options options) = 0;
  virtual void initialize(std::unique_ptr<Mesh> mesh, std::unique_ptr<ExodusModel> const &model,
                          std::unique_ptr<Options> const &options) = 0;

};
