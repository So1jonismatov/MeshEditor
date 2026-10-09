#include "MeasurementCardWidget.h"
#include "../../Application.h"

#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <cmath>
#include <algorithm>

// ── 2D Diagram Canvas ────────────────────────────────────────────────────────

MeasurementDiagramWidget::MeasurementDiagramWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(62);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void MeasurementDiagramWidget::setData(const MeasurementData &data)
{
    m_data = data;
    update();
}

void MeasurementDiagramWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const int w = width();
    const int h = height();

    // Solid dark canvas background with subtle border
    p.setPen(QColor(42, 42, 42));
    p.setBrush(QColor(22, 22, 22));
    p.drawRect(0, 0, w - 1, h - 1);

    if (m_data.type == MeasurementType::Distance || m_data.type == MeasurementType::Edge)
    {
        const int margin = 24;
        const int y = h / 2 + 5;
        const int x1 = margin;
        const int x2 = w - margin;

        QColor accentColor = (m_data.type == MeasurementType::Distance)
                                 ? QColor(0, 195, 255)
                                 : QColor(255, 185, 0);

        // Extension lines
        QPen extPen(QColor(120, 120, 120), 1.0, Qt::DashLine);
        p.setPen(extPen);
        p.drawLine(x1, y - 14, x1, y + 8);
        p.drawLine(x2, y - 14, x2, y + 8);

        // Dimension line
        QPen dimPen(accentColor, 1.8);
        p.setPen(dimPen);
        p.drawLine(x1, y, x2, y);

        // Endpoints
        p.setBrush(accentColor);
        p.drawEllipse(QPointF(x1, y), 3.0, 3.0);
        p.drawEllipse(QPointF(x2, y), 3.0, 3.0);

        // Center readout bubble
        QString valStr = (m_data.value > 0.0f)
                             ? QString::asprintf("%.4f u", m_data.value)
                             : QStringLiteral("--");
        QFont f = p.font();
        f.setPointSize(9);
        f.setBold(true);
        p.setFont(f);

        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(valStr) + 10;
        int th = fm.height() + 2;
        QRect textRect((w - tw) / 2, y - 18, tw, th);

        p.setPen(QPen(accentColor, 1.0));
        p.setBrush(QColor(30, 30, 30));
        p.drawRect(textRect);

        p.setPen(QColor(240, 240, 240));
        p.drawText(textRect, Qt::AlignCenter, valStr);
    }
    else if (m_data.type == MeasurementType::Angle)
    {
        const int cx = w / 2 - 18;
        const int cy = h - 12;
        const int len = 36;

        QColor accentColor(255, 110, 180);

        // Base ray
        QPen rayPen(QColor(160, 160, 160), 1.8);
        p.setPen(rayPen);
        p.drawLine(cx, cy, cx + len, cy);

        // Angled ray
        float deg = (m_data.isComplete && m_data.value > 0.0f)
                        ? std::clamp(m_data.value, 5.0f, 175.0f)
                        : 45.0f;
        float rad = deg * (3.14159265f / 180.0f);
        int ex = cx + static_cast<int>(len * std::cos(rad));
        int ey = cy - static_cast<int>(len * std::sin(rad));

        p.setPen(QPen(accentColor, 1.8));
        p.drawLine(cx, cy, ex, ey);

        // Arc
        p.setPen(QPen(accentColor, 1.2, Qt::DashLine));
        p.setBrush(QColor(255, 110, 180, 35));
        QRect arcRect(cx - 16, cy - 16, 32, 32);
        p.drawPie(arcRect, 0, static_cast<int>(deg * 16));

        // Center dot
        p.setPen(Qt::NoPen);
        p.setBrush(accentColor);
        p.drawEllipse(QPointF(cx, cy), 3.0, 3.0);

        // Angle label bubble
        QString angleStr = (m_data.isComplete && m_data.value > 0.0f)
                               ? QString::asprintf("%.2f°", m_data.value)
                               : QStringLiteral("--°");
        QFont f = p.font();
        f.setPointSize(9);
        f.setBold(true);
        p.setFont(f);

        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(angleStr) + 10;
        int th = fm.height() + 2;
        QRect textRect(cx + len - 6, cy - len + 4, tw, th);

        p.setPen(QPen(accentColor, 1.0));
        p.setBrush(QColor(30, 30, 30));
        p.drawRect(textRect);

        p.setPen(QColor(240, 240, 240));
        p.drawText(textRect, Qt::AlignCenter, angleStr);
    }
    else
    {
        p.setPen(QColor(100, 100, 100));
        QFont f = p.font();
        f.setPointSize(8);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, "No measurement active");
    }
}

