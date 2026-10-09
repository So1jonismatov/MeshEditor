#pragma once

#include "Manipulator.h"
#include <glm/glm.hpp>

class RotationManipulator : public Manipulator
{
public:
    RotationManipulator(glm::vec3 dir_L, float majorRadius = 1.0f,
                        float minorRadius = 0.05f);
    void handleMovement(MovementType movementType, const Viewport &viewport,
                        double x, double y) override;
    void setDirection(glm::vec3 inDir_L, float majorRadius = 1.0f,
                      float minorRadius = 0.05f);

private:
    glm::vec3 dir_L{0.0f, 1.0f, 0.0f};
    glm::vec3 dragOrigin_W{0.0f, 0.0f, 0.0f};
    glm::vec3 dragAxis_W{0.0f, 1.0f, 0.0f};
    glm::vec3 dragBasisU_W{1.0f, 0.0f, 0.0f};
    glm::vec3 dragBasisV_W{0.0f, 0.0f, 1.0f};
    float dragAngle = 0.0f;
    bool dragging = false;
};