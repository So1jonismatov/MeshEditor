#pragma once

#include <cstdint>
#include <vector>

#include <HalfEdge.h>
#include "../../Interfaces/IRenderSystem.h"

namespace Utils
{
std::vector<Vertex> buildCoordinateAxes();

HalfEdgeTable createCube();
std::vector<glm::mat4> createCubeWallTransforms(unsigned int rows,
                                                unsigned int columns,
                                                float spacing);
HalfEdgeTable createTessellatedCircle(unsigned int segments, float radius);

HalfEdgeTable meshCylinder(double R, double h, uint32_t numSubdivisions);
HalfEdgeTable meshCone(double R, double h, uint32_t numSubdivisions);
HalfEdgeTable meshTorus(double minorRadius, double majorRadius,
                        uint32_t majorSegments);
HalfEdgeTable meshArrow();
HalfEdgeTable meshScaleArrow();
} // namespace Utils