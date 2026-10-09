#pragma once
#include "ManipulatorOperator.h"
#include <HalfEdge.h>

class Mesh;

class EditFaceOperator : public ManipulatorOperator
{
public:
    void onEnter(View &view) override;
    void onExit(View &view) override;
    void onUpdate(View &view) override;
    void onMouseMove(View &view, double x, double y) override;
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override;

private:
    Mesh *m_targetMesh = nullptr;
    FaceHandle m_targetFace{-1};

    void cleanup(View &view);
};
