#include "RotationManipulator.h"
#include "utils/CreatePrimitives.h"
#include "utils/MathUtils.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

using Utils::calcPlaneAngle;
using Utils::getRayPlaneIntersection;
using Utils::normalizedOr;
using Utils::pickLocalRingBasis;
using Utils::wrapAngle;

RotationManipulator::RotationManipulator(glm::vec3 inDir_L, float majorRadius,
                                         float minorRadius)
{
    setManipulator(true);
    setDirection(inDir_L, majorRadius, minorRadius);
}

void RotationManipulator::setDirection(glm::vec3 inDir_L, float majorRadius,
                                       float minorRadius)
{
    if (majorRadius <= 0.0f)
        majorRadius = 1.0f;
    if (minorRadius <= 0.0f)
        minorRadius = 0.05f;

    // Normalize the Direction
    const float dirLength = glm::length(inDir_L);
    if (dirLength < 1e-6f)
        dir_L = glm::vec3(0.0f, 1.0f, 0.0f);
    else
        dir_L = inDir_L / dirLength;
    // Create a torus mesh and attach it to a child node
    const glm::mat4 rotation =
        Utils::rotationBetweenVectors(glm::vec3(0.0f, 1.0f, 0.0f), dir_L);

    auto child = std::make_unique<Node>();
    child->setManipulator(true);
    child->setRelativeTransform(rotation);
    auto mesh =
        std::make_unique<Mesh>(Utils::meshTorus(minorRadius, majorRadius, 64));
    glm::vec3 absDir = glm::abs(dir_L);
    mesh->material.setDiffuse(absDir);
    mesh->setRenderBlackEdges(false);
    child->attachMesh(std::move(mesh));
    attachNode(std::move(child));
}

void RotationManipulator::handleMovement(MovementType movementType,
                                         const Viewport &viewport, double x,
                                         double y)
{
    if (movementType == MovementType::Push)
    {
        glm::mat4 absTrf = calcAbsoluteTransform();
        dragOrigin_W = glm::vec3(absTrf * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        dragAxis_W = normalizedOr(glm::vec3(absTrf * glm::vec4(dir_L, 0.0f)),
                                  glm::vec3(0.0f, 1.0f, 0.0f));

        glm::vec3 basisU_L(1.0f, 0.0f, 0.0f);
        glm::vec3 basisV_L(0.0f, 1.0f, 0.0f);
        pickLocalRingBasis(dir_L, basisU_L, basisV_L);

        dragBasisU_W =
            normalizedOr(glm::vec3(absTrf * glm::vec4(basisU_L, 0.0f)),
                         glm::vec3(1.0f, 0.0f, 0.0f));
        dragBasisU_W -= dragAxis_W * glm::dot(dragBasisU_W, dragAxis_W);
        dragBasisU_W = normalizedOr(dragBasisU_W, glm::vec3(1.0f, 0.0f, 0.0f));
        dragBasisV_W = normalizedOr(glm::cross(dragAxis_W, dragBasisU_W),
                                    glm::vec3(0.0f, 0.0f, 1.0f));
    }

    ray cursorRay_W = viewport.calcCursorRay(x, y);
    float ndotd = glm::dot(cursorRay_W.dir, dragAxis_W);
    bool useScreenSpace = std::abs(ndotd) < 0.1f;
    float currentAngle = 0.0f;
    glm::vec3 ptOnPlane_W;

    if (!useScreenSpace &&
        getRayPlaneIntersection(cursorRay_W.orig, cursorRay_W.dir, dragOrigin_W,
                                dragAxis_W, ptOnPlane_W))
    {
        currentAngle = calcPlaneAngle(ptOnPlane_W, dragOrigin_W, dragBasisU_W,
                                      dragBasisV_W);
    }
    else
    {

        glm::mat4 viewProj = viewport.calcMatrices().viewProjection;
        glm::vec4 centerProj = viewProj * glm::vec4(dragOrigin_W, 1.0f);
        glm::vec2 centerSS = glm::vec2(centerProj) / centerProj.w;
        float ndcX = static_cast<float>(x / viewport.getWidth() * 2.0 - 1.0);
        float ndcY = static_cast<float>(1.0 - y / viewport.getHeight() * 2.0);
        glm::vec2 cursorSS = glm::vec2(ndcX, ndcY);
        glm::vec2 dirSS = glm::normalize(cursorSS - centerSS);
        currentAngle = std::atan2(dirSS.y, dirSS.x);
    }

    if (movementType == MovementType::Push)
    {
        dragAngle = currentAngle;
        dragging = true;
        return;
    }

    if (!dragging)
        return;

    float deltaAngle = wrapAngle(currentAngle - dragAngle);
    dragAngle = currentAngle;

    if (std::abs(deltaAngle) > 1e-6f)
    {
        glm::mat4 deltaMatrix = glm::rotate(glm::mat4(1.0f), deltaAngle, dir_L);
        sendFeedback(deltaMatrix);
    }

    if (movementType == MovementType::Release)
        dragging = false;
}