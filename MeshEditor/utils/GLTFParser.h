#pragma once

#include <memory>
#include <string>

#include "Model.h"

class GLTFParser
{
public:
	static std::unique_ptr<Model> read(const std::string &filename);
};
