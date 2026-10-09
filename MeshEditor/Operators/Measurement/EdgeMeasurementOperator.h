#pragma once
 
#include "Operator.h"
#include "Mesh.h"
#include "Node.h"
#include <glm/glm.hpp>

class EdgeMeasurementOperator : public Operator
{
public:
    void onEnter(View &view) override;
    void onExit(View &view) override;
    void onUpdate(View &view) override;
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override;
    
    bool isExclusiveTool() const override { return true; }
    bool consumesMouseInput(ButtonCode button) const override { return button == ButtonCode::MouseButtonLeft; }

private:
    struct SelectedEdge {
        Mesh* mesh = nullptr;
        Node* node = nullptr;
        glm::vec3 worldStart{0.0f};
        glm::vec3 worldEnd{0.0f};
    };

    SelectedEdge m_edge;
    bool m_hasEdge = false;

    void updatePolyline(View &view);
};
