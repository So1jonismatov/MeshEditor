#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera
{
public:
    glm::mat4 calcViewMatrix() const;

    glm::vec3 calcForward() const;
    glm::vec3 calcRight() const;
    double distanceFromEyeToTarget() const;
    const glm::vec3 &getEye() const;
    const glm::vec3 &getTarget() const;
    const glm::vec3 &getUp() const;

    void setFrontView();
    void setTopView();
    void setRearView();
    void setRightView();
    void setLeftView();
    void setBottomView();
    void setIsoView();

    void orbitArcball(glm::vec3 fromTrackball, glm::vec3 toTrackball);
    void fpsLook(float yawDelta, float pitchDelta);
    void pan(double u, double v);
    void zoom(double factor);

    // One discrete arcball orbit step around the target, animated via
    // startTransitionTo. Shared by the navigation gizmo's arrow buttons and
    // the keyboard arrow keys so both behave identically. Rotations are about
    // the camera's own axes: Up/Down about its right axis, Left/Right about
    // its up axis, RollLeft/RollRight about the look-at axis. A still-running
    // previous step is snapped to its end pose first, so rapid presses chain
    // full steps. Steps snap to the angleRadians grid: from 170° a +15° step
    // rotates only 10° to land exactly on 180°.
    enum class OrbitStepDirection
    {
        Up,
        Down,
        Left,
        Right,
        RollLeft,
        RollRight
    };
    void orbitStepAnimated(OrbitStepDirection dir, float angleRadians,
                           double durationSec);

    void translate(glm::vec3 delta);
    void setDistanceToTarget(double D);
    void transform(const glm::mat4 &trf);
    void rotate(glm::vec3 point, glm::vec3 axis, double angle);
    void setEyeTargetUp(glm::vec3 newEye, glm::vec3 newTarget, glm::vec3 newUp);

    static glm::vec3 mapToTrackball(float nx, float ny);

    // Smooth animated transition toward another camera pose (used by the view
    // presets / navigation cube). The transition is ticked once per frame from
    // View::update with the real elapsed time, so its duration is independent
    // of the frame rate. Any direct camera manipulation (orbit, pan, zoom,
    // preset snap, ...) cancels an in-flight transition.
    void startTransitionTo(const Camera &goal, double durationSec = 1.0);
    bool tickTransition(double dtSeconds); // true while animating
    bool isTransitioning() const;
    void cancelTransition();
    // Snap an in-flight transition straight to its end pose (no-op when idle).
    void finishTransition();

private:
    glm::vec3 eye{0.0f, 0.0f, 1.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};

    // In-flight transition state (valid while m_transitionActive)
    bool m_transitionActive = false;
    double m_transitionTime = 0.0;
    double m_transitionDuration = 1.0;
    glm::quat m_transitionFromRot{1.0f, 0.0f, 0.0f, 0.0f};
    glm::quat m_transitionToRot{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_transitionFromTarget{0.0f};
    glm::vec3 m_transitionToTarget{0.0f};
    float m_transitionFromDist = 1.0f;
    float m_transitionToDist = 1.0f;
};
