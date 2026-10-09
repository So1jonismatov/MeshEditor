#include "AABB.h"

glm::vec3 AABB::center() const
{
    return (min + max) * 0.5f;
}
glm::vec3 AABB::size() const
{
    return max - min;
}

void AABB::expand(const glm::vec3 &p)
{
    min = glm::min(min, p);
    max = glm::max(max, p);
}
void AABB::expand(const AABB &other)
{
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
}

std::array<glm::vec3, 8> AABB::corners() const
{
    return {glm::vec3{min.x, min.y, min.z}, glm::vec3{min.x, min.y, max.z},
            glm::vec3{min.x, max.y, min.z}, glm::vec3{min.x, max.y, max.z},
            glm::vec3{max.x, min.y, min.z}, glm::vec3{max.x, min.y, max.z},
            glm::vec3{max.x, max.y, min.z}, glm::vec3{max.x, max.y, max.z}};
}

AABB AABB::transformed(const glm::mat4 &trf) const
{
    glm::vec3 newMin = glm::vec3(trf[3]);
    glm::vec3 newMax = newMin;

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            float a = trf[j][i] * min[j];
            float b = trf[j][i] * max[j];
            if (a < b)
            {
                newMin[i] += a;
                newMax[i] += b;
            }
            else
            {
                newMin[i] += b;
                newMax[i] += a;
            }
        }
    }

    return {newMin, newMax};
}

bool AABB::intersectRay(const glm::vec3 &orig, const glm::vec3 &dir,
                        float &tOut) const
{
    float tMin = -std::numeric_limits<float>::max();
    float tMax = std::numeric_limits<float>::max();

    for (int i = 0; i < 3; ++i)
    {
        if (std::abs(dir[i]) < 1e-8f)
        {
            if (orig[i] < min[i] || orig[i] > max[i])
                return false;
        }
        else
        {
            const float invD = 1.0f / dir[i];
            float t0 = (min[i] - orig[i]) * invD;
            float t1 = (max[i] - orig[i]) * invD;
            if (invD < 0.0f)
                std::swap(t0, t1);
            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            if (tMin > tMax)
                return false;
        }
    }

    tOut = tMin;
    return tMax >= 0.0f;
}
