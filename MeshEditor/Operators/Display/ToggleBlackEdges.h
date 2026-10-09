#pragma once

#include "ToggleMeshFlagOperator.h"

using ToggleBlackEdgesOperator =
    ToggleMeshFlagOperator<&Mesh::getRenderBlackEdges,
                           &Mesh::setRenderBlackEdges>;
