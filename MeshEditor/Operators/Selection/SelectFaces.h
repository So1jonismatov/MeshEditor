#pragma once

#include "Operator.h"
#include <HalfEdge.h>

class Mesh;
class Node;

class SelectFacesOperator : public Operator
{
public:
    void onMouseInput(View &view, ButtonCode button, Action action,
                      Modifier mods, double x, double y) override;
    void onMouseMove(View &view, double x, double y) override;

private:
    bool m_isDragging = false;
    bool m_accumulateSelection = false;
    double m_lastX = 0.0;
    double m_lastY = 0.0;

    void selectFaceAt(View &view, double x, double y, bool accumulate);
};