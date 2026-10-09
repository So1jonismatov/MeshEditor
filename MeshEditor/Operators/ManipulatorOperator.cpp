#include "ManipulatorOperator.h"
#include "View.h"
#include "Contact.h"
#include "Node.h"
#include "Manipulator.h"
#include "Application.h"
#include "Model/Graph/Model.h"
#include "utils/MathUtils.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <functional>

ManipulatorOperator::~ManipulatorOperator()
{
    detachManipulator();
}

bool ManipulatorOperator::isExclusiveTool() const
{
    return true;
}

void ManipulatorOperator::attachGizmo(Node *parent, std::unique_ptr<Node> gizmo,
                                      const glm::mat4 &localPose)
{
    detachManipulator();
    gizmo->setRelativeTransform(localPose);
    m_manipulatorNode = gizmo.get();
    parent->attachNode(std::move(gizmo));
}

void ManipulatorOperator::detachManipulator()
{
    if (!m_manipulatorNode)
        return;

    // During destruction / document close the application or model may already
    // be torn down.  Guard every dereference so we never chase a dangling ptr.
    Application *appInst = Application::getInstance();
    Model *model = appInst ? appInst->getModel() : nullptr;

    bool alive = false;
    if (model)
    {
        std::function<bool(Node *)> checkNode = [&](Node *n) -> bool
        {
            if (n == m_manipulatorNode)
                return true;
            for (const auto &child : n->getChildren())
            {
                if (checkNode(child.get()))
                    return true;
            }
            return false;
        };
        for (const auto &n : model->getNodes())
        {
            if (checkNode(n.get()))
            {
                alive = true;
                break;
            }
        }
    }

    if (alive)
    {
        Node *parent = m_manipulatorNode->getParent();
        if (parent)
        {
            // Extract and immediately destroy the gizmo subtree.
            std::unique_ptr<Node> extracted =
                parent->detachChild(m_manipulatorNode);
        }
    }

    m_manipulatorNode = nullptr;
    m_activeManipulatorPart = nullptr;
}

// for constant size gizmos
void ManipulatorOperator::updateGizmoTransform(View &view)
{
    if (!m_manipulatorNode)
        return;
    // get parents absolute transform
    Node *parent = m_manipulatorNode->getParent();
    const glm::mat4 parentAbs =
        parent ? parent->calcAbsoluteTransform() : glm::mat4(1.0f);

    // get the gizmos world origin
    const glm::mat4 rel = m_manipulatorNode->getRelativeTransform();

    const glm::vec3 worldPos =
        glm::vec3(parentAbs * glm::vec4(glm::vec3(rel[3]), 1.0f));

    // get the world x,y,z rotation matrix with normalizations
    // Gramm-Shmidt like process
    const glm::mat3 raw = glm::mat3(parentAbs) * glm::mat3(rel);

    const glm::vec3 axisX = Utils::normalizedOr(raw[0], glm::vec3(1, 0, 0));
    glm::vec3 axisY = raw[1] - axisX * glm::dot(axisX, raw[1]);

    axisY = Utils::normalizedOr(axisY, glm::vec3(0, 1, 0));

    const glm::vec3 axisZ = glm::cross(axisX, axisY);
    const glm::mat3 worldRot(axisX, axisY, axisZ);

    // calculate the camera dependent scale
    const Viewport &vp = view.getViewport();
    const float dist = glm::length(worldPos - vp.getCamera().getEye());
    const float fov = static_cast<float>(vp.getFov());
    // 12 percent of the entire window
    const float screenScale =
        0.12f * (vp.isParallelProjection()
                     ? static_cast<float>(vp.calcTargetPlaneHeight())
                     : 2.0f * dist * std::tan(glm::radians(fov / 2.0f)));
    // calculate the needed world transformation matrix
    const glm::mat4 desiredWorld =
        glm::translate(glm::mat4(1.0f), worldPos) * glm::mat4(worldRot) *
        glm::scale(glm::mat4(1.0f), glm::vec3(screenScale));

    m_manipulatorNode->setRelativeTransform(glm::inverse(parentAbs) *
                                            desiredWorld);
}

bool ManipulatorOperator::beginManipulatorDrag(View &view, double x, double y)
{
    if (!m_manipulatorNode)
        return false;

    std::vector<Contact> hits = view.raycast(x, y, FilterValue::Manipulator);
    if (hits.empty())
        return false;

    auto manip = dynamic_cast<Manipulator *>(hits.front().node);
    if (!manip)
        return false;

    // Accept either the gizmo root itself or one of its direct sub-handles.
    if (manip != m_manipulatorNode && manip->getParent() != m_manipulatorNode)
        return false;

    m_state = State::Edit;
    m_activeManipulatorPart = manip;
    m_activeManipulatorPart->handleMovement(MovementType::Push,
                                            view.getViewport(), x, y);
    return true;
}

void ManipulatorOperator::updateManipulatorDrag(View &view, double x, double y)
{
    if (m_state == State::Edit && m_activeManipulatorPart)
        m_activeManipulatorPart->handleMovement(MovementType::Drag,
                                                view.getViewport(), x, y);
}

bool ManipulatorOperator::endManipulatorDrag(View &view, double x, double y)
{
    if (m_state != State::Edit || !m_activeManipulatorPart)
        return false;

    m_activeManipulatorPart->handleMovement(MovementType::Release,
                                            view.getViewport(), x, y);
    m_state = State::Idle;
    m_activeManipulatorPart = nullptr;
    return true;
}
