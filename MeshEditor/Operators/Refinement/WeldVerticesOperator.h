#pragma once

#include "../Operator.h"

class WeldVerticesOperator : public Operator
{
public:
    explicit WeldVerticesOperator(float epsilon = 0.001f) : m_epsilon(epsilon) {}

    void setTolerance(float epsilon) { m_epsilon = epsilon; }

    void onEnter(View &view) override;
    void onExit(View &view) override {}
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override {}

    bool isExclusiveTool() const override { return false; }

private:
    float m_epsilon = 0.001f;
};
