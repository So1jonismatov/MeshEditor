#pragma once

#include "Camera.h"
#include <cstdint>

struct ray
{
    glm::vec3 orig;
    glm::vec3 dir{0.0f, 0.0f, 1.0f};
};

struct ViewportMatrices
{
    glm::mat4 viewProjection{1.0f};
    glm::vec3 eye{0.0f, 0.0f, 0.0f};
};

class Viewport
{
public:
    glm::mat4 calcProjectionMatrix() const;
    ViewportMatrices calcMatrices() const;

    void setViewportSize(uint32_t inWidth, uint32_t inHeight);
    void setFOV(double inFOV);
    void setZNear(double inZNear);
    void setZFar(double inZFar);
    void setParallelProjection(bool use);

    double getZNear() const;
    double getZFar() const;
    double getFov() const;
    double getWidth() const;
    double getHeight() const;
    bool isParallelProjection() const;

    ray calcCursorRay(double x, double y) const;

    double calcTargetPlaneWidth() const;
    double calcTargetPlaneHeight() const;
    double calcAspectRatio() const;

    void zoomToFit(glm::vec3 min, glm::vec3 max);
    void adjustNearFarPlanes(const glm::vec3 &sceneMin, const glm::vec3 &sceneMax);

    Camera &getCamera();
    const Camera &getCamera() const;

private:
    double m_zNear = 1;
    double m_zFar = 100.0;
    double m_FOV = 90.0;
    uint32_t m_Width = 1;
    uint32_t m_Height = 1;
    bool m_parallelProjection = false;

    Camera m_camera;
};
