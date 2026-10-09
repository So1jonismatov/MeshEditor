#include "ViewportWidget.h"
#include "NavGizmoWidget.h"
#include "MeasurementCardWidget.h"
#include "Adapters/QtKeyMap.h"
#include "Adapters/QtWindow.h"

#include "../Application.h"
#include "../View/View.h"
#include "../../Interfaces/IRenderSystem.h"

#include <QCursor>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWheelEvent>

#include <iostream>

View *ViewportWidget::view() const
{
    return m_view;
}

ViewportWidget::ViewportWidget(Application &app, QWidget *parent)
    : QOpenGLWidget(parent), m_app(app)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(200, 150);

    m_fpsLabel = new QLabel("-- FPS", this);
    m_fpsLabel->setStyleSheet(
        "QLabel { color: #9ff0a8; background-color: rgba(0, 0, 0, 120);"
        " padding: 2px 6px; border-radius: 3px; font-family: monospace; }");
    m_fpsLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_fpsLabel->adjustSize();
    m_fpsLabel->move(8, 8);
    m_fpsLabel->raise();
    m_fpsClock.start();

    // Top-right navigation cube overlay (the camera/display dropdown lives in
    // the tab row, built by MainWindow).
    m_navGizmo = new NavGizmoWidget(this, this);

    // Floating HUD Measurement Card overlay
    m_measurementCard = new MeasurementCardWidget(this);

    connect(m_measurementCard, &MeasurementCardWidget::closeRequested, this, [this]() {
        if (m_view)
        {
            if (m_view->isOperatorActive(KeyCode::KeyM))
                simulateKey(KeyCode::KeyM);
            else if (m_view->isOperatorActive(KeyCode::KeyA))
                simulateKey(KeyCode::KeyA);
            else if (m_view->isOperatorActive(KeyCode::KeyU))
                simulateKey(KeyCode::KeyU);
        }
        m_app.clearMeasurementData();
    });

    m_app.setOnMeasurementChanged([this](const MeasurementData &data) {
        QMetaObject::invokeMethod(this, [this, data]() {
            if (m_measurementCard)
            {
                if (data.type != MeasurementType::None)
                {
                    m_measurementCard->updateMeasurement(data);
                    if (!m_measurementCard->isUserMoved())
                    {
                        positionOverlays();
                    }
                    m_measurementCard->show();
                    m_measurementCard->raise();
                }
                else
                {
                    m_measurementCard->clearMeasurement();
                }
            }
        }, Qt::QueuedConnection);
    });

    m_activityTimer.start();

    // Demand-driven rendering: when the scene and camera are static, the FPS
    // drops to 0.0 FPS (Idle), bringing CPU and GPU usage to 0%. When the user
    // interacts (orbit, pan, zoom, gizmo drag, key, tool hover) or animations
    // run, it instantly renders at full 60 FPS.
    connect(&m_frameTimer, &QTimer::timeout, this, &ViewportWidget::onFrameTick);
    m_frameTimer.setTimerType(Qt::PreciseTimer);
    m_frameTimer.start(16);
}

ViewportWidget::~ViewportWidget()
{
    // The widget dies before Application/View do (MainWindow is destroyed
    // first in main). Release everything that points back at this widget while
    // it is still alive, otherwise late shutdown code would call into freed
    // memory (e.g. FpsCameraOperator::onExit -> setCursorCaptured -> Qt).
    m_frameTimer.stop();
    m_app.setOnMeasurementChanged(nullptr);
    if (m_view)
        m_view->resetOperatorState();
}

void ViewportWidget::wakeRendering()
{
    m_activityTimer.restart();
    update();
}

bool ViewportWidget::needsContinuousRendering() const
{
    if (!m_view)
        return false;

    // 1. Smooth camera interpolation / transitions
    if (m_view->getViewport().getCamera().isTransitioning())
        return true;

    // 2. FPS camera mode (cursor captured)
    if (m_cursorCaptured)
        return true;

    // 3. Mouse button held down (dragging orbit / pan / zoom / gizmo)
    if (m_mouseButtonPressed)
        return true;

    // 4. Pending mouse events waiting to be drained inside paintGL
    if (!m_pendingMouse.empty())
        return true;

    // 5. Background task running on model
    if (Model *model = m_view->getModel())
    {
        if (model->isBusy())
            return true;
    }

    // 6. Recent user interaction grace period (500 ms)
    if (m_activityTimer.isValid() && m_activityTimer.elapsed() < 500)
        return true;

    return false;
}

