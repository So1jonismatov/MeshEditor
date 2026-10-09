#pragma once

#include "../Operator.h"
#include <memory>

class RemoveDegenerateFacesOperator : public Operator
{
public:
    void onEnter(View &view) override;
    void onExit(View &view) override {}
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override {}

    bool isExclusiveTool() const override { return false; }
};
