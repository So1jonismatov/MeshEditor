#pragma once

#include <string>
#include <memory>
#include "Model.h"

std::unique_ptr<Model> loadModel(const std::string &filename);
void saveModel(const Model &model, const std::string &filename);