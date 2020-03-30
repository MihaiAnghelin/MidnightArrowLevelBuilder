#pragma once
#include "mapData.h"
#include "glm/vec2.hpp"
#include <vector>

void simulateLight(glm::vec2 pos, MapData &mapData, std::vector<glm::vec2> &triangles);