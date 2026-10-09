#include "KeyboardOrbit.h"

#include "View.h"
#include <glm/gtc/constants.hpp>

KeyboardOrbitOperator::KeyboardOrbitOperator(
    Camera::OrbitStepDirection direction)
    : m_direction(direction)
{
}

void KeyboardOrbitOperator::onKeyboardInput(View &view, KeyCode key,
                                            Action action, Modifier mods)
{
    (void)key;
    (void)mods;

    if (action != Action::Press && action != Action::Repeat)
        return;

    Camera &camera = view.getViewport().getCamera();

    // Held key: auto-repeat fires far faster than the step animation, and
    // orbitStepAnimated snap-finishes a running step before starting the
    // next — unthrottled repeats would spin the camera at the repeat rate.
    // Let the in-flight step play out; the repeat arriving right after it
    // ends chains the next step, so holding gives continuous motion.
    if (action == Action::Repeat && camera.isTransitioning())
        return;

    constexpr float kStep = glm::pi<float>() / 12.0f; // 15 degrees
    constexpr double kAnimSec = 0.5;
    camera.orbitStepAnimated(m_direction, kStep, kAnimSec);
}
