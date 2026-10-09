#include "PaintBoundaryFaces.h"

#include "Application.h"
#include "View.h"
#include "Threading/TaskRunner.h"
#include <unordered_set>

void PaintBoundaryFacesOperator::onKeyboardInput(View &view, KeyCode key,
                                                 Action action, Modifier mods)
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
        std::unordered_set<Geometry *> visited;
        model->forEachMeshRecursive([&visited](Mesh *mesh) {
            if (!visited.insert(mesh->getGeometry()).second)
                return;
            mesh->paintBoundaryFaces();
        });
    });
}