#include "TranslationManipulator.h"
#include "utils/CreatePrimitives.h"
#include "utils/MathUtils.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

TranslationManipulator::TranslationManipulator(glm::vec3 inDir_L, float length,
                                               float visualScale)
{
    setManipulator(true);
    setDirection(inDir_L, length, visualScale);
}

void TranslationManipulator::setDirection(glm::vec3 inDir_L, float length,
                                          float visualScale)
{
    const float baseLength = 1.5f;
    if (length <= 0.0f)
        length = baseLength;

    if (visualScale <= 0.0f)
        visualScale = 1.0f;

    // Normalize the Direction
    const float dirLength = glm::length(inDir_L);
    if (dirLength < 1e-6f)
    {
        dir_L = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    else
    {
        dir_L = inDir_L / dirLength;
    }

    // Create an arrow mesh and attach it to a child node
    const float scaleFactor = length / baseLength;
    const glm::mat4 scale =
        glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, scaleFactor));

    const glm::mat4 rotation =
        Utils::rotationBetweenVectors(glm::vec3(0.0f, 0.0f, 1.0f), dir_L);

    auto child = std::make_unique<Node>();
    child->setManipulator(true);
    child->setRelativeTransform(
        rotation * scale *
        glm::scale(glm::mat4(1.0f), glm::vec3(visualScale, visualScale, 1.0f)));
    auto mesh = std::make_unique<Mesh>(Utils::meshArrow());
    glm::vec3 absDir = glm::abs(dir_L);
    mesh->material.setDiffuse(absDir);
    mesh->setRenderBlackEdges(false);
    child->attachMesh(std::move(mesh));
    attachNode(std::move(child));
}

void TranslationManipulator::handleMovement(MovementType movementType,
                                            const Viewport &viewport, double x,
                                            double y)
{
    auto isFiniteVec = [](const glm::vec3 &v)
    { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };

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

    if (movementType == MovementType::Push)
    {
        startPoint_L = pt_L;
    }
    else if (movementType == MovementType::Drag ||
             movementType == MovementType::Release)
    {
        glm::vec3 delta = pt_L - startPoint_L;
        if (!isFiniteVec(delta) || glm::length(delta) > 1e4f)
        {
            startPoint_L = pt_L;
            return;
        }

        glm::vec3 projDelta = glm::dot(delta, dir_L) * dir_L;
        startPoint_L += projDelta;
        sendFeedback(glm::translate(glm::mat4(1.0f), projDelta));
    }
}