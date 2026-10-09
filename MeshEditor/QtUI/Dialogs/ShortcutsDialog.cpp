#include "ShortcutsDialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QFont>

ShortcutsDialog::ShortcutsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Keyboard & Mouse Reference");
    resize(580, 620);
    setStyleSheet("QDialog { background-color: #1a1e26; color: #ffffff; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // Title label
    auto *titleLabel = new QLabel("Keyboard & Mouse Reference", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #50b4ff;");
    mainLayout->addWidget(titleLabel);

    // Search filter input
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Type to filter shortcuts or actions...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #14171e;"
        "  border: 1px solid #363e4d;"
        "  border-radius: 4px;"
        "  color: #e2e8f0;"
        "  padding: 6px 10px;"
        "  font-size: 12px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #50b4ff;"
        "}");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ShortcutsDialog::filterShortcuts);
    mainLayout->addWidget(m_searchEdit);

    // Native Tree Widget for shortcuts reference
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabels({"Action / Description", "Shortcut"});
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setAlternatingRowColors(false);
    m_treeWidget->setFocusPolicy(Qt::NoFocus);
    m_treeWidget->setStyleSheet(
        "QTreeWidget {"
        "  background-color: #14171e;"
        "  border: 1px solid #363e4d;"
        "  border-radius: 4px;"
        "  color: #d1d5db;"
        "  outline: none;"
        "}"
        "QTreeWidget::item {"
        "  padding: 4px 6px;"
        "  border-bottom: 1px solid #1a1e27;"
        "}"
        "QTreeWidget::item:hover {"
        "  background-color: #212733;"
        "}"
        "QTreeWidget::item:selected {"
        "  background-color: #263347;"
        "}"
        "QHeaderView::section {"
        "  background-color: #1f242e;"
        "  color: #94a3b8;"
        "  padding: 5px 8px;"
        "  border: none;"
        "  border-bottom: 1px solid #363e4d;"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "}");

    m_treeWidget->header()->setStretchLastSection(false);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    // 1. Mouse Navigation
    addCategory("Mouse Navigation", {
        {"Left drag", "Pan the camera"},
        {"Right drag", "Orbit (trackball) around target"},
        {"Scroll wheel", "Zoom camera in / out"},
        {"Middle click", "Select face under cursor (Shift = multi-select)"}
    });

    // 2. Navigation Cube
    addCategory("Navigation Cube (Top-Right)", {
        {"Left drag cube", "Smooth arcball rotation"},
        {"▲ ▼ ◄ ►", "Rotate 15° about camera axes (6 clicks = 90°)"},
        {"Curved arrows", "Roll camera about view line"},
        {"Double-click face", "Snap smoothly to standard orthographic view"}
    });

    // 3. Camera Controls
    addCategory("Camera Views & Projections", {
        {"F1 … F7", "Standard views: Front / Rear / Right / Left / Top / Bottom / Iso"},
        {"F8", "Toggle perspective / orthographic projection"},
        {"F9 / F", "Zoom to fit (frame entire model in viewport)"},
        {"F10", "FPS (WASD) walkthrough camera mode"},
        {"F11", "Toggle fullscreen mode"}
    });

    // 4. Display & Shading Modes
    addCategory("Display & Shading Modes", {
        {"P", "Toggle PBR / Standard face shading"},
        {"R", "Wireframe black-edge overlay"},
        {"H", "Color hole boundary loops"},
        {"L", "Highlight open mesh boundary faces"},
        {"G", "Display Octree bounding box hierarchy"},
        {"J", "Toggle model Axis-Aligned Bounding Box (AABB)"}
    });

    // 5. Mesh Editing Tools
    addCategory("Mesh Editing & Measurement Tools", {
        {"E", "Edit mesh — translate / extrude selected faces"},
        {"V", "Edit vertex — translate individual mesh vertices"},
        {"Y", "Edit face — full 3D triad (move, rotate, scale)"},
        {"T", "Transform (translate) selected scene node"},
        {"B", "Scale selected scene node"},
        {"N", "Move faces orthogonal to normal direction"},
        {"M", "Measure distance between two points"},
        {"A", "Measure angle between two face normals"},
        {"U", "Measure length of an edge"},
        {"Backspace / Del", "Delete selected mesh faces"},
        {"Esc", "Cancel active tool / deselect"}
    });

    // 6. Mesh Refinement & Remeshing
    addCategory("Mesh Refinement & Remeshing", {
        {"1 (Num 1)", "Laplacian smoothing with configurable passes"},
        {"2 (Num 2)", "Remove degenerate and zero-area faces"},
        {"3 (Num 3)", "Weld overlapping vertices within tolerance"},
        {"4 (Num 4)", "Decimate mesh polygon count"},
        {"5 (Num 5)", "Subdivide triangles (1-to-4 subdivision)"},
        {"6 (Num 6)", "Equalize triangles (isotropic remesh)"}
    });

    // 7. File & Tab Management
    addCategory("File & Document Management", {
        {"O / Ctrl+O", "Open mesh scene in new document tab"},
        {"Ctrl+I", "Import / add mesh into current scene"},
        {"S", "Save active scene As"},
        {"Ctrl+N", "Create empty scene in new document tab"},
        {"Ctrl+W", "Close active document tab"},
        {"Ctrl+Q", "Exit MeshEditor application"}
    });

    mainLayout->addWidget(m_treeWidget, 1);

    // Native Close button box
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->setStyleSheet(
        "QPushButton {"
        "  background-color: #272e3b;"
        "  border: 1px solid #3c4656;"
        "  color: #e2e8f0;"
        "  border-radius: 4px;"
        "  padding: 6px 18px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background-color: #353e50;"
        "  color: #ffffff;"
        "}");
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    mainLayout->addWidget(buttons);
}

void ShortcutsDialog::addCategory(const QString &categoryName,
                                 const std::vector<std::pair<QString, QString>> &shortcuts)
{
    auto *catItem = new QTreeWidgetItem(m_treeWidget);
    catItem->setText(0, categoryName);
    QFont catFont = catItem->font(0);
    catFont.setBold(true);
    catFont.setPointSize(10);
    catItem->setFont(0, catFont);
    catItem->setForeground(0, QColor(80, 180, 255));
    catItem->setFirstColumnSpanned(true);
    catItem->setFlags(Qt::ItemIsEnabled);

    for (const auto &[key, desc] : shortcuts)
    {
        auto *item = new QTreeWidgetItem(catItem);
        item->setText(0, desc);
        item->setText(1, key);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        QFont keyFont = item->font(1);
        keyFont.setFamily("Consolas, Courier New, monospace");
        keyFont.setBold(true);
        item->setFont(1, keyFont);
        item->setForeground(1, QColor(255, 205, 90));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    }
    catItem->setExpanded(true);
}

void ShortcutsDialog::filterShortcuts(const QString &query)
{
    QString q = query.trimmed().toLower();
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *cat = m_treeWidget->topLevelItem(i);
        bool anyChildVisible = false;

        for (int j = 0; j < cat->childCount(); ++j)
        {
            QTreeWidgetItem *child = cat->child(j);
            bool matches = q.isEmpty() ||
                           child->text(0).toLower().contains(q) ||
                           child->text(1).toLower().contains(q);
            child->setHidden(!matches);
            if (matches)
                anyChildVisible = true;
        }

        bool catMatches = q.isEmpty() || cat->text(0).toLower().contains(q);
        cat->setHidden(!catMatches && !anyChildVisible);
        if (anyChildVisible || catMatches)
            cat->setExpanded(true);
    }
}
