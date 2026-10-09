#pragma once
#include "Operator.h"

class Pan : public Operator
{
public:
    void onEnter(View &view) override;
    void onExit(View &view) override;
    void onMouseMove(View &view, double x, double y) override;
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override;

private:
    double m_lastX = 0.0;
    double m_lastY = 0.0;
    bool m_active = false;
};