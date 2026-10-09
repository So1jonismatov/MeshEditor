#pragma once
#include "Operator.h"
#include <glm/glm.hpp>
#include <memory>

class Node;
class Manipulator;

class ManipulatorOperator : public Operator
{
public:
    virtual ~ManipulatorOperator();
    bool isExclusiveTool() const override;

protected:
    enum class State
    {
        Idle,
        Edit
    };
    State m_state = State::Idle;

    Node *m_manipulatorNode = nullptr;
    Manipulator *m_activeManipulatorPart = nullptr;

    void attachGizmo(Node *parent, std::unique_ptr<Node> gizmo,
                     const glm::mat4 &localPose);
    void detachManipulator();

    void updateGizmoTransform(View &view);

    bool beginManipulatorDrag(View &view, double x, double y);
    void updateManipulatorDrag(View &view, double x, double y);

    bool endManipulatorDrag(View &view, double x, double y);
};
