#include "ToggleProjection.h"

#include "View.h"

void ToggleProjectionOperator::onKeyboardInput(View &view, KeyCode key,
                                               Action action, Modifier mods)
{
    (void)key;
    (void)mods;

    if (action != Action::Press)
        return;

    Viewport &viewport = view.getViewport();
    viewport.setParallelProjection(!viewport.isParallelProjection());
}
