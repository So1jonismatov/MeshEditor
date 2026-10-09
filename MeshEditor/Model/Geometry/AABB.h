#pragma once
#include <glm/glm.hpp>
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

// Axis-aligned bounding box. Kept as a small value type with the handful of
// operations the editor actually needs, so call sites stop hand-rolling
// 8-corner loops and slab tests.
struct AABB
{
    glm::vec3 min{0.0f, 0.0f, 0.0f};
    glm::vec3 max{0.0f, 0.0f, 0.0f};

    glm::vec3 center() const;
    glm::vec3 size() const;

    // Grow the box to include a point / another box.
    void expand(const glm::vec3 &p);
    void expand(const AABB &other);

    std::array<glm::vec3, 8> corners() const;

    // Smallest axis-aligned box in world space that encloses this box after
    // applying `trf` (e.g. a node's absolute transform).
    AABB transformed(const glm::mat4 &trf) const;

    // Slab-method ray/box intersection. `tOut` receives the near hit distance.
    bool intersectRay(const glm::vec3 &orig, const glm::vec3 &dir,
                      float &tOut) const;
};

// Backwards-compatible alias: the codebase spells this type `bbox`.
using bbox = AABB;
