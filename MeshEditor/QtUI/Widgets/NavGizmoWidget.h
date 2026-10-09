#pragma once

#include <QPointF>
#include <QWidget>

class ViewportWidget;

// A CAD-style navigation cube drawn with QPainter over the top-right of the
// viewport. It reads the live camera orientation each repaint so the cube +
// axis triad rotate with the view. Two interactions:
//   * drag              → arcball-orbit the camera (the triad is orbit-only; no
//                         camera-translating arrows)
//   * double-click face → snap the camera to that side (Front/Back/Left/Right/
//                         Top/Bottom), reusing Camera's view presets.
// Text labels live here in Qt because GLRenderSystem cannot render text.
class NavGizmoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NavGizmoWidget(ViewportWidget *viewport, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    // Hit-test the front-most labeled face under pos and snap the camera to it.
    // Returns true if a face was hit and the view was changed.
    bool snapToFaceAt(const QPointF &pos);

    ViewportWidget *m_viewport; // not owned

    bool m_dragging = false;
    QPointF m_lastPos;
};
