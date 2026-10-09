#include "NavGizmoWidget.h"
#include "ViewportWidget.h"

#include "../View/View.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
// Cube corner i encodes its sign per axis in bits 0/1/2 (x/y/z).
glm::vec3 corner(int i)
{
    return glm::vec3((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f,
                     (i & 4) ? 1.0f : -1.0f);
}

enum class Preset
{
    Front,
    Back,
    Left,
    Right,
    Top,
    Bottom
};

struct CubeFace
{
    std::array<int, 4> corners; // boundary loop
    glm::vec3 normal;           // outward, world space
    const char *label;
    Preset preset;
};

// +X Right, -X Left, +Y Top, -Y Bottom, +Z Front, -Z Back — matches the
// Camera::set*View presets (see Camera.cpp).
const std::array<CubeFace, 6> kFaces = {{
    {{1, 3, 7, 5}, {1, 0, 0}, "Right", Preset::Right},
    {{0, 4, 6, 2}, {-1, 0, 0}, "Left", Preset::Left},
    {{2, 3, 7, 6}, {0, 1, 0}, "Top", Preset::Top},
    {{0, 1, 5, 4}, {0, -1, 0}, "Bottom", Preset::Bottom},
    {{4, 5, 7, 6}, {0, 0, 1}, "Front", Preset::Front},
    {{0, 1, 3, 2}, {0, 0, -1}, "Back", Preset::Back},
}};

// One shared duration for every gizmo-driven camera animation: face snaps
// and arrow steps both finish in half a second.
constexpr double kGizmoAnimSec = 0.5;

// Smoothly animates the camera into the preset pose instead of snapping: the
// goal pose is computed on a throwaway copy, then the live camera slerps
// toward it, ticked every frame from View::update.
void applyPreset(Camera &cam, Preset p)
{
    Camera goal = cam;
    switch (p)
    {
    case Preset::Front:
        goal.setFrontView();
        break;
    case Preset::Back:
        goal.setRearView();
        break;
    case Preset::Left:
        goal.setLeftView();
        break;
    case Preset::Right:
        goal.setRightView();
        break;
    case Preset::Top:
        goal.setTopView();
        break;
    case Preset::Bottom:
        goal.setBottomView();
        break;
    }
    cam.startTransitionTo(goal, kGizmoAnimSec);
}

// Each press = an animated 15° arcball step (6 presses = 90°), handled by
// Camera::orbitStepAnimated: ▲/▼ orbit about the camera's right axis, ◄/►
// about its up axis, the curved arrows roll about the look-at axis. A
// still-running step snaps to its end pose before the next one starts, so
// rapid clicks chain full steps.
using Arrow = Camera::OrbitStepDirection;

constexpr float kArrowStep = glm::pi<float>() / 12.0f; // 15 degrees

void applyArrow(Camera &cam, Arrow a)
{
    cam.orbitStepAnimated(a, kArrowStep, kGizmoAnimSec);
}

// ---- Gizmo layout ---------------------------------------------------------
// Straight arrows sit at N/S/W/E on an outer ring; the two curved roll arrows
// are circular arcs on an inner ring, sweeping from the top toward each side.
struct Layout
{
    QPointF center;
    qreal cubeScale;
    QRectF up, down, left, right; // straight-arrow hit boxes
    qreal arcRadius;              // radius of the roll arcs
    qreal rightStart, rightSweep; // Qt angles (deg): 0=3 o'clock, CCW+
    qreal leftStart, leftSweep;
};

Layout makeLayout(int w, int h)
{
    Layout L;
    L.center = QPointF(w * 0.5, h * 0.5);
    L.cubeScale = std::min(w, h) * 0.16;

    const qreal cx = L.center.x();
    const qreal cy = L.center.y();
    const qreal s = 26.0; // arrow long dimension
    const qreal t = 18.0; // arrow short dimension
    L.up = QRectF(cx - s / 2, 4, s, t);
    L.down = QRectF(cx - s / 2, h - 4 - t, s, t);
    L.left = QRectF(4, cy - s / 2, t, s);
    L.right = QRectF(w - 4 - t, cy - s / 2, t, s);

    L.arcRadius = std::min(w, h) * 0.31;
    // Right roll arc: from just right of top (75°) clockwise toward 3 o'clock.
    L.rightStart = 74.0;
    L.rightSweep = -50.0;
    // Left roll arc: from just left of top (106°) toward 9 o'clock.
    L.leftStart = 106.0;
    L.leftSweep = 50.0;
    return L;
}

QPointF pointOnArc(const QPointF &c, qreal r, qreal angleDeg)
{
    const qreal a = angleDeg * glm::pi<double>() / 180.0;
    return QPointF(c.x() + r * std::cos(a), c.y() - r * std::sin(a));
}

void drawStraightArrow(QPainter &p, const QRectF &box, const QPointF &dir)
{
    const QPointF c = box.center();
    const qreal len = std::min(box.width(), box.height()) * 0.5;
    const QPointF perp(-dir.y(), dir.x());
    QPolygonF tri;
    tri << c + dir * len << c - dir * len + perp * len
        << c - dir * len - perp * len;
    p.drawPolygon(tri);
}

void drawArcArrow(QPainter &p, const QPointF &c, qreal r, qreal start,
                  qreal sweep, const QColor &color)
{
    const QRectF box(c.x() - r, c.y() - r, 2 * r, 2 * r);
    QPainterPath path;
    path.arcMoveTo(box, start);
    path.arcTo(box, start, sweep);
    p.setPen(QPen(color, 2.2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Arrowhead at the swept end, pointing along the direction of travel.
    const qreal endAng = start + sweep;
    const QPointF end = pointOnArc(c, r, endAng);
    const QPointF prev = pointOnArc(c, r, endAng - 0.15 * sweep);
    QPointF dir = end - prev;
    const qreal dl = std::hypot(dir.x(), dir.y());
    if (dl > 1e-3)
        dir /= dl;
    const QPointF perp(-dir.y(), dir.x());
    const qreal hs = 5.0;
    QPolygonF head;
    head << end << end - dir * (2 * hs) + perp * hs
         << end - dir * (2 * hs) - perp * hs;
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawPolygon(head);
}
} // namespace

NavGizmoWidget::NavGizmoWidget(ViewportWidget *viewport, QWidget *parent)
    : QWidget(parent), m_viewport(viewport)
{
    // Translucent so the rounded panel's corners stay clear over the scene.
    setAttribute(Qt::WA_TranslucentBackground);
    setToolTip("Drag to orbit · arrows to rotate · double-click a face to snap");
    resize(sizeHint());
}

QSize NavGizmoWidget::sizeHint() const
{
    return QSize(160, 160);
}

void NavGizmoWidget::paintEvent(QPaintEvent *)
{
    View *view = m_viewport ? m_viewport->view() : nullptr;
    if (!view)
        return;

    const Camera &cam = view->getViewport().getCamera();
    // Rotation part of the view matrix maps world → camera space.
    const glm::mat3 R = glm::mat3(cam.calcViewMatrix());

    const Layout L = makeLayout(width(), height());
    const float cx = static_cast<float>(L.center.x());
    const float cy = static_cast<float>(L.center.y());
    const float scale = static_cast<float>(L.cubeScale);

    auto project = [&](const glm::vec3 &worldPt)
    {
        const glm::vec3 c = R * worldPt;
        return QPointF(cx + scale * c.x, cy - scale * c.y);
    };

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Rounded translucent backdrop so the gizmo reads over any scene.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 22, 28, 140));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);

    // ---- Orbit arrows ------------------------------------------------------
    const QColor arrowColor(175, 182, 194);
    p.setPen(Qt::NoPen);
    p.setBrush(arrowColor);
    drawStraightArrow(p, L.up, QPointF(0, -1));
    drawStraightArrow(p, L.down, QPointF(0, 1));
    drawStraightArrow(p, L.left, QPointF(-1, 0));
    drawStraightArrow(p, L.right, QPointF(1, 0));
    drawArcArrow(p, L.center, L.arcRadius, L.rightStart, L.rightSweep,
                 arrowColor);
    drawArcArrow(p, L.center, L.arcRadius, L.leftStart, L.leftSweep,
                 arrowColor);

    // ---- Axis triad --------------------------------------------------------
    struct Axis
    {
        glm::vec3 dir;
        QColor color;
        const char *label;
    };
    const std::array<Axis, 3> axes = {{
        {{1, 0, 0}, QColor(230, 74, 74), "X"},
        {{0, 1, 0}, QColor(120, 200, 90), "Y"},
        {{0, 0, 1}, QColor(90, 150, 235), "Z"},
    }};
    for (const Axis &a : axes)
    {
        const QPointF tip = project(a.dir * 1.35f);
        p.setPen(QPen(a.color, 2.0));
        p.drawLine(QPointF(cx, cy), tip);
        p.setPen(a.color);
        p.drawText(QRectF(tip.x() - 7, tip.y() - 8, 14, 16), Qt::AlignCenter,
                   a.label);
    }

    // ---- Cube faces (front-facing only, far→near) --------------------------
    struct DrawFace
    {
        const CubeFace *face;
        QPolygonF poly;
        QPointF center;
        float depth;
    };
    std::vector<DrawFace> visible;
    for (const CubeFace &f : kFaces)
    {
        const glm::vec3 nView = R * f.normal;
        if (nView.z <= 0.02f)
            continue; // back-facing

        QPolygonF poly;
        glm::vec3 sum(0.0f);
        float depth = 0.0f;
        for (int ci : f.corners)
        {
            const glm::vec3 wc = corner(ci);
            poly << project(wc);
            depth += (R * wc).z;
            sum += wc;
        }
        visible.push_back({&f, poly, project(sum * 0.25f), depth * 0.25f});
    }
    std::sort(visible.begin(), visible.end(),
              [](const DrawFace &a, const DrawFace &b)
              { return a.depth < b.depth; });

    QFont labelFont = p.font();
    labelFont.setPointSizeF(labelFont.pointSizeF() * 0.8);
    labelFont.setBold(true);
    p.setFont(labelFont);

    for (const DrawFace &df : visible)
    {
        p.setPen(QPen(QColor(60, 66, 80), 1.2));
        p.setBrush(QColor(226, 231, 240, 235));
        p.drawPolygon(df.poly);

        p.setPen(QColor(40, 44, 54));
        p.drawText(QRectF(df.center.x() - 24, df.center.y() - 9, 48, 18),
                   Qt::AlignCenter, QString::fromLatin1(df.face->label));
    }
}

