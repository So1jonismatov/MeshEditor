#include "ColorHoles.h"

#include "Application.h"
#include "View.h"
#include "Threading/TaskRunner.h"

void ColorHolesOperator::onKeyboardInput(View &view, KeyCode key, Action action,
                                         Modifier mods)
{
    (void)key;
    (void)mods;

    if (action != Action::Press)
        return;

    Model *model = view.getModel();
    Application *app = Application::getInstance();
    if (!model || !app)
        return;

    runAsync(view, [model]() {
        model->forEachMeshRecursive([](Mesh *mesh) { mesh->colorHoles(); });
    });
}