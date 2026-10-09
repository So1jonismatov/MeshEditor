#pragma once

#include "ManipulatorOperator.h"

class Node;

class ScaleNodeOperator : public ManipulatorOperator
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

    // Build the scale triad on `node` (viewport pick or sidebar selection).
    // Anchors at the centre of the subtree's world bounds, so group nodes
    // without a mesh of their own work too. No-op when already attached.
    void attachToNode(View &view, Node *node);
    void cleanup(View &view);
};