void NavGizmoWidget::mousePressEvent(QMouseEvent *event)
{
    View *view = m_viewport ? m_viewport->view() : nullptr;
    if (!view)
        return;

    Camera &cam = view->getViewport().getCamera();
    const QPointF pos = event->position();
    const Layout L = makeLayout(width(), height());

    // 1) Straight orbit arrows (rectangular hit boxes).
    const std::array<std::pair<const QRectF *, Arrow>, 4> straight = {{
        {&L.up, Arrow::Up},
        {&L.down, Arrow::Down},
        {&L.left, Arrow::Left},
        {&L.right, Arrow::Right},
    }};
    for (const auto &h : straight)
    {
        if (h.first->contains(pos))
        {
            applyArrow(cam, h.second);
            m_viewport->update();
            update();
            return;
        }
    }

    // 2) Curved roll arrows: hit-test by polar angle + radius band.
    {
        const qreal dx = pos.x() - L.center.x();
        const qreal dy = L.center.y() - pos.y(); // flip to math convention
        const qreal radius = std::hypot(dx, dy);
        qreal ang = std::atan2(dy, dx) * 180.0 / glm::pi<double>();
        if (ang < 0)
            ang += 360.0;
        if (std::abs(radius - L.arcRadius) < 13.0)
        {
            const qreal rHi = L.rightStart;              // 74
            const qreal rLo = L.rightStart + L.rightSweep; // 24
            const qreal lLo = L.leftStart;               // 106
            const qreal lHi = L.leftStart + L.leftSweep;   // 156
            if (ang >= rLo && ang <= rHi)
            {
                applyArrow(cam, Arrow::RollRight);
                m_viewport->update();
                update();
                return;
            }
            if (ang >= lLo && ang <= lHi)
            {
                applyArrow(cam, Arrow::RollLeft);
                m_viewport->update();
                update();
                return;
            }
        }
    }

    // 3) Plain press begins an arcball orbit drag. A double-click on a labeled
    //    face snaps to that side (handled in mouseDoubleClickEvent).
    m_dragging = true;
    m_lastPos = pos;
}

