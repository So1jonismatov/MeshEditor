#include "SelectFaces.h"

#include "View.h"
#include "Contact.h"
#include "FilterValue.h"

#include <optional>

void SelectFacesOperator::selectFaceAt(View &view, double x, double y,
                                       bool accumulate)
{
    // pickFace dispatches to the octree raycast or the FBO colour-id pass
    // depending on the active pick mode (set via the sidebar radio group).
    std::optional<Contact> hit = view.pickFace(x, y);
    if (!hit)
    {
        if (!accumulate)
        {
            if (Model *model = view.getModel())
            {
                model->forEachMeshRecursive([](Mesh *mesh)
                                            { mesh->clearSelectedFace(); });
            }
        }
        return;
    }

    Node *node = hit->node;
    if (!node)
        return;

    Mesh *mesh = node->getMesh();
    if (!mesh)
        return;

    if (!accumulate)
    {
        if (Model *model = view.getModel())
        {
            model->forEachMeshRecursive([](Mesh *otherMesh)
                                        { otherMesh->clearSelectedFace(); });
        }
        mesh->setSelectedFace(hit->face);
    }
    else
    {
        mesh->addSelectedFace(hit->face);
    }
}

void SelectFacesOperator::onMouseInput(View &view, ButtonCode button,
                                       Action action, Modifier mods, double x,
                                       double y)
{

    (void)button;

    if (action == Action::Press)
    {
        m_isDragging = true;
        m_accumulateSelection =
            (static_cast<int>(mods) & static_cast<int>(Modifier::Shift)) != 0;

        selectFaceAt(view, x, y, m_accumulateSelection);

        m_lastX = x;
        m_lastY = y;
        return;
    }

    if (action != Action::Release)
        return;

    m_isDragging = false;
    m_accumulateSelection = false;
}

void SelectFacesOperator::onMouseMove(View &view, double x, double y)
{
    if (!m_isDragging || !m_accumulateSelection)
        return;

    if (x == m_lastX && y == m_lastY)
        return;

    selectFaceAt(view, x, y, true);
    m_lastX = x;
    m_lastY = y;
}