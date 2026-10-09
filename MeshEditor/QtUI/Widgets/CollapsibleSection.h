#pragma once

#include <QWidget>
#include <QString>

class QToolButton;
class QFrame;
class QVBoxLayout;

class CollapsibleSection : public QWidget {
    Q_OBJECT

public:
    explicit CollapsibleSection(const QString &title, QWidget *parent = nullptr);
    void setContentLayout(QLayout *contentLayout);

private slots:
    void toggle(bool checked);

private:
    QToolButton *m_toggleButton;
    QFrame *m_contentArea;
    QVBoxLayout *m_mainLayout;
};
