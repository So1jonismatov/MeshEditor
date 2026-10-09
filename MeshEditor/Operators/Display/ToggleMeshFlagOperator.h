#pragma once

#include "Operator.h"
#include "View.h"
#include "Mesh.h"
#include "Node.h"

// Definition lives in ToggleMeshFlagOperator.cpp, explicitly instantiated
// there for the three concrete Getter/Setter pairs used by ToggleAABB.h /
// ToggleBlackEdges.h / ToggleOctree.h — that closed set means the template
// body doesn't need to stay visible in this header.
template <bool (Mesh::*Getter)() const, void (Mesh::*Setter)(bool)>
class ToggleMeshFlagOperator : public Operator
{
public:
    void onKeyboardInput(View &view, KeyCode key, Action action,
                         Modifier mods) override;
};
