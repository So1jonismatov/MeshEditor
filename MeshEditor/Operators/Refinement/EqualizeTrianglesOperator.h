#pragma once

#include "../Operator.h"

class EqualizeTrianglesOperator : public Operator
{
public:
    explicit EqualizeTrianglesOperator(float targetEdgeLength = 0.0f, int iterations = 3);

    void setParameters(float targetEdgeLength, int iterations)
    {
        m_targetEdgeLength = targetEdgeLength;
        m_iterations = iterations;
    }

    void onEnter(View &view) override;
    void onExit(View &view) override {}
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override {}

    bool isExclusiveTool() const override { return false; }

private:
    float m_targetEdgeLength = 0.0f; // 0 = auto-compute mean edge length
    int m_iterations = 3;
};
