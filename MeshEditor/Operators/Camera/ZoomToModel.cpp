#include "ZoomToModel.h"

#include "View.h"

static bool collectModelBounds(const Model &model, glm::vec3 &globalMin,
                               glm::vec3 &globalMax)
{
    bool hasMesh = false;

    model.forEachMeshRecursive(
        [&](const Node *node, Mesh *mesh)
        {
            const glm::mat4 absTrf = node->calcAbsoluteTransform();
            const bbox worldBox =
                mesh->getBoundingBox().transformed(absTrf);
            globalMin = glm::min(globalMin, worldBox.min);
            globalMax = glm::max(globalMax, worldBox.max);
            hasMesh = true;
        });

    return hasMesh;
}

void ZoomToModelOperator::onKeyboardInput(View &view, KeyCode key,
                                          Action action, Modifier mods)
{
    (void)key;
    (void)mods;

    if (action == Action::Press)
    {
        Model *model = view.getModel();
        if (!model)
            return;

        glm::vec3 globalMin(std::numeric_limits<float>::max());
        glm::vec3 globalMax(-std::numeric_limits<float>::max());
        if (collectModelBounds(*model, globalMin, globalMax))
        {
            view.getViewport().zoomToFit(globalMin, globalMax);
        }
    }
}
