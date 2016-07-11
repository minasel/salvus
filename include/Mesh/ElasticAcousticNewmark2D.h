#pragma once

#include <Mesh/Mesh.h>

// forward decl.

class ElasticAcousticNewmark2D: public Mesh {

 public:
  ~ElasticAcousticNewmark2D() {};
  virtual void advanceField(double dt);
  virtual void applyInverseMassMatrix();

  ElasticAcousticNewmark2D(const std::unique_ptr<Options> &options): Mesh(options) {
      mGlobalFields = {"ux", "uy", "u",
                       "vx", "vy", "v",
                       "ax", "ay", "a",
                       "ax_", "ay_", "a_",
                       "m"};}

};