// ── Native Floating Measurement Window ──────────────────────────────────────

MeasurementCardWidget::MeasurementCardWidget(QWidget *parent)
    : QWidget(parent)
{
    // Native compact floating window inside viewport:
    // Pure Qt child widget for 60 FPS compositor synchronization without HWND lag.
    setFixedWidth(230);
    setCursor(Qt::ArrowCursor);
    setFocusPolicy(Qt::NoFocus);

    // Native desktop window styling (Windows dark theme palette)
    setStyleSheet(
        "MeasurementCardWidget {"
        "  background-color: #202020;"
        "  border: 1px solid #3c3c3c;"
        "  border-radius: 2px;"
        "}");

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── Native Window Title Bar ──────────────────────────────────────────
    m_titleBar = new QFrame(this);
    m_titleBar->setObjectName("TitleBar");
    m_titleBar->setFixedHeight(26);
    m_titleBar->setStyleSheet(
        "QFrame#TitleBar {"
        "  background-color: #2b2b2b;"
        "  border-bottom: 1px solid #383838;"
        "  border-top-left-radius: 2px;"
        "  border-top-right-radius: 2px;"
        "}");
    m_titleBar->installEventFilter(this);

    auto *headerLayout = new QHBoxLayout(m_titleBar);
    headerLayout->setContentsMargins(8, 0, 0, 0);
    headerLayout->setSpacing(0);

    m_windowTitle = new QLabel("Measurement", m_titleBar);
    m_windowTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_windowTitle->setStyleSheet(
        "QLabel {"
        "  font-family: 'Segoe UI', sans-serif;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  color: #e0e0e0;"
        "}");
    headerLayout->addWidget(m_windowTitle);
    headerLayout->addStretch(1);

    // Native Windows-style caption close button (flush to top-right corner)
    m_closeBtn = new QPushButton("✕", m_titleBar);
    m_closeBtn->setObjectName("CloseBtn");
    m_closeBtn->setFixedSize(32, 26);
    m_closeBtn->setFocusPolicy(Qt::NoFocus);
    m_closeBtn->setToolTip("Close");
    m_closeBtn->setStyleSheet(
        "QPushButton#CloseBtn {"
        "  background: transparent;"
        "  color: #a8a8a8;"
        "  border: none;"
        "  border-radius: 0px;"
        "  font-size: 10px;"
        "}"
        "QPushButton#CloseBtn:hover {"
        "  background-color: #c42b1c;"
        "  color: #ffffff;"
        "}"
        "QPushButton#CloseBtn:pressed {"
        "  background-color: #a82315;"
        "  color: #ffffff;"
        "}");
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        clearMeasurement();
        emit closeRequested();
    });
    headerLayout->addWidget(m_closeBtn);

    rootLayout->addWidget(m_titleBar);

    // ── Client Area: Just the visualizer and the value ───────────────────
    auto *clientLayout = new QVBoxLayout;
    clientLayout->setContentsMargins(6, 6, 6, 6);
    clientLayout->setSpacing(5);

    // 1. Visualizer Canvas
    m_diagram = new MeasurementDiagramWidget(this);
    clientLayout->addWidget(m_diagram);

    // 2. Value Readout Label
    m_valueLabel = new QLabel("--", this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setStyleSheet(
        "QLabel {"
        "  font-family: 'Segoe UI', sans-serif;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #ffffff;"
        "  padding: 2px;"
        "}");
    clientLayout->addWidget(m_valueLabel);

    rootLayout->addLayout(clientLayout);

    hide();
}

