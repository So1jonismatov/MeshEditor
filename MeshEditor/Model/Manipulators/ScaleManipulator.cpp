#include "ScaleManipulator.h"
#include "utils/CreatePrimitives.h"
#include "utils/MathUtils.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

ScaleManipulator::ScaleManipulator(glm::vec3 inDir_L, float length)
{
    setManipulator(true);
    setDirection(inDir_L, length);
}

void ScaleManipulator::setDirection(glm::vec3 inDir_L, float length)
{
    const float baseLength = 1.5f;
    if (length <= 0.0f)
        length = baseLength;

    const float dirLength = glm::length(inDir_L);
    if (dirLength < 1e-6f)
        dir_L = glm::vec3(0.0f, 0.0f, 1.0f);
    else
        dir_L = inDir_L / dirLength;

    const float scaleFactor = length / baseLength;
    const glm::mat4 scale =
        glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, scaleFactor));

    const glm::mat4 rotation =
        Utils::rotationBetweenVectors(glm::vec3(0.0f, 0.0f, 1.0f), dir_L);


    // create mesh and attach to manipulator
    auto child = std::make_unique<Node>();
    child->setManipulator(true);
    child->setRelativeTransform(rotation * scale);
    auto mesh = std::make_unique<Mesh>(Utils::meshScaleArrow());
    mesh->material.setDiffuse(glm::abs(dir_L));
    mesh->setRenderBlackEdges(false);
    child->attachMesh(std::move(mesh));
    attachNode(std::move(child));
}

void ScaleManipulator::handleMovement(MovementType movementType,
                                      const Viewport &viewport, double x,
                                      double y)
{
    if (movementType == MovementType::Push)
    {
        cachedAbsTrf = calcAbsoluteTransform();
        cachedInvAbsTrf = glm::inverse(cachedAbsTrf);
        cachedDir_W =
            glm::normalize(glm::vec3(cachedAbsTrf * glm::vec4(dir_L, 0.0f)));
    }

    glm::vec3 pt_L;
    if (!projectCursorOntoAxis(viewport, x, y, cachedAbsTrf, cachedInvAbsTrf,
                               cachedDir_W, pt_L))
        return;

    // Project pt_L onto the manipulation axis to get a signed distance.
    float currentProjection = glm::dot(pt_L, dir_L);

    if (movementType == MovementType::Push)
    {
        initialProjection = currentProjection;
        cumulativeScale = 1.0f;
        return;
    }

    if (movementType != MovementType::Drag &&
        movementType != MovementType::Release)
        return;

    // Guard: if the initial grab was at/near the origin along the axis,
    // we can't compute a meaningful ratio.
    if (std::abs(initialProjection) < 1e-5f)
        return;

    // Compute the absolute target scale as a ratio of current / initial.
    float targetScale = currentProjection / initialProjection;

    // Clamp to prevent collapse or explosion.
    targetScale = glm::clamp(targetScale, 0.01f, 100.0f);

    // Derive the incremental scale to send (relative to what we've already sent).
    float incrementalScale = targetScale / cumulativeScale;

    // Reject degenerate increments.
    if (!std::isfinite(incrementalScale) || incrementalScale < 1e-4f ||
        incrementalScale > 1e4f)
        return;

    cumulativeScale = targetScale;

    glm::vec3 scaleVec = glm::vec3(1.0f) + dir_L * (incrementalScale - 1.0f);

    sendFeedback(glm::scale(glm::mat4(1.0f), scaleVec));
}