#pragma once

#include "Operator.h"
#include "Camera.h"

// Arrow-key camera orbit: each press is one animated 15° arcball step about
// the camera's own axes, identical to the navigation gizmo's arrow buttons
// (Camera::orbitStepAnimated). Rapid presses chain — a still-running step is
// snapped to its end pose before the next one starts.
class KeyboardOrbitOperator : public Operator
{
public:
    explicit KeyboardOrbitOperator(Camera::OrbitStepDirection direction);

    void onKeyboardInput(View &view, KeyCode key, Action action,
                         Modifier mods) override;

private:
    Camera::OrbitStepDirection m_direction;
};
