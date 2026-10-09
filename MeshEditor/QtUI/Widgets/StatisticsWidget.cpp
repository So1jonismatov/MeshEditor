#include "StatisticsWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include "../../Model/Graph/Model.h"
#include "../../Model/Geometry/Mesh.h"

StatisticsWidget::StatisticsWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto *box = new QGroupBox("Information", this);
    auto *formLayout = new QFormLayout(box);
    m_titleLabel = new QLabel("Full Scene");
    m_facesLabel = new QLabel("0");
    m_verticesLabel = new QLabel("0");
    
    // Make the title bold to distinguish what we are looking at
    QFont boldFont = m_titleLabel->font();
    boldFont.setBold(true);
    m_titleLabel->setFont(boldFont);

    formLayout->addRow("Scope:", m_titleLabel);
    formLayout->addRow("Faces:", m_facesLabel);
    formLayout->addRow("Vertices:", m_verticesLabel);

    layout->addWidget(box);
    layout->addStretch();
}

void StatisticsWidget::updateStats(Model *model, Node *selectedNode) {
    if (!model) {
        m_titleLabel->setText("None");
        m_facesLabel->setText("0");
        m_verticesLabel->setText("0");
        return;
    }

    size_t faces = 0;
    size_t verts = 0;

    // Helper lambda to safely count geometry without including Manipulator gizmos
    auto countGeometry = [&](const Node* n, auto& countRef) -> void {
        if (!n || n->isManipulator()) return;
        if (Mesh *mesh = n->getMesh()) {
            faces += mesh->getHalfEdgeTable().getFaces().size();
            verts += mesh->getHalfEdgeTable().getVertices().size();
        }
        for (const auto &child : n->getChildren()) {
            countRef(child.get(), countRef);
        }
    };

    if (selectedNode) {
        m_titleLabel->setText("Selected Node");
        countGeometry(selectedNode, countGeometry);
    } else {
        m_titleLabel->setText("Full Scene");
        for (const auto &node : model->getNodes()) {
            countGeometry(node.get(), countGeometry);
        }
    }

    m_facesLabel->setText(QString::number(faces));
    m_verticesLabel->setText(QString::number(verts));
}
