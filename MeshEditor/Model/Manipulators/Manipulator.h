#pragma once

#include "ViewPort.h"
#include "Node.h"
#include <functional>
#include <glm/glm.hpp>

enum class MovementType
{
    Push,
    Drag,
    Release
};

class Manipulator : public Node
{
public:
    virtual void handleMovement(MovementType movementType,
                                const Viewport &viewport, double x, double y);

    void setCallback(const std::function<void(const glm::mat4 &)> &inCallback);

protected:
    void sendFeedback(const glm::mat4 &deltaMatrix);

    static bool projectCursorOntoAxis(const Viewport &viewport, double x,
                                      double y, const glm::mat4 &cachedAbsTrf,
                                      const glm::mat4 &cachedInvAbsTrf,
                                      const glm::vec3 &cachedDir_W,
                                      glm::vec3 &pointLocal);

private:
    std::function<void(const glm::mat4 &)> callback;
};