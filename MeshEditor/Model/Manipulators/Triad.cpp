#include "Triad.h"

Triad::Triad(float majorRadius, float arrowLength)
{
    setManipulator(true);
    if (majorRadius <= 0.0f)
        majorRadius = 1.0f;
    if (arrowLength <= 0.0f)
        arrowLength = 1.5f;

    const float minorRadius = glm::max(majorRadius * 0.03f, 0.02f);
    const float handleLength = glm::max(arrowLength, majorRadius * 1.1f);

    attachNode(std::make_unique<TranslationManipulator>(glm::vec3{1, 0, 0},
                                                        handleLength));
    attachNode(std::make_unique<TranslationManipulator>(glm::vec3{0, 1, 0},
                                                        handleLength));
    attachNode(std::make_unique<TranslationManipulator>(glm::vec3{0, 0, 1},
                                                        handleLength));

    attachNode(std::make_unique<RotationManipulator>(
        glm::vec3{1, 0, 0}, majorRadius, minorRadius));
    attachNode(std::make_unique<RotationManipulator>(
        glm::vec3{0, 1, 0}, majorRadius, minorRadius));
    attachNode(std::make_unique<RotationManipulator>(
        glm::vec3{0, 0, 1}, majorRadius, minorRadius));
}

ScaleTriad::ScaleTriad(float arrowLength)
{
    setManipulator(true);
    if (arrowLength <= 0.0f)
        arrowLength = 1.5f;

    attachNode(
        std::make_unique<ScaleManipulator>(glm::vec3{1, 0, 0}, arrowLength));
    attachNode(
        std::make_unique<ScaleManipulator>(glm::vec3{0, 1, 0}, arrowLength));
    attachNode(
        std::make_unique<ScaleManipulator>(glm::vec3{0, 0, 1}, arrowLength));
}
