#include "ToggleMeshFlagOperator.h"
#include "ToggleAABB.h"
#include "ToggleBlackEdges.h"
#include "ToggleOctree.h"

template <bool (Mesh::*Getter)() const, void (Mesh::*Setter)(bool)>
void ToggleMeshFlagOperator<Getter, Setter>::onKeyboardInput(
    View &view, KeyCode key, Action action, Modifier mods)
{
    (void)key;
    (void)mods;

    if (action != Action::Press)
        return;

    if (Model *model = view.getModel())
    {
        // Skip manipulator gizmos: debug overlays (black edges, AABB,
        // octree) belong to the edited node meshes only, never the widgets.
        model->forEachMeshRecursive(
            [](const Node *node, Mesh *mesh)
            {
                if (node->isManipulator())
                    return;
                (mesh->*Setter)(!(mesh->*Getter)());
            });
    }
}

// Closed set of instantiations — one per Toggle*Operator alias.
template class ToggleMeshFlagOperator<&Mesh::getRenderMeshAABB,
                                      &Mesh::setRenderMeshAABB>;
template class ToggleMeshFlagOperator<&Mesh::getRenderBlackEdges,
                                      &Mesh::setRenderBlackEdges>;
template class ToggleMeshFlagOperator<&Mesh::getRenderOctreeBB,
                                      &Mesh::setRenderOctreeBB>;
