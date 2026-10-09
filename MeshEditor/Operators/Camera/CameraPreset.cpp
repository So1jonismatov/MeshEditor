#include "CameraPreset.h"

#include "View.h"

CameraPresetOperator::CameraPresetOperator(Preset preset) : m_preset(preset) {}

void CameraPresetOperator::onKeyboardInput(View &view, KeyCode key,
                                           Action action, Modifier mods)
{
    (void)key;
    (void)mods;

    if (action != Action::Press)
        return;

    Camera &camera = view.getViewport().getCamera();
    // Compute the preset pose on a copy, then animate the live camera into it
    // (0.5 s, ticked from View::update, same as the navigation gizmo) instead
    // of snapping.
    Camera goal = camera;
    switch (m_preset)
    {
    case Preset::Front:
        goal.setFrontView();
        break;
    case Preset::Rear:
        goal.setRearView();
        break;
    case Preset::Right:
        goal.setRightView();
        break;
    case Preset::Left:
        goal.setLeftView();
        break;
    case Preset::Top:
        goal.setTopView();
        break;
    case Preset::Bottom:
        goal.setBottomView();
        break;
    case Preset::Iso:
        goal.setIsoView();
        break;
    }
    camera.startTransitionTo(goal, 0.5);
}