void ViewportWidget::onFrameTick()
{
    if (needsContinuousRendering())
    {
        update();
    }
    else
    {
        // Idle: update FPS label to show 0.0 FPS · Idle
        if (m_fpsClock.elapsed() >= 500)
        {
            if (m_fpsLabel)
            {
                m_fpsLabel->setText(QStringLiteral("0.0 FPS  ·  Idle"));
                m_fpsLabel->adjustSize();
            }
            m_framesSinceUpdate = 0;
            m_engineMsAccum = 0.0;
            m_fpsClock.restart();
        }
    }
}

void ViewportWidget::positionOverlays()
{
    const int margin = 8;
    if (m_navGizmo)
    {
        m_navGizmo->move(width() - m_navGizmo->width() - margin, margin);
        m_navGizmo->raise();
    }
    if (m_measurementCard && m_measurementCard->isVisible())
    {
        if (!m_measurementCard->isUserMoved())
        {
            int top = m_navGizmo ? (m_navGizmo->y() + m_navGizmo->height() + 8) : margin;
            m_measurementCard->move(std::max(0, width() - m_measurementCard->width() - margin), top);
        }
        else
        {
            m_measurementCard->clampToParent();
        }
        m_measurementCard->raise();
    }
}

void ViewportWidget::updateFpsOverlay()
{
    ++m_framesSinceUpdate;
    const qint64 elapsed = m_fpsClock.elapsed();
    if (elapsed >= 500)
    {
        const double fps = m_framesSinceUpdate * 1000.0 / elapsed;
        // Engine ms = CPU time issuing the scene inside View::update(). If
        // FPS is low while this stays small, the frame is spent in the
        // GPU/driver/compositor, not in engine code.
        const double engineMs = m_engineMsAccum / m_framesSinceUpdate;
        m_fpsLabel->setText(QString::number(fps, 'f', 1) + " FPS  ·  CPU " +
                            QString::number(engineMs, 'f', 1) + " ms");
        m_fpsLabel->adjustSize();
        m_framesSinceUpdate = 0;
        m_engineMsAccum = 0.0;
        m_fpsClock.restart();
    }
}

void ViewportWidget::resizeEvent(QResizeEvent *event)
{
    QOpenGLWidget::resizeEvent(event);
    if (m_fpsLabel)
    {
        m_fpsLabel->move(8, 8);
        m_fpsLabel->raise();
    }
    positionOverlays();
    wakeRendering();
}

void ViewportWidget::initializeGL()
{
    if (m_view)
        return;

    m_adapter = new QtWindow(this);
    m_view = m_app.createView(m_adapter);
    if (!m_view)
    {
        std::cerr << "[Qt] Failed to create engine view" << std::endl;
        return;
    }
    emit viewCreated();
}

void ViewportWidget::resizeGL(int w, int h)
{
    if (!m_view)
        return;

    const qreal dpr = devicePixelRatioF();
    if (IRenderSystem *rs = m_app.getRenderSystem())
    {
        rs->setViewport(0.0, 0.0, w * dpr, h * dpr);
    }
    m_view->getViewport().setViewportSize(static_cast<uint32_t>(w),
                                          static_cast<uint32_t>(h));
}

void ViewportWidget::paintGL()
{
    if (!m_view)
        return;

    // PickingPhase: service any queued FBO pick now, while the context is
    // current and this widget's framebuffer is bound (endPickPass restores it).
    drainPendingMouse();

    IRenderSystem *rs = m_app.getRenderSystem();
    if (rs)
    {
        rs->clearDisplay(0.07f, 0.08f, 0.10f);
    }

    QElapsedTimer engineTimer;
    engineTimer.start();
    m_view->update();
    m_engineMsAccum = m_engineMsAccum + engineTimer.nsecsElapsed() / 1e6;

    // Keep the navigation cube in sync with the camera (orbit/pan/presets).
    if (m_navGizmo)
        m_navGizmo->update();

    updateFpsOverlay();
}

void ViewportWidget::simulateKey(KeyCode key)
{
    if (!m_adapter)
        return;
    setFocus();
    m_adapter->injectKey(key, Action::Press, Modifier::NoModifier);
    m_adapter->injectKey(key, Action::Release, Modifier::NoModifier);
    wakeRendering();
    emit toolStateMaybeChanged();
}

void ViewportWidget::setCursorCaptured(bool captured)
{
    m_cursorCaptured = captured;
    if (captured)
    {
        setCursor(Qt::BlankCursor);
        m_virtualCursor = mapFromGlobal(QCursor::pos());
        m_skipNextMove = true;
        QCursor::setPos(mapToGlobal(rect().center()));
    }
    else
    {
        unsetCursor();
    }
    wakeRendering();
}

void ViewportWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_adapter)
    {
        QOpenGLWidget::keyPressEvent(event);
        return;
    }
    const KeyCode key = qtKeyToKeyCode(event->key());
    if (key == KeyCode::KeyUnknown)
    {
        QOpenGLWidget::keyPressEvent(event);
        return;
    }
    m_adapter->injectKey(key,
                         event->isAutoRepeat() ? Action::Repeat : Action::Press,
                         qtModifiersToModifier(event->modifiers()));
    wakeRendering();
    emit toolStateMaybeChanged();
}

void ViewportWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (!m_adapter || event->isAutoRepeat())
    {
        QOpenGLWidget::keyReleaseEvent(event);
        return;
    }
    const KeyCode key = qtKeyToKeyCode(event->key());
    if (key == KeyCode::KeyUnknown)
    {
        QOpenGLWidget::keyReleaseEvent(event);
        return;
    }
    m_adapter->injectKey(key, Action::Release,
                         qtModifiersToModifier(event->modifiers()));
    wakeRendering();
}

void ViewportWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    m_mouseButtonPressed = true;
    if (!m_adapter)
        return;
    const ButtonCode button = qtButtonToButtonCode(event->button());
    const Modifier mods = qtModifiersToModifier(event->modifiers());
    const double x = event->position().x();
    const double y = event->position().y();
    // In FBO pick mode a press may trigger a colour-id read; queue it so the
    // pick runs inside paintGL where the context/framebuffer are valid.
    if (wantsDeferredMouse())
        m_pendingMouse.push_back(
            {PendingMouse::Press, button, Action::Press, mods, x, y});
    else
        m_adapter->injectMouse(button, Action::Press, mods, x, y);
    wakeRendering();
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_mouseButtonPressed = (event->buttons() != Qt::NoButton);
    if (!m_adapter)
        return;
    const ButtonCode button = qtButtonToButtonCode(event->button());
    const Modifier mods = qtModifiersToModifier(event->modifiers());
    const double x = event->position().x();
    const double y = event->position().y();
    if (wantsDeferredMouse())
        m_pendingMouse.push_back(
            {PendingMouse::Release, button, Action::Release, mods, x, y});
    else
        m_adapter->injectMouse(button, Action::Release, mods, x, y);
    wakeRendering();
}

void ViewportWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_mouseButtonPressed = (event->buttons() != Qt::NoButton);
    if (!m_adapter)
        return;

    if (m_cursorCaptured)
    {
        if (m_skipNextMove)
        {
            m_skipNextMove = false;
            return;
        }
        const QPoint center = mapToGlobal(rect().center());
        const QPointF delta = event->globalPosition() - QPointF(center);
        if (delta.isNull())
            return;
        m_virtualCursor += delta;
        m_skipNextMove = true;
        QCursor::setPos(center);
        m_adapter->injectCursorPos(m_virtualCursor.x(), m_virtualCursor.y());
    }
    else
    {
        const double x = event->position().x();
        const double y = event->position().y();
        // Shift-drag face selection in FBO mode reads the framebuffer, so defer
        // it to paintGL. A plain hover queues nothing expensive because the
        // select operator ignores non-dragging moves anyway; but queuing keeps
        // press/move ordering intact for the drag case. No makeCurrent here —
        // that per-move context switch was the FBO-mode framerate drop.
        if (wantsDeferredMouse())
            m_pendingMouse.push_back({PendingMouse::Move,
                                      ButtonCode::MouseButtonLeft, Action::Press,
                                      Modifier::NoModifier, x, y});
        else
            m_adapter->injectCursorPos(x, y);
    }
    wakeRendering();
}

bool ViewportWidget::wantsDeferredMouse() const
{
    return m_view && m_view->getPickMode() == PickMode::Fbo;
}

void ViewportWidget::drainPendingMouse()
{
    if (m_pendingMouse.empty() || !m_adapter)
        return;
    // Context is current and this widget's framebuffer is bound (we are inside
    // paintGL): the FBO pick pass can read back safely and restore it.
    for (const PendingMouse &m : m_pendingMouse)
    {
        switch (m.kind)
        {
        case PendingMouse::Press:
        case PendingMouse::Release:
            m_adapter->injectMouse(m.button, m.action, m.mods, m.x, m.y);
            break;
        case PendingMouse::Move:
            m_adapter->injectCursorPos(m.x, m.y);
            break;
        }
    }
    m_pendingMouse.clear();
}

void ViewportWidget::wheelEvent(QWheelEvent *event)
{
    if (!m_adapter)
        return;
    const QPoint degrees = event->angleDelta();
    if (!degrees.isNull())
    {
        m_adapter->injectScroll(degrees.x() / 120.0, degrees.y() / 120.0);
        wakeRendering();
    }
    event->accept();
}
