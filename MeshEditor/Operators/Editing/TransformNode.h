#pragma once

#include "ManipulatorOperator.h"

class Node;

class TransformNodeOperator : public ManipulatorOperator
{
public:
    void onEnter(View &view) override;
    void onExit(View &view) override;
    void onUpdate(View &view) override;

    void onMouseMove(View &view, double x, double y) override;
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override;

private:
    Node *m_targetNode = nullptr;

    void attachToNode(View &view, Node *node);
    void cleanup(View &view);
};

using TransformMeshOperator = TransformNodeOperator;
