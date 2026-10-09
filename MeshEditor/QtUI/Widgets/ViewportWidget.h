#pragma once

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QPointF>
#include <QTimer>

#include <vector>

#include "../../Interfaces/Keys.h"

class Application;
class NavGizmoWidget;
class MeasurementCardWidget;
class QLabel;
class QtWindow;
class View;

// Hosts the engine's 3D view inside a Qt OpenGL widget. On first GL context
// creation it builds the engine View (with a QtWindow adapter) and afterwards
// forwards all Qt input events into the engine's operator dispatcher.
class ViewportWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit ViewportWidget(Application &app, QWidget *parent = nullptr);
    ~ViewportWidget() override;

    // Simulates the keyboard shortcut of an existing operator (used by the
    // sidebar buttons and menu entries).
    void simulateKey(KeyCode key);

    void setCursorCaptured(bool captured);

    // The engine view backing this widget (created on the first GL context).
    // Null until initializeGL has run. Used by MainWindow to snapshot/restore
    // per-tab camera state and by the nav gizmo to read/mutate the camera.
    View *view() const;

signals:
    void viewCreated();
    // A key reached the operator dispatcher, so an enter/exit tool may have
    // toggled — MainWindow re-reads the engine state to sync tool buttons.
    void toolStateMaybeChanged();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void resizeEvent(QResizeEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateFpsOverlay();
    void positionOverlays(); // keeps the nav gizmo pinned top-right
    void wakeRendering();
    bool needsContinuousRendering() const;
    void onFrameTick();

    // FBO colour-id picking reads back the framebuffer, which is only valid
    // while this widget's GL context is current and its framebuffer is bound —
    // a guarantee Qt gives only inside paintGL. So in FBO pick mode we don't
    // run the pick from the event handler; we queue the mouse action here and
    // replay it at the top of the next paintGL (the tutorial's PickingPhase
    // before RenderPhase). Octree mode needs no GL and injects immediately.
    struct PendingMouse
    {
        enum Kind
        {
            Press,
            Release,
            Move
        } kind;
        ButtonCode button;
        Action action;
        Modifier mods;
        double x;
        double y;
    };
    std::vector<PendingMouse> m_pendingMouse;
    bool wantsDeferredMouse() const; // true only in FBO pick mode
    void drainPendingMouse();        // called from paintGL, context current

    Application &m_app;
    QtWindow *m_adapter = nullptr; // owned by m_view
    View *m_view = nullptr;        // owned by Application
    QTimer m_frameTimer;

    QLabel *m_fpsLabel = nullptr;         // corner FPS overlay
    NavGizmoWidget *m_navGizmo = nullptr; // top-right navigation cube
    MeasurementCardWidget *m_measurementCard = nullptr; // floating measurement card
    QElapsedTimer m_fpsClock;
    QElapsedTimer m_activityTimer;
    int m_framesSinceUpdate = 0;
    double m_engineMsAccum = 0.0; // CPU time spent inside View::update()
    bool m_mouseButtonPressed = false;

    // Cursor-capture emulation for the FPS camera: the OS cursor is hidden
    // and re-centered every move, while an unbounded virtual position is fed
    // to the engine so look deltas keep accumulating.
    bool m_cursorCaptured = false;
    bool m_skipNextMove = false;
    QPointF m_virtualCursor;
};
