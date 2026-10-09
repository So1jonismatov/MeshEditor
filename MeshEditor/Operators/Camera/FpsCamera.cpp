#include "FpsCamera.h"

#include "View.h"
#include "../Interfaces/IWindow.h"
#include <algorithm>

void FpsCameraOperator::onEnter(View &view)
{
    m_hasLastCursor = false;
    m_moveForward = false;
    m_moveBack = false;
    m_moveLeft = false;
    m_moveRight = false;
    if (IWindow *window = view.getWindow())
    {
        window->setCursorCaptured(true);
    }
}

void FpsCameraOperator::onExit(View &view)
{
    if (IWindow *window = view.getWindow())
    {
        window->setCursorCaptured(false);
    }
    m_hasLastCursor = false;
    m_moveForward = false;
    m_moveBack = false;
    m_moveLeft = false;
    m_moveRight = false;
}

void FpsCameraOperator::onUpdate(View &view)
{
    moveCamera(view);
}

void FpsCameraOperator::onMouseMove(View &view, double x, double y)
{
    if (!m_hasLastCursor)
    {
        m_lastX = x;
        m_lastY = y;
        m_hasLastCursor = true;
        return;
    }

    const double dx = x - m_lastX;
    const double dy = y - m_lastY;

    Camera &camera = view.getViewport().getCamera();
    const double width = std::max(1.0, view.getViewport().getWidth());
    const double height = std::max(1.0, view.getViewport().getHeight());

    camera.fpsLook(static_cast<float>(-dx / width * 3.14159265),
                   static_cast<float>(-dy / height * 3.14159265));

    m_lastX = x;
    m_lastY = y;
}

void FpsCameraOperator::moveCamera(View &view)
{
    Camera &camera = view.getViewport().getCamera();
    const float moveStep =
        0.25f * static_cast<float>(camera.distanceFromEyeToTarget());
    const glm::vec3 forward = camera.calcForward();
    const glm::vec3 right = camera.calcRight();

    if (m_moveForward)
        camera.translate(forward * moveStep);
    if (m_moveBack)
        camera.translate(-forward * moveStep);
    if (m_moveLeft)
        camera.translate(-right * moveStep);
    if (m_moveRight)
        camera.translate(right * moveStep);
}

bool FpsCameraOperator::consumesKey(KeyCode key) const
{
    return key == KeyCode::KeyUp || key == KeyCode::KeyDown ||
           key == KeyCode::KeyLeft || key == KeyCode::KeyRight;
}

void FpsCameraOperator::onKeyboardInput(View &view, KeyCode key, Action action,
                                        Modifier mods)
{
    (void)view;
    (void)mods;

    const bool isPressed = action == Action::Press || action == Action::Repeat;
    const bool isReleased = action == Action::Release;

    switch (key)
    {
    case KeyCode::KeyUp:
        if (isPressed)
            m_moveForward = true;
        if (isReleased)
            m_moveForward = false;
        break;
    case KeyCode::KeyDown:
        if (isPressed)
            m_moveBack = true;
        if (isReleased)
            m_moveBack = false;
        break;
    case KeyCode::KeyLeft:
        if (isPressed)
            m_moveLeft = true;
        if (isReleased)
            m_moveLeft = false;
        break;
    case KeyCode::KeyRight:
        if (isPressed)
            m_moveRight = true;
        if (isReleased)
            m_moveRight = false;
        break;
    default:
        break;
    }
}
