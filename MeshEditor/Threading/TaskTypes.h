#pragma once

// Common type aliases for async task results.
// Include this header wherever you schedule typed tasks.

#include <memory>
#include <string>

class Model;

struct LoadModelResult
{
    std::unique_ptr<Model> model;
    std::string filename;
};
