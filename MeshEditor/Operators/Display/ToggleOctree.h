#pragma once

#include "ToggleMeshFlagOperator.h"

using ToggleOctreeOperator =
    ToggleMeshFlagOperator<&Mesh::getRenderOctreeBB, &Mesh::setRenderOctreeBB>;
