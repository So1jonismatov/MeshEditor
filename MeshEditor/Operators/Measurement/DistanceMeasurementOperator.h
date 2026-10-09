#pragma once
 
#include "Operator.h"
#include "Mesh.h"
#include "Node.h"
#include <glm/glm.hpp>
#include <vector>

class DistanceMeasurementOperator : public Operator
{
public:
    void onEnter(View &view) override;
    void onExit(View &view) override;
    void onUpdate(View &view) override;
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override;
    void onMouseMove(View &view, double x, double y) override;
    bool isExclusiveTool() const override { return true; }
    bool consumesMouseInput(ButtonCode button) const override { return button == ButtonCode::MouseButtonLeft; }

private:
    struct Point {
        Mesh* mesh = nullptr;
        Node* node = nullptr;
        glm::vec3 worldPos{0.0f};
    };

    bool m_active = false;
    bool m_hasMeasurement = false;
    Point m_startPoint;
    Point m_endPoint;

    void updatePolyline(View &view, bool show);
    bool raycastPoint(View &view, double x, double y, Point &outPoint);
};
