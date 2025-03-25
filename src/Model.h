#pragma once

#include <SDL3/SDL.h>
#include <vector>

#include "Vertex.h"

class Model {
public:
  Model();
  std::vector<Vertex> vertices{};
  std::vector<Uint32> indices{};
};
