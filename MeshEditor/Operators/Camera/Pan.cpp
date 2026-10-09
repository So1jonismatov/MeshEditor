#include "Pan.h"
#include "View.h"
#include "FilterValue.h"
#include "Contact.h"

void Pan::onEnter(View &view) {}

void Pan::onExit(View &view) {}

void Pan::onMouseInput(View &view, ButtonCode button, Action action,
                       Modifier mods, double x, double y)
{
    if (button == ButtonCode::MouseButtonLeft)
    {
        if (action == Action::Press)
        {
            if (!view.raycast(x, y, FilterValue::Manipulator).empty())
            {
                m_active = false;
                return;
            }

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

void Pan::onMouseMove(View &view, double x, double y)
{
    if (!m_active)
        return;

    double dx = x - m_lastX;
    double dy = y - m_lastY;

    Viewport &viewport = view.getViewport();
    viewport.getCamera().pan(
        -dx * viewport.calcTargetPlaneWidth() / viewport.getWidth(),
        dy * viewport.calcTargetPlaneHeight() / viewport.getHeight());

    m_lastX = x;
    m_lastY = y;
}