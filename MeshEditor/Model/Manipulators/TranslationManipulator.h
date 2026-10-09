#pragma once

#include "Manipulator.h"
#include <glm/glm.hpp>

class TranslationManipulator : public Manipulator
{
public:
    TranslationManipulator(glm::vec3 dir_L, float length = 1.5f,
                           float visualScale = 1.0f);
    void handleMovement(MovementType movementType, const Viewport &viewport,
                        double x, double y) override;
    void setDirection(glm::vec3 inDir_L, float length = 1.5f,
                      float visualScale = 1.0f);

private:
    glm::vec3 startPoint_L{0.0f, 0.0f, 0.0f};
    glm::vec3 dir_L{1.0f, 0.0f, 0.0f};
    glm::mat4 cachedAbsTrf{1.0f};
    glm::mat4 cachedInvAbsTrf{1.0f};
    glm::vec3 cachedDir_W{1.0f, 0.0f, 0.0f};
};