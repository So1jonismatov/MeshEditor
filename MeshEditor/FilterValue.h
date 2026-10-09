#pragma once
#include <cstdint>

enum class FilterValue : uint32_t
{
    Node = 1,
    Manipulator = 2,
    NM = (uint32_t)(Node | Manipulator),
};