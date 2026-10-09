#pragma once

#include <memory>
#include <string>

#include "Model.h"

std::unique_ptr<Model> loadSceneModel(const std::string &filename);

// Prebuilds all buffers and octrees for a model (called automatically inside loadSceneModel).
void warmupModel(Model &model);
