#include "ViewUtils.h"

#include "View.h"
#include "Geometry/Mesh.h"
#include "Graph/Model.h"
#include "Graph/Node.h"

bool getSceneBoundingBox(const Model *model, glm::vec3 &outMin, glm::vec3 &outMax)
{
    if (!model)
        return false;

    const bbox &box = model->getWorldBoundingBox();
    if (box.min == box.max && box.min == glm::vec3(0.0f))
        return false;

    outMin = box.min;
    outMax = box.max;
    return true;
}

void zoomViewToModel(View &view)
{
    Model *model = view.getModel();
    if (!model)
        return;

    glm::vec3 globalMin, globalMax;
    if (!getSceneBoundingBox(model, globalMin, globalMax))
        return;

    view.getViewport().zoomToFit(globalMin, globalMax);
}
