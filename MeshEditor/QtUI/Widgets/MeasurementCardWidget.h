#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QFrame>
#include <glm/glm.hpp>

#include "../../Interfaces/Keys.h"

enum class MeasurementType
{
    None,
    Distance,
    Edge,
    Angle
};

struct MeasurementData
{
    MeasurementType type = MeasurementType::None;
    bool isComplete = false;
    float value = 0.0f; // length in units or angle in degrees
    glm::vec3 p1{0.0f};
    glm::vec3 p2{0.0f};
    glm::vec3 p3{0.0f};
    glm::vec3 n1{0.0f};
    glm::vec3 n2{0.0f};
    glm::vec3 delta{0.0f};
};

// 2D diagram canvas inside the measurement window
class MeasurementDiagramWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MeasurementDiagramWidget(QWidget *parent = nullptr);
    void setData(const MeasurementData &data);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    MeasurementData m_data;
};

// Native floating tool window inside the viewport
class MeasurementCardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MeasurementCardWidget(QWidget *parent = nullptr);

    void updateMeasurement(const MeasurementData &data);
    void clearMeasurement();

    bool isUserMoved() const { return m_userMoved; }
    void resetPosition() { m_userMoved = false; }
    void clampToParent();
    MeasurementType currentType() const { return m_currentType; }

signals:
    void closeRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void handleDragPress(QMouseEvent *event);
    void handleDragMove(QMouseEvent *event);
    void handleDragRelease(QMouseEvent *event);

    // Native Window Title Bar
    QFrame *m_titleBar = nullptr;
    QLabel *m_windowTitle = nullptr;
    QPushButton *m_closeBtn = nullptr;

    // Content: Just the visualizer and the value
    MeasurementDiagramWidget *m_diagram = nullptr;
    QLabel *m_valueLabel = nullptr;

    // Window and drag state
    bool m_userMoved = false;
    bool m_isDragging = false;
    QPoint m_dragStartGlobal;
    QPoint m_dragStartWidget;
    QPoint m_lastDelta;
    MeasurementType m_currentType = MeasurementType::None;
};
