#pragma once

#include <QWidget>

class QLabel;
class Model;
class Node;

class StatisticsWidget : public QWidget {
    Q_OBJECT

public:
    explicit StatisticsWidget(QWidget *parent = nullptr);
    void updateStats(Model *model, Node *selectedNode);

private:
    QLabel *m_titleLabel;
    QLabel *m_facesLabel;
    QLabel *m_verticesLabel;
};
