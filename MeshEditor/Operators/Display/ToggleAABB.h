#pragma once

#include "ToggleMeshFlagOperator.h"

using ToggleAABBOperator =
    ToggleMeshFlagOperator<&Mesh::getRenderMeshAABB, &Mesh::setRenderMeshAABB>;
