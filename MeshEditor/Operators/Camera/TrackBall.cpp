#include "TrackBall.h"
#include "View.h"

void TrackBall::onEnter(View &view) {}

void TrackBall::onExit(View &view) {}

void TrackBall::onMouseInput(View &view, ButtonCode button, Action action,
                             Modifier mods, double x, double y)
{
    if (button == ButtonCode::MouseButtonRight)
    {
        if (action == Action::Press)
        {
            m_lastX = x;
            m_lastY = y;
            m_active = true;
        }
        else if (action == Action::Release)
        {
            m_active = false;
        }
    }
}

void TrackBall::onMouseMove(View &view, double x, double y)
{
    if (!m_active)
        return;

    Viewport &viewport = view.getViewport();
    double ar = viewport.calcAspectRatio();

    auto toNdc = [&](double mx, double my)
    {
        const float ndcX =
            static_cast<float>(-(mx / viewport.getWidth() * 2.0 - 1.0) * ar);
        const float ndcY =
            static_cast<float>(my / viewport.getHeight() * 2.0 - 1.0);
        return Camera::mapToTrackball(ndcX, ndcY);
    };

    viewport.getCamera().orbitArcball(toNdc(m_lastX, m_lastY), toNdc(x, y));

    m_lastX = x;
    m_lastY = y;
}