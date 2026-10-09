#include "ViewPort.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Viewport::calcProjectionMatrix() const
{
    if (m_parallelProjection)
    {
        double halfHeight = calcTargetPlaneHeight() / 2.0;
        double halfWidth = calcTargetPlaneWidth() / 2.0;
        double depthRange = m_zFar;
        return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight,
                          -depthRange, depthRange);
    }
    else
    {
        return glm::perspective(glm::radians(m_FOV), calcAspectRatio(), m_zNear,
                                m_zFar);
    }
}

ViewportMatrices Viewport::calcMatrices() const
{
    ViewportMatrices matrices;
    matrices.viewProjection =
        calcProjectionMatrix() * m_camera.calcViewMatrix();
    matrices.eye = m_camera.getEye();
    return matrices;
}

void Viewport::setViewportSize(uint32_t inWidth, uint32_t inHeight)
{
    m_Width = inWidth;
    m_Height = inHeight;
}

void Viewport::setFOV(double inFOV)
{
    m_FOV = inFOV;
}

void Viewport::setZNear(double inZNear)
{
    m_zNear = inZNear;
}

void Viewport::setZFar(double inZFar)
{
    m_zFar = inZFar;
}

void Viewport::setParallelProjection(bool use)
{
    m_parallelProjection = use;
}

double Viewport::getZNear() const
{
    return m_zNear;
}

double Viewport::getZFar() const
{
    return m_zFar;
}

double Viewport::getFov() const
{
    return m_FOV;
}

double Viewport::getWidth() const
{
    return m_Width;
}

double Viewport::getHeight() const
{
    return m_Height;
}

bool Viewport::isParallelProjection() const
{
    return m_parallelProjection;
}

ray Viewport::calcCursorRay(double x, double y) const
{
    glm::vec4 viewport(0.0f, 0.0f, (float)(m_Width), (float)(m_Height));
    const glm::mat4 view = m_camera.calcViewMatrix();
    const glm::mat4 projection = calcProjectionMatrix();

    float winY = (float)(m_Height) - (float)(y); // inverting y

    glm::vec3 a =
        glm::unProject(glm::vec3(x, winY, 0.0f), view, projection, viewport);
    glm::vec3 b =
        glm::unProject(glm::vec3(x, winY, 1.0f), view, projection, viewport);

    return ray{a, glm::normalize(b - a)};
}

double Viewport::calcTargetPlaneWidth() const
{
    return calcTargetPlaneHeight() * calcAspectRatio();
}

double Viewport::calcTargetPlaneHeight() const
{
    return 2.0 * m_camera.distanceFromEyeToTarget() *
           tan(glm::radians(m_FOV / 2.0));
}

double Viewport::calcAspectRatio() const
{
    if (m_Height == 0)
        return 1.0;
    return static_cast<double>(m_Width) / static_cast<double>(m_Height);
}

void Viewport::adjustNearFarPlanes(const glm::vec3 &sceneMin, const glm::vec3 &sceneMax)
{
    const glm::vec3 eye = m_camera.getEye();
    const glm::vec3 target = m_camera.getTarget();
    glm::vec3 dir = target - eye;
    float distToTarget = glm::length(dir);
    if (distToTarget < 1e-4f)
    {
        dir = glm::vec3(0.0f, 0.0f, -1.0f);
        distToTarget = 1.0f;
    }
    else
    {
        dir /= distToTarget;
    }

    const float radius = glm::length(sceneMax - sceneMin) * 0.5f;

    // Fallback if scene is degenerate or empty
    if (radius < 1e-4f)
    {
        setZNear(std::max(0.01, (double)distToTarget * 0.01));
        setZFar(std::max(10.0, (double)distToTarget * 5.0));
        return;
    }

    // Project the 8 bounding box corners along the camera view direction
    const glm::vec3 corners[8] = {
        {sceneMin.x, sceneMin.y, sceneMin.z},
        {sceneMax.x, sceneMin.y, sceneMin.z},
        {sceneMin.x, sceneMax.y, sceneMin.z},
        {sceneMax.x, sceneMax.y, sceneMin.z},
        {sceneMin.x, sceneMin.y, sceneMax.z},
        {sceneMax.x, sceneMin.y, sceneMax.z},
        {sceneMin.x, sceneMax.y, sceneMax.z},
        {sceneMax.x, sceneMax.y, sceneMax.z}
    };

    float minDepth = std::numeric_limits<float>::max();
    float maxDepth = std::numeric_limits<float>::lowest();

    for (int i = 0; i < 8; ++i)
    {
        float d = glm::dot(corners[i] - eye, dir);
        minDepth = std::min(minDepth, d);
        maxDepth = std::max(maxDepth, d);
    }

    double computedNear;
    double computedFar;

    if (m_parallelProjection)
    {
        double extent = std::max((double)radius * 1.5, (double)distToTarget + radius);
        computedFar = std::max(10.0, extent * 1.2);
        setZFar(computedFar);
        return;
    }

    // Perspective projection dynamic depth range:
    if (minDepth > 0.01f * radius)
    {
        // Entire model is in front of the camera.
        // Keeping zNear tight to minDepth maximizes 24-bit depth precision across the visible model.
        computedNear = std::max(0.001 * (double)radius, (double)minDepth * 0.9);
        computedFar = std::max(computedNear * 2.0, (double)maxDepth * 1.15);
    }
    else
    {
        // Camera is inside the bounding box or very close.
        computedNear = std::max(0.0005 * (double)radius, std::min(0.02 * (double)distToTarget, 0.05));
        computedFar = std::max(computedNear * 10.0, (double)(distToTarget + radius * 2.0));
    }

    // Safety clamps
    if (computedNear < 0.0001)
        computedNear = 0.0001;
    if (computedFar <= computedNear * 1.2)
        computedFar = computedNear * 2.0;

    setZNear(computedNear);
    setZFar(computedFar);
}

void Viewport::zoomToFit(glm::vec3 min, glm::vec3 max)
{
    glm::vec3 center = (min + max) * 0.5f;
    double radius = glm::length(max - min) * 0.5;
    if (radius < 0.001)
        radius = 0.001;

    double halfFov = glm::radians(m_FOV) / 2.0;
    double distance = radius / std::sin(halfFov);
    m_camera.setEyeTargetUp(center + glm::vec3(0, 0, distance), center,
                            glm::vec3(0, 1, 0));
    adjustNearFarPlanes(min, max);
}

Camera &Viewport::getCamera()
{
    return m_camera;
}

const Camera &Viewport::getCamera() const
{
    return m_camera;
}
