#pragma once

#include <glm/glm.hpp>

class View;
class Model;

void zoomViewToModel(View &view);
bool getSceneBoundingBox(const Model *model, glm::vec3 &outMin, glm::vec3 &outMax);
