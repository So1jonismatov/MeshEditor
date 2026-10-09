#pragma once

#include "Operator.h"

class SaveSceneOperator : public Operator
{
public:
    void onKeyboardInput(View &view, KeyCode key, Action action,
                         Modifier mods) override;
};
