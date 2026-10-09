#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace
{
constexpr float kCameraEpsilon = 1e-6f;

bool isFiniteVec3(const glm::vec3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

glm::vec3 normalizedOr(const glm::vec3 &value, const glm::vec3 &fallback)
{
    const float lenSq = glm::dot(value, value);
    if (lenSq <= kCameraEpsilon || !std::isfinite(lenSq))
        return fallback;
    return value / std::sqrt(lenSq);
}

// Rotation delta that lands `currentAngle` on the next multiple of `step` in
// the direction of travel, so steps snap to the grid: from 170° a +15° step
// moves only +10° to reach 180°. An angle already within ~5% of a step of a
// multiple counts as being ON it and steps a full grid cell onward.
float snappedStepDelta(float currentAngle, float step, float sign)
{
    constexpr float kGridEps = 0.05f;
    const float units = currentAngle / step;
    const float target = (sign > 0.0f)
                             ? (std::floor(units + kGridEps) + 1.0f) * step
                             : (std::ceil(units - kGridEps) - 1.0f) * step;
    return target - currentAngle;
}

// Camera-to-world rotation of a look-at pose, as a quaternion. Columns are the
// camera basis (right, up, -forward), matching the OpenGL view convention so
// that q * (0,0,-1) == forward and q * (0,1,0) == up.
glm::quat orientationOf(const glm::vec3 &eye, const glm::vec3 &target,
                        const glm::vec3 &up)
{
    const glm::vec3 forward =
        normalizedOr(target - eye, glm::vec3(0.0f, 0.0f, -1.0f));
    glm::vec3 right = normalizedOr(glm::cross(forward, up),
                                   glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 trueUp =
        normalizedOr(glm::cross(right, forward), glm::vec3(0.0f, 1.0f, 0.0f));
    return glm::quat_cast(glm::mat3(right, trueUp, -forward));
}
} // namespace

glm::mat4 Camera::calcViewMatrix() const
{
    return glm::lookAt(eye, target, up);
}

glm::vec3 Camera::calcForward() const
{
    return normalizedOr(target - eye, glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Camera::calcRight() const
{
    const glm::vec3 forward = calcForward();
    glm::vec3 right = glm::cross(forward, up);
    if (glm::dot(right, right) <= kCameraEpsilon || !isFiniteVec3(right))
    {
        right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
        if (glm::dot(right, right) <= kCameraEpsilon || !isFiniteVec3(right))
        {
            right = glm::cross(forward, glm::vec3(1.0f, 0.0f, 0.0f));
        }
    }
    return normalizedOr(right, glm::vec3(1.0f, 0.0f, 0.0f));
}

double Camera::distanceFromEyeToTarget() const
{
    return glm::length(target - eye);
}

const glm::vec3 &Camera::getEye() const
{
    return eye;
}

const glm::vec3 &Camera::getTarget() const
{
    return target;
}

const glm::vec3 &Camera::getUp() const
{
    return up;
}

void Camera::translate(glm::vec3 delta)
{
    cancelTransition();
    eye += delta;
    target += delta;
}

void Camera::setDistanceToTarget(double D)
{
    cancelTransition();
    const glm::vec3 forward = calcForward();
    const float distance = std::isfinite(D)
                               ? std::max(static_cast<float>(D), kCameraEpsilon)
                               : 1.0f;
    eye = target - forward * distance;
}

void Camera::transform(const glm::mat4 &trf)
{
    cancelTransition();
    eye = glm::vec3(trf * glm::vec4(eye, 1.0f));
    target = glm::vec3(trf * glm::vec4(target, 1.0f));
    up = normalizedOr(glm::vec3(trf * glm::vec4(up, 0.0f)),
                      glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::rotate(glm::vec3 point, glm::vec3 axis, double angle)
{
    glm::mat4 trf = glm::translate(glm::mat4(1.0f), point) *
                    glm::rotate(glm::mat4(1.0f), (float)angle, axis) *
                    glm::translate(glm::mat4(1.0f), -point);
    transform(trf);
}

void Camera::setEyeTargetUp(glm::vec3 newEye, glm::vec3 newTarget,
                            glm::vec3 newUp)
{
    cancelTransition();
    eye = newEye;
    target = newTarget;
    const glm::vec3 forward =
        normalizedOr(target - eye, glm::vec3(0.0f, 0.0f, -1.0f));
    glm::vec3 upCandidate = normalizedOr(newUp, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 right = glm::cross(forward, upCandidate);
    if (glm::dot(right, right) <= kCameraEpsilon || !isFiniteVec3(right))
    {
        upCandidate = glm::abs(forward.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                  : glm::vec3(0.0f, 0.0f, 1.0f);
        right = glm::cross(forward, upCandidate);
        if (glm::dot(right, right) <= kCameraEpsilon || !isFiniteVec3(right))
        {
            upCandidate = glm::vec3(1.0f, 0.0f, 0.0f);
            right = glm::cross(forward, upCandidate);
        }
    }

    right = normalizedOr(right, glm::vec3(1.0f, 0.0f, 0.0f));
    up = normalizedOr(glm::cross(right, forward), glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::setFrontView()
{
    double D = distanceFromEyeToTarget();
    if (D < 0.0001)
        D = 1.0;
    setEyeTargetUp(target + glm::vec3(0, 0, 1), target, glm::vec3(0, 1, 0));
    setDistanceToTarget(D);
}

void Camera::setTopView()
{
    double D = distanceFromEyeToTarget();
    if (D < 0.0001)
        D = 1.0;
    setEyeTargetUp(target + glm::vec3(0, 1, 0), target, glm::vec3(0, 0, -1));
    setDistanceToTarget(D);
}

void Camera::setRearView()
{
    glm::vec3 oldTarget = target;
    setFrontView();
    rotate(oldTarget, glm::vec3(0, 1, 0), glm::radians(180.0));
}

void Camera::setRightView()
{
    glm::vec3 oldTarget = target;
    setFrontView();
    rotate(oldTarget, glm::vec3(0, 1, 0), glm::radians(90.0));
}

void Camera::setLeftView()
{
    glm::vec3 oldTarget = target;
    setFrontView();
    rotate(oldTarget, glm::vec3(0, 1, 0), glm::radians(-90.0));
}

void Camera::setBottomView()
{
    glm::vec3 oldTarget = target;
    setTopView();
    rotate(oldTarget, glm::vec3(1, 0, 0), glm::radians(180.0));
}

void Camera::setIsoView()
{
    double D = distanceFromEyeToTarget();
    if (D < 0.0001)
        D = 1.0;
    setEyeTargetUp(target + glm::vec3(1, 1, 1), target, glm::vec3(0, 1, 0));
    setDistanceToTarget(D);
}

void Camera::orbitArcball(glm::vec3 fromTrackball, glm::vec3 toTrackball)
{
    cancelTransition();
    if (!isFiniteVec3(fromTrackball) || !isFiniteVec3(toTrackball))
        return;

    const float fromLenSq = glm::dot(fromTrackball, fromTrackball);
    const float toLenSq = glm::dot(toTrackball, toTrackball);
    if (fromLenSq <= kCameraEpsilon || toLenSq <= kCameraEpsilon)
        return;

    const glm::vec3 na = fromTrackball / std::sqrt(fromLenSq);
    const glm::vec3 nb = toTrackball / std::sqrt(toLenSq);
    const float dotProd = glm::clamp(glm::dot(na, nb), -1.0f, 1.0f);
    const double angle = std::acos(dotProd);
    if (!std::isfinite(angle) || angle <= 1e-6)
        return;

    glm::vec3 axis = glm::cross(na, nb);
    if (glm::dot(axis, axis) <= kCameraEpsilon || !isFiniteVec3(axis))
    {
        axis = glm::cross(na, glm::vec3(0.0f, 1.0f, 0.0f));
        if (glm::dot(axis, axis) <= kCameraEpsilon || !isFiniteVec3(axis))
            axis = glm::cross(na, glm::vec3(1.0f, 0.0f, 0.0f));
    }
    axis = normalizedOr(axis, glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat3 viewRotInv = glm::inverse(glm::mat3(calcViewMatrix()));
    glm::vec3 axisWCS = viewRotInv * axis;
    if (!isFiniteVec3(axisWCS) || glm::dot(axisWCS, axisWCS) <= kCameraEpsilon)
        return;

    const glm::quat rotation = glm::angleAxis(
        (float)angle, normalizedOr(axisWCS, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 offset = eye - target;
    const glm::mat3 rotMat = glm::mat3_cast(rotation);
    eye = target + rotMat * offset;
    up = normalizedOr(rotMat * up, glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::pan(double u, double v)
{
    const glm::vec3 right = calcRight();
    const glm::vec3 upVec = normalizedOr(up, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 delta =
        right * static_cast<float>(u) + upVec * static_cast<float>(v);
    translate(delta);
}

void Camera::zoom(double factor)
{
    double D = distanceFromEyeToTarget();
    setDistanceToTarget(D * factor);
}

void Camera::fpsLook(float yawDelta, float pitchDelta)
{
    cancelTransition();
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 forward =
        normalizedOr(target - eye, glm::vec3(0.0f, 0.0f, -1.0f));

    const glm::quat yawRotation = glm::angleAxis(yawDelta, worldUp);
    forward = glm::mat3_cast(yawRotation) * forward;

    glm::vec3 right = normalizedOr(glm::cross(forward, worldUp), calcRight());
    if (glm::dot(right, right) < 1e-8f)
        right = calcRight();

    const glm::quat pitchRotation = glm::angleAxis(pitchDelta, right);
    glm::vec3 pitchedForward = glm::mat3_cast(pitchRotation) * forward;
    double epsilon = 1e-3f;
    if (0.9f - std::abs(
                   glm::dot(normalizedOr(pitchedForward, forward), worldUp)) >=
        epsilon)
        forward = pitchedForward;

    target = eye + normalizedOr(forward, glm::vec3(0.0f, 0.0f, -1.0f));
    right = normalizedOr(
        glm::cross(normalizedOr(target - eye, forward), worldUp), calcRight());
    if (glm::dot(right, right) >= 1e-8f)
        up =
            normalizedOr(glm::cross(right, normalizedOr(target - eye, forward)),
                         glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::orbitStepAnimated(OrbitStepDirection dir, float angleRadians,
                               double durationSec)
{
    // The step is computed on a copy and the live camera animates toward it;
    // starting from mid-transition poses works because startTransitionTo
    // captures the CURRENT pose as the new "from".
    // Chained steps: if the previous step is still animating, snap it to its
    // end pose first so this step starts from there. Rapid presses therefore
    // accumulate one full step each instead of restarting the smoothstep from
    // a mid-transition pose and losing rotation.
    finishTransition();

    // The step is an arcball rotation about the CAMERA's own axes: Up/Down
    // orbits about the camera's right axis, Left/Right about its up axis,
    // RollLeft/RollRight rolls about the look-at axis.
    Camera goal = *this;
    const glm::vec3 pivot = target;

    glm::vec3 axis;
    float sign;
    switch (dir)
    {
    case OrbitStepDirection::Up:
        axis = calcRight();
        sign = -1.0f;
        break;
    case OrbitStepDirection::Down:
        axis = calcRight();
        sign = 1.0f;
        break;
    case OrbitStepDirection::Left:
        axis = up;
        sign = -1.0f;
        break;
    case OrbitStepDirection::Right:
        axis = up;
        sign = 1.0f;
        break;
    case OrbitStepDirection::RollLeft:
        axis = calcForward();
        sign = 1.0f;
        break;
    case OrbitStepDirection::RollRight:
    default:
        axis = calcForward();
        sign = -1.0f;
        break;
    }
    axis = normalizedOr(axis, glm::vec3(0.0f, 1.0f, 0.0f));

    // Snap to the step grid: measure the current absolute angle about the
    // rotation axis (right-handed, from the projection of a world axis onto
    // the rotation plane) and rotate only as far as the next grid multiple —
    // a +15° press at 170° moves 10° and lands exactly on 180°. Rolls track
    // the up vector (the eye doesn't move under roll); orbits track the eye.
    float delta = sign * angleRadians;

    const bool isRoll = dir == OrbitStepDirection::RollLeft ||
                        dir == OrbitStepDirection::RollRight;
    const glm::vec3 tracked = isRoll ? up : (eye - target);
    glm::vec3 inPlane = tracked - axis * glm::dot(tracked, axis);
    if (glm::dot(inPlane, inPlane) > kCameraEpsilon)
    {
        inPlane = glm::normalize(inPlane);

        // Reference: the world axis most perpendicular to the rotation axis,
        // projected into the rotation plane. All nice poses (front/right/top
        // views) sit on multiples of 90°, so any world axis yields the same
        // 15° grid.
        const glm::vec3 candidates[3] = {{1.0f, 0.0f, 0.0f},
                                         {0.0f, 1.0f, 0.0f},
                                         {0.0f, 0.0f, 1.0f}};
        int best = 0;
        float bestAbsDot = 2.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float absDot = std::abs(glm::dot(axis, candidates[i]));
            if (absDot < bestAbsDot)
            {
                bestAbsDot = absDot;
                best = i;
            }
        }
        glm::vec3 ref =
            candidates[best] - axis * glm::dot(candidates[best], axis);
        if (glm::dot(ref, ref) > kCameraEpsilon)
        {
            ref = glm::normalize(ref);
            const float current =
                std::atan2(glm::dot(glm::cross(ref, inPlane), axis),
                           glm::dot(ref, inPlane));
            delta = snappedStepDelta(current, angleRadians, sign);
        }
    }

    goal.rotate(pivot, axis, delta);
    startTransitionTo(goal, durationSec);
}

void Camera::startTransitionTo(const Camera &goal, double durationSec)
{
    m_transitionFromRot = orientationOf(eye, target, up);
    m_transitionToRot = orientationOf(goal.eye, goal.target, goal.up);
    m_transitionFromTarget = target;
    m_transitionToTarget = goal.target;
    m_transitionFromDist =
        static_cast<float>(std::max(distanceFromEyeToTarget(), 1e-4));
    m_transitionToDist =
        static_cast<float>(std::max(goal.distanceFromEyeToTarget(), 1e-4));
    m_transitionTime = 0.0;
    m_transitionDuration = std::max(durationSec, 0.0);
    m_transitionActive = true;
    if (m_transitionDuration <= 0.0)
        tickTransition(0.0); // snap immediately
}

bool Camera::tickTransition(double dtSeconds)
{
    if (!m_transitionActive)
        return false;

    m_transitionTime += std::max(dtSeconds, 0.0);
    float t = m_transitionDuration <= 0.0
                  ? 1.0f
                  : static_cast<float>(
                        std::min(m_transitionTime / m_transitionDuration, 1.0));
    const float ease = t * t * (3.0f - 2.0f * t); // smoothstep ease in-out

    const glm::quat rot =
        glm::slerp(m_transitionFromRot, m_transitionToRot, ease);
    const glm::vec3 newTarget =
        glm::mix(m_transitionFromTarget, m_transitionToTarget, ease);
    const float dist =
        glm::mix(m_transitionFromDist, m_transitionToDist, ease);

    const glm::vec3 forward =
        normalizedOr(rot * glm::vec3(0.0f, 0.0f, -1.0f),
                     glm::vec3(0.0f, 0.0f, -1.0f));
    const glm::vec3 newUp = normalizedOr(rot * glm::vec3(0.0f, 1.0f, 0.0f),
                                         glm::vec3(0.0f, 1.0f, 0.0f));

    // Applied directly to the members: the public setters cancel in-flight
    // transitions on purpose (they mean "the user grabbed the camera").
    target = newTarget;
    eye = newTarget - forward * dist;
    up = newUp;

    if (t >= 1.0f)
        m_transitionActive = false;
    return true;
}

glm::vec3 Camera::mapToTrackball(float nx, float ny) // returns z from x and y
{
    glm::vec3 v(nx, ny, 0.0f);
    const float lenSq = v.x * v.x + v.y * v.y;
    if (lenSq >= 1.0f)
        return glm::normalize(v);
    v.z = std::sqrt(1.0f - lenSq);
    return v;
}

bool Camera::isTransitioning() const
{
    return m_transitionActive;
}
void Camera::cancelTransition()
{
    m_transitionActive = false;
}

void Camera::finishTransition()
{
    if (!m_transitionActive)
        return;
    m_transitionTime = m_transitionDuration;
    tickTransition(0.0);
}
