#pragma once

#include "../Operator.h"

class SubdivideOperator : public Operator
{
public:
    explicit SubdivideOperator(int levels = 1, bool onlySelected = false);

    void setParameters(int levels, bool onlySelected)
    {
        m_levels = levels;
        m_onlySelected = onlySelected;
    }

    void onEnter(View &view) override;
    void onExit(View &view) override {}
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override {}

    bool isExclusiveTool() const override { return false; }

private:
    int m_levels = 1;
    bool m_onlySelected = false;
};
