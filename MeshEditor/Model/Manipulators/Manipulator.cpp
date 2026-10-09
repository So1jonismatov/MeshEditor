#include "Manipulator.h"
#include <cmath>

void Manipulator::handleMovement(MovementType movementType,
                                 const Viewport &viewport, double x, double y)
{
    (void)movementType;
    (void)viewport;
    (void)x;
    (void)y;
}

void Manipulator::setCallback(
    const std::function<void(const glm::mat4 &)> &inCallback)
{
    callback = inCallback;
}

void Manipulator::sendFeedback(const glm::mat4 &deltaMatrix)
{

    auto root = dynamic_cast<Manipulator *>(getParent());

    if (root)// If parent is manipulator call the parent's callback
    {
        if (root->callback)
            root->callback(deltaMatrix);
    }
    else  // Else apply the relative transform
    {
        Node::applyRelativeTransform(deltaMatrix); // transform the MANIPULATOR
        if (callback)
            callback(deltaMatrix);
    }
}

bool Manipulator::projectCursorOntoAxis(const Viewport &viewport, double x,
                                        double y, const glm::mat4 &cachedAbsTrf,
                                        const glm::mat4 &cachedInvAbsTrf,
                                        const glm::vec3 &cachedDir_W,
                                        glm::vec3 &pointLocal)
{
    // Get the origin of the manipulator in world space
    const glm::vec3 origin_W =
        glm::vec3(cachedAbsTrf * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    // Get the cursor ray in world space
    const ray cursorRay_W = viewport.calcCursorRay(x, y);

    const glm::vec3 viewDir = viewport.getCamera().calcForward();
    glm::vec3 planeNormal =
        glm::cross(cachedDir_W, glm::cross(viewDir, cachedDir_W));
    if (glm::length(planeNormal) < 1e-6f)
        planeNormal = -viewDir;
    else
        planeNormal = glm::normalize(planeNormal);

    // Project the cursor ray onto the plane defined by the manipulator's axis
    // and the camera view direction
    const float denom = glm::dot(cursorRay_W.dir, planeNormal);
    if (std::abs(denom) < 1e-5f)
        return false;

    // Calculate the intersection point of the cursor ray with the plane
    const float t = glm::dot(origin_W - cursorRay_W.orig, planeNormal) / denom;
    if (!std::isfinite(t) || std::abs(t) > 1e6f)
        return false;

    // Calculate the point on the plane and then project it onto the
    // manipulator's axis
    const glm::vec3 ptOnPlane = cursorRay_W.orig + t * cursorRay_W.dir;
    const glm::vec3 ptOnRay =
        origin_W + cachedDir_W * glm::dot(ptOnPlane - origin_W, cachedDir_W);

    pointLocal = glm::vec3(cachedInvAbsTrf * glm::vec4(ptOnRay, 1.0f));
    return true;
}