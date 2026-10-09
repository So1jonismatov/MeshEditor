#pragma once

#include "../Operator.h"

class DecimateOperator : public Operator
{
public:
    explicit DecimateOperator(double percent = 50.0) : m_percent(percent) {}

    void setPercentage(double percent) { m_percent = percent; }

    void onEnter(View &view) override;
    void onExit(View &view) override {}
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override {}

    bool isExclusiveTool() const override { return false; }

private:
    double m_percent = 50.0;
};