bool MeasurementCardWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_titleBar)
    {
        switch (event->type())
        {
        case QEvent::MouseButtonPress:
            handleDragPress(static_cast<QMouseEvent *>(event));
            return true;
        case QEvent::MouseMove:
            handleDragMove(static_cast<QMouseEvent *>(event));
            return true;
        case QEvent::MouseButtonRelease:
            handleDragRelease(static_cast<QMouseEvent *>(event));
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MeasurementCardWidget::mousePressEvent(QMouseEvent *event)
{
    handleDragPress(event);
}

void MeasurementCardWidget::mouseMoveEvent(QMouseEvent *event)
{
    handleDragMove(event);
}

void MeasurementCardWidget::mouseReleaseEvent(QMouseEvent *event)
{
    handleDragRelease(event);
}

void MeasurementCardWidget::handleDragPress(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_dragStartGlobal = event->globalPosition().toPoint();
        m_dragStartWidget = pos();
        m_isDragging = false;
        m_lastDelta = QPoint(0, 0);
        event->accept();
    }
}

void MeasurementCardWidget::handleDragMove(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        QPoint delta = event->globalPosition().toPoint() - m_dragStartGlobal;

        // Ignore micro-movements (<3px) before initiating drag to avoid accidental jitter
        if (!m_isDragging)
        {
            if (delta.manhattanLength() < 3)
                return;
            m_isDragging = true;
        }

        // Filter out duplicate or 0-movement events from OS mouse polling
        if (delta == m_lastDelta)
            return;
        m_lastDelta = delta;

        QPoint newPos = m_dragStartWidget + delta;
        if (QWidget *p = parentWidget())
        {
            int maxX = std::max(0, p->width() - width());
            int maxY = std::max(0, p->height() - height());
            newPos.setX(std::clamp(newPos.x(), 0, maxX));
            newPos.setY(std::clamp(newPos.y(), 0, maxY));
        }

        if (newPos != pos())
        {
            move(newPos);
            m_userMoved = true;
            if (parentWidget())
                parentWidget()->update();
        }
        event->accept();
    }
}

void MeasurementCardWidget::handleDragRelease(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_isDragging = false;
        m_lastDelta = QPoint(0, 0);
        if (parentWidget())
            parentWidget()->update();
        event->accept();
    }
}

void MeasurementCardWidget::clampToParent()
{
    if (QWidget *p = parentWidget())
    {
        int maxX = std::max(0, p->width() - width());
        int maxY = std::max(0, p->height() - height());
        QPoint clamped(std::clamp(x(), 0, maxX), std::clamp(y(), 0, maxY));
        if (clamped != pos())
            move(clamped);
    }
}

void MeasurementCardWidget::updateMeasurement(const MeasurementData &data)
{
    if (data.type == MeasurementType::None)
    {
        hide();
        return;
    }

    m_currentType = data.type;
    m_diagram->setData(data);

    if (data.type == MeasurementType::Distance)
    {
        m_windowTitle->setText("Point Distance");
        if (data.value > 0.0f)
            m_valueLabel->setText(QString::asprintf("%.5f units", data.value));
        else
            m_valueLabel->setText("Click start point");
    }
    else if (data.type == MeasurementType::Edge)
    {
        m_windowTitle->setText("Edge Length");
        if (data.value > 0.0f)
            m_valueLabel->setText(QString::asprintf("%.5f units", data.value));
        else
            m_valueLabel->setText("Click near an edge");
    }
    else if (data.type == MeasurementType::Angle)
    {
        m_windowTitle->setText("Face Angle");
        if (data.isComplete)
        {
            float dihedral = 180.0f - data.value;
            m_valueLabel->setText(QString::asprintf("%.2f° (Dihedral: %.2f°)", data.value, dihedral));
        }
        else
        {
            m_valueLabel->setText("Select two faces");
        }
    }

    adjustSize();
    show();
    raise();
}

void MeasurementCardWidget::clearMeasurement()
{
    hide();
    m_userMoved = false;
    m_currentType = MeasurementType::None;
}
