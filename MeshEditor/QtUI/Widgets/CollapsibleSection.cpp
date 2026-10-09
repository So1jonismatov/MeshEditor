#include "CollapsibleSection.h"
#include <QVBoxLayout>
#include <QToolButton>
#include <QFrame>

CollapsibleSection::CollapsibleSection(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    m_toggleButton = new QToolButton(this);
    m_toggleButton->setStyleSheet("QToolButton { border: none; font-weight: bold; text-align: left; }");
    m_toggleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toggleButton->setArrowType(Qt::RightArrow);
    m_toggleButton->setText(title);
    m_toggleButton->setCheckable(true);
    m_toggleButton->setChecked(false);
    m_toggleButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_contentArea = new QFrame(this);
    m_contentArea->setStyleSheet("QFrame { border: none; margin-left: 10px; }");
    m_contentArea->setVisible(false);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->addWidget(m_toggleButton);
    m_mainLayout->addWidget(m_contentArea);

    connect(m_toggleButton, &QToolButton::toggled, this, &CollapsibleSection::toggle);
}

void CollapsibleSection::setContentLayout(QLayout *contentLayout)
{
    delete m_contentArea->layout();
    m_contentArea->setLayout(contentLayout);
}

void CollapsibleSection::toggle(bool checked)
{
    m_toggleButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
    m_contentArea->setVisible(checked);
}