bool NavGizmoWidget::snapToFaceAt(const QPointF &pos)
{
    View *view = m_viewport ? m_viewport->view() : nullptr;
    if (!view)
        return false;

    Camera &cam = view->getViewport().getCamera();
    const Layout L = makeLayout(width(), height());
    const glm::mat3 R = glm::mat3(cam.calcViewMatrix());
    const float cx = static_cast<float>(L.center.x());
    const float cy = static_cast<float>(L.center.y());
    const float scale = static_cast<float>(L.cubeScale);
    auto project = [&](const glm::vec3 &wp)
    {
        const glm::vec3 c = R * wp;
        return QPointF(cx + scale * c.x, cy - scale * c.y);
    };

    const CubeFace *hit = nullptr;
    float bestDepth = -1e9f;
    for (const CubeFace &f : kFaces)
    {
        if ((R * f.normal).z <= 0.02f)
            continue; // only front faces are clickable
        QPolygonF poly;
        float depth = 0.0f;
        for (int ci : f.corners)
        {
            const glm::vec3 wc = corner(ci);
            poly << project(wc);
            depth += (R * wc).z;
        }
        if (poly.containsPoint(pos, Qt::OddEvenFill) && depth > bestDepth)
        {
            bestDepth = depth;
            hit = &f;
        }
    }
    if (!hit)
        return false;

    applyPreset(cam, hit->preset);
    m_viewport->update();
    update();
    return true;
}

void NavGizmoWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Double-clicking a labeled cube face snaps the camera to that side.
    // Cancel any orbit drag the preceding single press may have started.
    m_dragging = false;
    if (snapToFaceAt(event->position()))
        return;
    QWidget::mouseDoubleClickEvent(event);
}

void NavGizmoWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging)
        return;
    View *view = m_viewport ? m_viewport->view() : nullptr;
    if (!view)
        return;

    // Map gizmo-local positions to a trackball and orbit — same math the
    // TrackBall operator uses on the main viewport (sign conventions matched).
    auto toBall = [&](const QPointF &pt)
    {
        const float nx = -static_cast<float>(pt.x() / width() * 2.0 - 1.0);
        const float ny = static_cast<float>(pt.y() / height() * 2.0 - 1.0);
        return Camera::mapToTrackball(nx, ny);
    };

    Camera &cam = view->getViewport().getCamera();
    cam.orbitArcball(toBall(m_lastPos), toBall(event->position()));
    m_lastPos = event->position();

    m_viewport->update();
    update();
}

void NavGizmoWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
}
