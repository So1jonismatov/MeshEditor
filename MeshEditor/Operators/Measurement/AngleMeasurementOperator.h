#pragma once

#include "Operator.h"
#include "Mesh.h"
#include "Node.h"
#include <glm/glm.hpp>

class AngleMeasurementOperator : public Operator
{
public:
    void onEnter(View &view) override;
    void onExit(View &view) override;
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override;
    
    bool isExclusiveTool() const override { return true; }
    bool consumesMouseInput(ButtonCode button) const override { return button == ButtonCode::MouseButtonLeft; }

private:
    struct SelectedFace {
        Mesh* mesh = nullptr;
        Node* node = nullptr;
        FaceHandle fh{-1};
        glm::vec3 normal{0.0f};
    };

    SelectedFace m_firstFace;
    bool m_hasFirst = false;

    glm::vec3 computeWorldNormal(Mesh* mesh, FaceHandle fh, Node* node);
};
