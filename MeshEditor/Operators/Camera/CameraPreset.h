#pragma once

#include "Operator.h"

class CameraPresetOperator : public Operator
{
public:
    enum class Preset
    {
        Front,
        Rear,
        Right,
        Left,
        Top,
        Bottom,
        Iso,
    };

    explicit CameraPresetOperator(Preset preset);

    void onKeyboardInput(View &view, KeyCode key, Action action,
                         Modifier mods) override;

private:
    Preset m_preset;
};
