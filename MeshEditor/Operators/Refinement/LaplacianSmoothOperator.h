#pragma once

#include "../Operator.h"
#include <memory>

class LaplacianSmoothOperator : public Operator
{
public:
    explicit LaplacianSmoothOperator(int passes = 3, float lambda = 0.5f)
        : m_passes(passes), m_lambda(lambda) {}

    void setParameters(int passes, float lambda)
    {
        m_passes = passes;
        m_lambda = lambda;
    }

    void onEnter(View &view) override;
    void onExit(View &view) override {}
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override {}

    bool isExclusiveTool() const override { return false; }

private:
    int m_passes = 3;
    float m_lambda = 0.5f;
};
