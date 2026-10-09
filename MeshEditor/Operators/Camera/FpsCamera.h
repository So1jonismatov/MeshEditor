#pragma once

#include "Operator.h"

class FpsCameraOperator : public Operator
{
public:
    void onEnter(View &view) override;
    void onExit(View &view) override;
    void onUpdate(View &view) override;
    void onMouseMove(View &view, double x, double y) override;
    void onKeyboardInput(View &view, KeyCode key, Action action,
                         Modifier mods) override;

    // While FPS mode is active the arrow keys are movement keys — claim them
    // so the dispatcher doesn't hand them to the arrow-key orbit operator.
    bool consumesKey(KeyCode key) const override;

private:
    double m_lastX = 0.0;
    double m_lastY = 0.0;
    bool m_hasLastCursor = false;
    bool m_moveForward = false;
    bool m_moveBack = false;
    bool m_moveLeft = false;
    bool m_moveRight = false;

    void moveCamera(View &view);
};
