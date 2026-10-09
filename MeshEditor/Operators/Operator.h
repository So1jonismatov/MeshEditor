#pragma once
#include "Keys.h"

#include <functional>

class View;

class Operator
{
public:
    virtual ~Operator();
    virtual void onEnter(View &);
    virtual void onExit(View &);
    virtual void onUpdate(View &);
    virtual void onMouseMove(View &view, double x, double y);
    virtual void onMouseInput(View &view, ButtonCode button, Action action,
                              Modifier mods, double x, double y);
    virtual void onKeyboardInput(View &view, KeyCode key, Action action,
                                 Modifier mods);

    // Exclusive tools (the manipulator gizmos) cannot be active at the same
    // time: entering one exits any other. Non-tool operators return false.
    virtual bool isExclusiveTool() const;

    // While ACTIVE, an enter-exit operator can claim a key for itself: the
    // dispatcher then routes that key's events straight to it instead of the
    // globally bound single-key operator (e.g. FPS mode owns the arrow keys
    // for movement, overriding the arrow-key camera orbit).
    virtual bool consumesKey(KeyCode key) const;

    // Same for mouse buttons.
    virtual bool consumesMouseInput(ButtonCode button) const;

protected:
    void runAsync(View &view, std::function<void()> backgroundWork, std::function<void()> mainThreadCallback = nullptr);
};