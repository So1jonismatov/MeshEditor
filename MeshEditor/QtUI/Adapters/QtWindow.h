#pragma once

#include <QPointer>

#include "../../Interfaces/IWindow.h"

class ViewportWidget;

// Adapts the Qt viewport widget to the engine's IWindow interface. Owned by
// the View (which deletes it); the widget itself stays owned by the Qt
// widget hierarchy.
class QtWindow : public IWindow
{
public:
    explicit QtWindow(ViewportWidget *widget);

    unsigned int getWidth() const override;
    unsigned int getHeight() const override;

    void setCursorCaptured(bool captured) override;

    void setKeyCallback(const KeyCallback &callback) override;
    void setCursorPosCallback(const CursorPosCallback &callback) override;
    void setMouseCallback(const MouseCallback &callback) override;
    void setScrollCallback(const ScrollCallback &callback) override;

    bool openFileDialog(std::string &path) override;
    bool saveFileDialog(std::string &path) override;

    // Event injection: called by the widget's Qt event handlers, and by UI
    // buttons that simulate the keyboard shortcuts of existing operators.
    void injectKey(KeyCode key, Action action, Modifier mods);
    void injectCursorPos(double x, double y);
    void injectMouse(ButtonCode button, Action action, Modifier mods, double x,
                     double y);
    void injectScroll(double xoffset, double yoffset);

private:
    // QPointer, not a raw pointer: the widget is owned by the Qt widget
    // hierarchy and is destroyed before Application/View teardown runs. A raw
    // pointer would dangle and crash when late shutdown code (e.g. an
    // operator's onExit releasing the cursor capture) called into it.
    QPointer<ViewportWidget> m_widget;

    KeyCallback m_keyCallback;
    CursorPosCallback m_cursorPosCallback;
    MouseCallback m_mouseCallback;
    ScrollCallback m_scrollCallback;
};
