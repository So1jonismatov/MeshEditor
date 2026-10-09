#include "ScaleNode.h"
#include "Application.h"
#include "View.h"
#include "Contact.h"
#include "Triad.h"
#include <glm/gtc/matrix_transform.hpp>

namespace
{
// World-space bounds of a node's subtree, skipping gizmo helper nodes.
void expandSubtreeWorldBox(const Node *node, bbox &box, bool &any)
{
    if (!node || node->isManipulator())
        return;
    if (const Mesh *mesh = node->getMesh())
    {
        const bbox worldBox =
            mesh->getBoundingBox().transformed(node->calcAbsoluteTransform());
        if (any)
        {
            box.expand(worldBox);
        }
        else
        {
            box = worldBox;
            any = true;
        }
    }
    for (const auto &child : node->getChildren())
        expandSubtreeWorldBox(child.get(), box, any);
}
} // namespace

void ScaleNodeOperator::onEnter(View &view)
{
    m_state = State::Idle;

    // A node picked in the sidebar tree gets its triad immediately — no
    // viewport click needed.
    if (Application *app = Application::getInstance())
        attachToNode(view, app->getSelectedNode());
}

void ScaleNodeOperator::onExit(View &view)
{
    cleanup(view);
}

void ScaleNodeOperator::cleanup(View &view)
{
    detachManipulator();
    m_targetNode = nullptr;
    m_state = State::Idle;
}

void ScaleNodeOperator::onUpdate(View &view)
{
    // Follow the sidebar selection while the tool is active (never mid-drag:
    // the gizmo under the cursor must stay put until release).
    if (m_state == State::Idle)
    {
        if (Application *app = Application::getInstance())
        {
            Node *selected = app->getSelectedNode();
            if (selected != m_targetNode)
            {
                if (selected)
                    attachToNode(view, selected);
                else
                    cleanup(view);
            }
        }
    }

    updateGizmoTransform(view);
}

void ScaleNodeOperator::onMouseMove(View &view, double x, double y)
{
    updateManipulatorDrag(view, x, y);
}

void ScaleNodeOperator::attachToNode(View &view, Node *node)
{
    if (!node || node == m_targetNode)
        return;

    cleanup(view);
    m_targetNode = node;

    const glm::mat4 targetAbs = m_targetNode->calcAbsoluteTransform();

    // Anchor the triad at the centre of the subtree's world bounds; a node
    // with no meshes anywhere below falls back to its own origin.
    bbox worldBox;
    bool hasBox = false;
    expandSubtreeWorldBox(m_targetNode, worldBox, hasBox);
    const glm::vec3 worldCenter =
        hasBox ? worldBox.center() : glm::vec3(targetAbs[3]);
    const glm::vec3 localCenter =
        glm::vec3(glm::inverse(targetAbs) * glm::vec4(worldCenter, 1.0f));

    auto triad = std::make_unique<ScaleTriad>(1.5f);
    triad->setCallback(
        [this](const glm::mat4 &deltaMatrix)
        {
            if (!m_targetNode)
                return;
            const glm::mat4 T_center =
                m_manipulatorNode->getRelativeTransform();
            const glm::mat4 targetDelta =
                T_center * deltaMatrix * glm::inverse(T_center);
            m_targetNode->applyRelativeTransform(targetDelta);
        });

    attachGizmo(m_targetNode, std::move(triad),
                glm::translate(glm::mat4(1.0f), localCenter));
}

void ScaleNodeOperator::onMouseInput(View &view, ButtonCode button,
                                     Action action, Modifier mods, double x,
                                     double y)
{
    if (button != ButtonCode::MouseButtonLeft)
        return;

    if (action == Action::Press)
    {
        if (m_state == State::Idle)
        {
            if (beginManipulatorDrag(view, x, y))
                return;

            Application *app = Application::getInstance();

            std::vector<Contact> nodeContacts =
                view.raycast(x, y, FilterValue::Node);
            if (nodeContacts.empty())
            {
                cleanup(view);
                if (app)
                    app->setSelectedNode(nullptr);
                return;
            }

            Node *hitNode = nodeContacts.front().node;
            attachToNode(view, hitNode);
            // Keep the sidebar tree + highlight in sync with viewport picking.
            if (app)
                app->setSelectedNode(hitNode);
        }
    }
    else if (action == Action::Release)
    {
        endManipulatorDrag(view, x, y);
    }
}
