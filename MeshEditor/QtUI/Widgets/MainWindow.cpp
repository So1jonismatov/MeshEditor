#include "MainWindow.h"
#include "Console/LogConsole.h"
#include "Dialogs/ShortcutsDialog.h"
#include "Dialogs/RefinementDialogs.h"
#include "Operators/Refinement/SubdivideOperator.h"
#include "Operators/Refinement/DecimateOperator.h"
#include "Operators/Refinement/LaplacianSmoothOperator.h"
#include "Operators/Refinement/WeldVerticesOperator.h"
#include "Operators/Refinement/RemoveDegenerateFacesOperator.h"
#include "Operators/Refinement/EqualizeTrianglesOperator.h"
#include "ViewportWidget.h"
#include "CollapsibleSection.h"
#include "StatisticsWidget.h"
#include "../Application.h"
#include "Graph/Model.h"
#include "Graph/Node.h"
#include "Geometry/Mesh.h"
#include "View.h"
#include "ViewUtils.h"

#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(Application &app, QWidget *parent)
    : QMainWindow(parent), m_app(app)
{
    setWindowTitle("MeshEditor");
    resize(1440, 860);

    m_viewport = new ViewportWidget(m_app);
    m_logConsole = new LogConsole;
    m_logConsole->setMinimumHeight(80);
    m_statsWidget = new StatisticsWidget;
    m_statsWidget->setMinimumHeight(80);

    auto *bottomSplitter = new QSplitter(Qt::Horizontal);
    bottomSplitter->addWidget(m_statsWidget);
    bottomSplitter->addWidget(m_logConsole);
    bottomSplitter->setStretchFactor(0, 0);
    bottomSplitter->setStretchFactor(1, 1);

    // Right side: the tab bar + viewport above the integrated log console.
    auto *rightSplitter = new QSplitter(Qt::Vertical);
    rightSplitter->addWidget(buildViewportPane());
    rightSplitter->addWidget(bottomSplitter);
    rightSplitter->setStretchFactor(0, 1);
    rightSplitter->setStretchFactor(1, 0);
    rightSplitter->setSizes({650, 170});
    rightSplitter->setCollapsible(0, false);

    // Main split: 30% sidebar, 70% viewport area. The splitter keeps the
    // ratio sensible at arbitrary window sizes and lets the user drag it.
    auto *mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->addWidget(buildSidebar());
    mainSplitter->addWidget(rightSplitter);
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 7);
    mainSplitter->setSizes({430, 1010});
    mainSplitter->setCollapsible(1, false);

    setCentralWidget(mainSplitter);
    buildMenus();

    // Keep keyboard shortcuts working even when a sidebar widget has focus.
    setFocusProxy(m_viewport);

    connect(m_viewport, &ViewportWidget::viewCreated, this,
            &MainWindow::refreshSceneTree);
    // Any key that reached the dispatcher (button click or shortcut, incl.
    // Esc) may have toggled a manipulator tool — re-read the engine state.
    connect(m_viewport, &ViewportWidget::toolStateMaybeChanged, this,
            [this]() { syncToolButtonHighlights(); });

    m_app.setOnModelReplaced(
        [this]()
        {
            QMetaObject::invokeMethod(this, &MainWindow::refreshSceneTree,
                                      Qt::QueuedConnection);
        });
    // Viewport picking (e.g. the Move-node tool) changes the engine selection;
    // mirror it in the tree. Runs on the GUI thread, so a direct call is fine.
    m_app.setOnNodeSelectionChanged([this](Node *node)
                                    { onEngineNodeSelected(node); });
    m_app.setOnDocumentOpened([this](int index) { onDocumentOpened(index); });
    m_app.setOnExitRequested(
        [this]()
        { QMetaObject::invokeMethod(this, &MainWindow::close,
                                    Qt::QueuedConnection); });

    // Build tabs for any documents that already exist (the initial scene).
    // Signals are blocked: the engine View isn't created until the viewport's
    // first GL context, and it already renders the active document.
    {
        QSignalBlocker block(m_tabBar);
        for (int i = 0; i < m_app.documentCount(); ++i)
            m_tabBar->addTab(tabTitle(i));
        m_tabBar->setCurrentIndex(m_app.activeIndex());
    }
    updateWindowTitle();
}

QWidget *MainWindow::buildViewportPane()
{
    m_tabBar = new QTabBar;
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(false);
    m_tabBar->setExpanding(false);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setFocusPolicy(Qt::NoFocus);
    connect(m_tabBar, &QTabBar::currentChanged, this,
            &MainWindow::switchToDocument);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this,
            &MainWindow::closeTab);

    // ＋ new-tab button sits to the right of the tabs.
    auto *plusButton = new QToolButton;
    plusButton->setText("＋");
    plusButton->setToolTip("New empty scene tab");
    plusButton->setAutoRaise(true);
    plusButton->setFocusPolicy(Qt::NoFocus);
    connect(plusButton, &QToolButton::clicked, this, &MainWindow::newTab);

    auto *tabRow = new QWidget;
    auto *tabLayout = new QHBoxLayout(tabRow);
    tabLayout->setContentsMargins(0, 0, 4, 0);
    tabLayout->setSpacing(0);
    tabLayout->addWidget(m_tabBar);
    tabLayout->addWidget(plusButton);
    tabLayout->addStretch(1);
    // Camera & display dropdown, pinned to the far right of the tab row
    // (outside the renderer, top-right — level with the tabs).
    tabLayout->addWidget(buildOptionsMenuButton());

    auto *pane = new QWidget;
    auto *paneLayout = new QVBoxLayout(pane);
    paneLayout->setContentsMargins(0, 0, 0, 0);
    paneLayout->setSpacing(0);
    paneLayout->addWidget(tabRow);
    paneLayout->addWidget(m_viewport, 1);
    return pane;
}

QWidget *MainWindow::buildSidebar()
{
    auto *sidebar = new QWidget;
    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(6, 0, 6, 0);
    layout->setSpacing(6);

    auto *title = new QLabel("Scene hierarchy");
    title->setStyleSheet("font-weight: bold;");
    layout->addWidget(title);

    m_sceneTree = new QTreeWidget;
    m_sceneTree->setHeaderLabels({"Node", "Info"});

    m_sceneTree->header()->setStretchLastSection(false);

 
    m_sceneTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);

   
    m_sceneTree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_sceneTree->setColumnWidth(1, 80);

    
    connect(m_sceneTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *)
            {
                Node *node =
                    current
                        ? static_cast<Node *>(
                              current->data(0, Qt::UserRole).value<void *>())
                        : nullptr;
                m_app.setSelectedNode(node);
                if (m_viewport)
                    m_viewport->update();
            });
    layout->addWidget(m_sceneTree, 1);

    auto *refreshButton = new QPushButton("Refresh hierarchy");
    refreshButton->setFocusPolicy(Qt::NoFocus);
    connect(refreshButton, &QPushButton::clicked, this,
            &MainWindow::refreshSceneTree);
    layout->addWidget(refreshButton);

    layout->addWidget(buildOptionsPanel());
    return sidebar;
}

void MainWindow::addOperatorButton(QGridLayout *grid, int row, int col,
                                   const QString &text, const QString &tooltip,
                                   KeyCode key)
{
    auto *button = new QPushButton(text);
    button->setToolTip(tooltip);
    button->setFocusPolicy(Qt::NoFocus);
    connect(button, &QPushButton::clicked, this,
            [this, key]() { m_viewport->simulateKey(key); });
    grid->addWidget(button, row, col);
}

void MainWindow::addToolButton(QGridLayout *grid, int row, int col,
                               const QString &text, const QString &tooltip,
                               KeyCode key)
{
    auto *button = new QPushButton(text);
    button->setToolTip(tooltip);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCheckable(true);
    m_toolButtons.emplace_back(key, button);

    // Each manipulator key toggles its operator; the engine additionally exits
    // any other active manipulator (mutual exclusion). The check states are
    // read back from the engine afterwards (via toolStateMaybeChanged), never
    // toggled locally — so the highlight always matches the active tool, even
    // across tab switches, file loads and keyboard shortcuts.
    connect(button, &QPushButton::clicked, this,
            [this, key]() { m_viewport->simulateKey(key); });
    grid->addWidget(button, row, col);
}

QWidget *MainWindow::buildOptionsPanel()
{
    auto *panel = new QWidget;
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *shadingSection = new CollapsibleSection("Shading & Environment", panel);
    auto *editingSection = new CollapsibleSection("Mesh Editing", panel);
    auto *measurementSection = new CollapsibleSection("Measurements", panel);
    auto *refinementSection = new CollapsibleSection("Mesh Refinement", panel);

    // ── Shading & Environment Mode ─────────────────────────────────────────
    auto *shadingLayout = new QVBoxLayout;
    shadingLayout->setSpacing(4);

    auto *pbrRadio = new QRadioButton("PBR (Physically Based)");
    auto *phongRadio = new QRadioButton("Standard (Phong / Face)");
    auto *flatRadio = new QRadioButton("Flat / Unlit");
    pbrRadio->setFocusPolicy(Qt::NoFocus);
    phongRadio->setFocusPolicy(Qt::NoFocus);
    flatRadio->setFocusPolicy(Qt::NoFocus);
    phongRadio->setChecked(true);
    pbrRadio->setToolTip("Physically Based Rendering with Cook-Torrance BRDF, metallic/roughness, and reflections (P).");
    phongRadio->setToolTip("Classic Phong specular & diffuse face shading.");
    flatRadio->setToolTip("Flat unlit shading.");

    auto *shadingGroup = new QButtonGroup(panel);
    shadingGroup->setExclusive(true);
    shadingGroup->addButton(pbrRadio);
    shadingGroup->addButton(phongRadio);
    shadingGroup->addButton(flatRadio);

    shadingLayout->addWidget(pbrRadio);
    shadingLayout->addWidget(phongRadio);
    shadingLayout->addWidget(flatRadio);

    connect(pbrRadio, &QRadioButton::toggled, this,
            [this](bool on)
            {
                if (on && m_app.getRenderSystem())
                {
                    m_app.getRenderSystem()->setShadingMode(ShadingMode::Pbr);
                    if (m_viewport)
                        m_viewport->update();
                }
            });
    connect(phongRadio, &QRadioButton::toggled, this,
            [this](bool on)
            {
                if (on && m_app.getRenderSystem())
                {
                    m_app.getRenderSystem()->setShadingMode(ShadingMode::Standard);
                    if (m_viewport)
                        m_viewport->update();
                }
            });
    connect(flatRadio, &QRadioButton::toggled, this,
            [this](bool on)
            {
                if (on && m_app.getRenderSystem())
                {
                    m_app.getRenderSystem()->setShadingMode(ShadingMode::Flat);
                    if (m_viewport)
                        m_viewport->update();
                }
            });

    auto *skyCheck = new QCheckBox("Sky Environment Background");
    skyCheck->setFocusPolicy(Qt::NoFocus);
    skyCheck->setChecked(false);
    skyCheck->setToolTip("Enable 3D panoramic sky & horizon background.");
    connect(skyCheck, &QCheckBox::toggled, this,
            [this](bool on)
            {
                if (m_app.getRenderSystem())
                {
                    m_app.getRenderSystem()->setEnvironmentBackground(on);
                    if (m_viewport)
                        m_viewport->update();
                }
            });
    shadingLayout->addWidget(skyCheck);

    // ── Sliders for Contrast & Exposure ─────────────────────────────────────
    auto *sliderForm = new QGridLayout;
    sliderForm->setContentsMargins(0, 6, 0, 2);
    sliderForm->setSpacing(4);

    // Contrast Slider
    auto *contrastTitle = new QLabel("Contrast:");
    contrastTitle->setStyleSheet("color: #ccc; font-size: 11px;");
    auto *contrastValLabel = new QLabel("1.00x");
    contrastValLabel->setStyleSheet("color: #4da6ff; font-weight: bold; font-size: 11px;");
    contrastValLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *contrastSlider = new QSlider(Qt::Horizontal);
    contrastSlider->setRange(20, 250); // 0.20x to 2.50x
    contrastSlider->setValue(100);     // 1.00x
    contrastSlider->setFocusPolicy(Qt::NoFocus);
    contrastSlider->setToolTip("Adjust rendering contrast (0.2x – 2.5x).");

    connect(contrastSlider, &QSlider::valueChanged, this,
            [this, contrastValLabel](int val)
            {
                float c = val / 100.0f;
                contrastValLabel->setText(QString::asprintf("%.2fx", c));
                if (m_app.getRenderSystem())
                {
                    m_app.getRenderSystem()->setContrast(c);
                    if (m_viewport)
                        m_viewport->update();
                }
            });

    // Exposure Slider
    auto *exposureTitle = new QLabel("Exposure / HDR:");
    exposureTitle->setStyleSheet("color: #ccc; font-size: 11px;");
    auto *exposureValLabel = new QLabel("1.00x");
    exposureValLabel->setStyleSheet("color: #4da6ff; font-weight: bold; font-size: 11px;");
    exposureValLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *exposureSlider = new QSlider(Qt::Horizontal);
    exposureSlider->setRange(20, 300); // 0.20x to 3.00x
    exposureSlider->setValue(100);     // 1.00x
    exposureSlider->setFocusPolicy(Qt::NoFocus);
    exposureSlider->setToolTip("Adjust lighting exposure & HDR brightness (0.2x – 3.0x).");

    connect(exposureSlider, &QSlider::valueChanged, this,
            [this, exposureValLabel](int val)
            {
                float e = val / 100.0f;
                exposureValLabel->setText(QString::asprintf("%.2fx", e));
                if (m_app.getRenderSystem())
                {
                    m_app.getRenderSystem()->setExposure(e);
                    if (m_viewport)
                        m_viewport->update();
                }
            });

    sliderForm->addWidget(contrastTitle, 0, 0);
    sliderForm->addWidget(contrastValLabel, 0, 1);
    sliderForm->addWidget(contrastSlider, 1, 0, 1, 2);

    sliderForm->addWidget(exposureTitle, 2, 0);
    sliderForm->addWidget(exposureValLabel, 2, 1);
    sliderForm->addWidget(exposureSlider, 3, 0, 1, 2);

    shadingLayout->addLayout(sliderForm);

    QWidget *shadingContent = new QWidget;
    shadingContent->setLayout(shadingLayout);
    auto *shadingFullLayout = new QVBoxLayout;
    shadingFullLayout->setContentsMargins(0, 0, 0, 0);
    shadingFullLayout->addWidget(shadingContent);
    shadingSection->setContentLayout(shadingFullLayout);

    auto *toolsGrid = new QGridLayout;
    toolsGrid->setSpacing(4);
    addToolButton(toolsGrid, 0, 0, "Edit mesh",
                  "Translate/extrude selected faces (E)", KeyCode::KeyE);
    addToolButton(toolsGrid, 0, 1, "Edit vertex",
                  "Move individual vertices (V)", KeyCode::KeyV);
    addToolButton(toolsGrid, 1, 0, "Edit face (triad)",
                  "Full triad on a face: move/rotate/scale (Y)", KeyCode::KeyY);
    addToolButton(toolsGrid, 1, 1, "Move node",
                  "Translate the model node (T)", KeyCode::KeyT);
    addToolButton(toolsGrid, 2, 0, "Scale node",
                  "Scale the model node (B)", KeyCode::KeyB);
    addOperatorButton(toolsGrid, 2, 1, "Delete faces",
                  "Delete the selected faces (Del)", KeyCode::KeyDelete);
    
    QWidget* editingContent = new QWidget;
    editingContent->setLayout(toolsGrid);
    
    // Face-selection method
    auto *selLayout = new QVBoxLayout;
    selLayout->setSpacing(4);
    auto *octreeRadio = new QRadioButton("Octree raycast");
    auto *fboRadio = new QRadioButton("FBO colour-id");
    octreeRadio->setFocusPolicy(Qt::NoFocus);
    fboRadio->setFocusPolicy(Qt::NoFocus);
    octreeRadio->setToolTip("Select faces by CPU octree raycasting.");
    fboRadio->setToolTip("Select faces by rendering ids to an offscreen FBO.");
    octreeRadio->setChecked(true);
    auto *selGroup = new QButtonGroup(panel);
    selGroup->setExclusive(true);
    selGroup->addButton(octreeRadio);
    selGroup->addButton(fboRadio);
    selLayout->addWidget(octreeRadio);
    selLayout->addWidget(fboRadio);
    connect(octreeRadio, &QRadioButton::toggled, this,
            [this](bool on)
            {
                if (on && m_viewport->view())
                    m_viewport->view()->setPickMode(PickMode::Octree);
            });
    connect(fboRadio, &QRadioButton::toggled, this,
            [this](bool on)
            {
                if (on && m_viewport->view())
                    m_viewport->view()->setPickMode(PickMode::Fbo);
            });
            
    auto *editingFullLayout = new QVBoxLayout;
    editingFullLayout->setContentsMargins(0,0,0,0);
    editingFullLayout->addWidget(editingContent);
    editingFullLayout->addLayout(selLayout);
    editingSection->setContentLayout(editingFullLayout);

    // Measurement tools
    auto *measureGrid = new QGridLayout;
    measureGrid->setSpacing(4);
    addToolButton(measureGrid, 0, 0, "Distance",
                  "Measure distance between two points (M)", KeyCode::KeyM);
    addToolButton(measureGrid, 0, 1, "Angle",
                  "Measure angle between two faces (A)", KeyCode::KeyA);
    addToolButton(measureGrid, 1, 0, "Edge",
                  "Measure length of an edge (U)", KeyCode::KeyU);
    
    QWidget* measureContent = new QWidget;
    measureContent->setLayout(measureGrid);
    
    auto *measureLayout = new QVBoxLayout;
    measureLayout->setContentsMargins(0,0,0,0);
    measureLayout->addWidget(measureContent);
    measurementSection->setContentLayout(measureLayout);

    auto *refineGrid = new QGridLayout;
    refineGrid->setSpacing(4);

    auto *smoothBtn = new QPushButton("Laplacian Smooth");
    smoothBtn->setToolTip("Smooth vertices with configurable iterations and factor (1)");
    smoothBtn->setFocusPolicy(Qt::NoFocus);
    connect(smoothBtn, &QPushButton::clicked, this, [this]() {
        SmoothDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            if (m_viewport && m_viewport->view()) {
                LaplacianSmoothOperator op(dlg.getPasses(), dlg.getLambda());
                op.onEnter(*m_viewport->view());
            }
        }
    });

    auto *subdivBtn = new QPushButton("Subdivide Mesh");
    subdivBtn->setToolTip("1-to-4 triangle subdivision to increase mesh detail (5)");
    subdivBtn->setFocusPolicy(Qt::NoFocus);
    connect(subdivBtn, &QPushButton::clicked, this, [this]() {
        size_t faceCount = 0;
        bool hasSelection = false;
        if (Node *n = m_app.getSelectedNode()) {
            if (Mesh *m = n->getMesh()) {
                faceCount = m->getHalfEdgeTable().getFaces().size();
                hasSelection = (m->getSelectedFace().index >= 0);
            }
        }
        SubdivideDialog dlg(faceCount, hasSelection, this);
        if (dlg.exec() == QDialog::Accepted) {
            if (m_viewport && m_viewport->view()) {
                SubdivideOperator op(dlg.getLevels(), dlg.getOnlySelected());
                op.onEnter(*m_viewport->view());
            }
        }
    });

    auto *decBtn = new QPushButton("Decimate Mesh");
    decBtn->setToolTip("Reduce polygon count with reduction percentage selector (4)");
    decBtn->setFocusPolicy(Qt::NoFocus);
    connect(decBtn, &QPushButton::clicked, this, [this]() {
        size_t faceCount = 0;
        if (Node *n = m_app.getSelectedNode()) {
            if (Mesh *m = n->getMesh())
                faceCount = m->getHalfEdgeTable().getFaces().size();
        }
        DecimateDialog dlg(faceCount, this);
        if (dlg.exec() == QDialog::Accepted) {
            if (m_viewport && m_viewport->view()) {
                DecimateOperator op(dlg.getPercentage());
                op.onEnter(*m_viewport->view());
            }
        }
    });

    auto *weldBtn = new QPushButton("Weld Vertices");
    weldBtn->setToolTip("Snap overlapping vertices within tolerance (3)");
    weldBtn->setFocusPolicy(Qt::NoFocus);
    connect(weldBtn, &QPushButton::clicked, this, [this]() {
        WeldDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            if (m_viewport && m_viewport->view()) {
                WeldVerticesOperator op(dlg.getTolerance());
                op.onEnter(*m_viewport->view());
            }
        }
    });

    auto *degBtn = new QPushButton("Remove Degenerate");
    degBtn->setToolTip("Delete faces with near-zero area (2)");
    degBtn->setFocusPolicy(Qt::NoFocus);
    connect(degBtn, &QPushButton::clicked, this, [this]() {
        if (m_viewport && m_viewport->view()) {
            RemoveDegenerateFacesOperator op;
            op.onEnter(*m_viewport->view());
        }
    });

    auto *equalizeBtn = new QPushButton("Equalize Triangles");
    equalizeBtn->setToolTip("Isotropically remesh to make all triangles have equal uniform sizes (6)");
    equalizeBtn->setFocusPolicy(Qt::NoFocus);
    connect(equalizeBtn, &QPushButton::clicked, this, [this]() {
        float avgEdgeLen = 0.0f;
        if (Node *n = m_app.getSelectedNode()) {
            if (Mesh *m = n->getMesh()) {
                const auto &het = m->getHalfEdgeTable();
                const auto &pos = het.getPositions();
                const auto &hes = het.getHalfEdges();
                double total = 0.0;
                int count = 0;
                for (const auto &he : hes) {
                    if (he.fh.index == -1) continue;
                    int64_t vDst = he.dst.index;
                    int64_t vSrc = het.sourceVertex(het.handle(he)).index;
                    if (vSrc >= 0 && vDst >= 0 && vSrc < vDst &&
                        vSrc < static_cast<int64_t>(pos.size()) && vDst < static_cast<int64_t>(pos.size())) {
                        total += glm::length(pos[vDst] - pos[vSrc]);
                        count++;
                    }
                }
                if (count > 0) avgEdgeLen = static_cast<float>(total / count);
            }
        }
        EqualizeDialog dlg(avgEdgeLen, this);
        if (dlg.exec() == QDialog::Accepted) {
            if (m_viewport && m_viewport->view()) {
                EqualizeTrianglesOperator op(dlg.getTargetLength(), dlg.getIterations());
                op.onEnter(*m_viewport->view());
            }
        }
    });

    refineGrid->addWidget(smoothBtn, 0, 0);
    refineGrid->addWidget(subdivBtn, 0, 1);
    refineGrid->addWidget(decBtn, 1, 0);
    refineGrid->addWidget(weldBtn, 1, 1);
    refineGrid->addWidget(degBtn, 2, 0);
    refineGrid->addWidget(equalizeBtn, 2, 1);
    
    QWidget* refineContent = new QWidget;
    refineContent->setLayout(refineGrid);
    
    auto *refineLayout = new QVBoxLayout;
    refineLayout->setContentsMargins(0,0,0,0);
    refineLayout->addWidget(refineContent);
    refinementSection->setContentLayout(refineLayout);

    layout->addWidget(shadingSection);
    layout->addWidget(editingSection);
    layout->addWidget(measurementSection);
    layout->addWidget(refinementSection);
    layout->addStretch();

    // Default open
    editingSection->findChild<QToolButton*>()->setChecked(true);

    return panel;
}

QToolButton *MainWindow::buildOptionsMenuButton()
{
    auto *button = new QToolButton;
    button->setText(QStringLiteral("☰ Views"));
    button->setToolTip("Camera & display options");
    button->setPopupMode(QToolButton::InstantPopup);
    button->setFocusPolicy(Qt::NoFocus);
    button->setStyleSheet(
        "QToolButton { padding: 2px 10px; }"
        " QToolButton::menu-indicator { image: none; }");

    auto *menu = new QMenu(this);

    auto addCam = [&](const QString &text, KeyCode key)
    {
        QAction *a = menu->addAction(text);
        connect(a, &QAction::triggered, this,
                [this, key]() { m_viewport->simulateKey(key); });
    };

    // Front/Rear/Right/Left/Top/Bottom presets live on the navigation cube
    // (Ctrl+click a face), so they are intentionally omitted here.
    menu->addSection("Camera");
    addCam("Isometric", KeyCode::KeyF7);
    addCam("Zoom to fit", KeyCode::KeyF);
    addCam("Toggle projection", KeyCode::KeyF8);

    menu->addSection("Display");
    QAction *blackEdges = menu->addAction("Black edges");
    blackEdges->setCheckable(true);
    connect(blackEdges, &QAction::triggered, this,
            [this]() { m_viewport->simulateKey(KeyCode::KeyR); });
    QAction *holes = menu->addAction("Color holes");
    connect(holes, &QAction::triggered, this,
            [this]() { m_viewport->simulateKey(KeyCode::KeyH); });
    QAction *boundary = menu->addAction("Boundary faces");
    connect(boundary, &QAction::triggered, this,
            [this]() { m_viewport->simulateKey(KeyCode::KeyL); });
    QAction *octree = menu->addAction("Octree boxes");
    octree->setCheckable(true);
    connect(octree, &QAction::triggered, this,
            [this]() { m_viewport->simulateKey(KeyCode::KeyG); });
    QAction *aabb = menu->addAction("Bounding box");
    aabb->setCheckable(true);
    connect(aabb, &QAction::triggered, this,
            [this]() { m_viewport->simulateKey(KeyCode::KeyJ); });

    // Sync check marks with the active document whenever the menu is about to pop up.
    connect(menu, &QMenu::aboutToShow, this,
            [this, blackEdges, octree, aabb]()
            {
                if (!m_app.getModel())
                    return;

                const Mesh *mesh = nullptr;
                if (Model *model = m_app.getModel())
                    model->forEachMeshRecursive(
                        [&](const Mesh *m)
                        {
                            if (!mesh)
                                mesh = m;
                        });
                if (!mesh)
                    return;
                blackEdges->setChecked(mesh->getRenderBlackEdges());
                octree->setChecked(mesh->getRenderOctreeBB());
                aabb->setChecked(mesh->getRenderMeshAABB());
            });

    button->setMenu(menu);
    return button;
}

void MainWindow::buildMenus()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    QAction *newTabAction = fileMenu->addAction("&New Tab");
    newTabAction->setShortcut(QKeySequence::New); // Ctrl+N
    connect(newTabAction, &QAction::triggered, this, &MainWindow::newTab);
    QAction *openAction = fileMenu->addAction("&Open Scene in New Tab...");
    openAction->setShortcut(QKeySequence::Open); // Ctrl+O
    connect(openAction, &QAction::triggered, this,
            [this]() { m_viewport->simulateKey(KeyCode::KeyO); });
    QAction *importAction = fileMenu->addAction("&Import / Add to Scene...");
    importAction->setShortcut(QKeySequence("Ctrl+I"));
    connect(importAction, &QAction::triggered, this,
            [this]() { m_viewport->simulateKey(KeyCode::KeyI); });
    QAction *saveAction = fileMenu->addAction("&Save Scene As...");
    connect(saveAction, &QAction::triggered, this,
            [this]() { m_viewport->simulateKey(KeyCode::KeyS); });
    fileMenu->addSeparator();
    QAction *closeTabAction = fileMenu->addAction("&Close Tab");
    closeTabAction->setShortcut(QKeySequence::Close); // Ctrl+W
    connect(closeTabAction, &QAction::triggered, this,
            [this]() { closeTab(m_app.activeIndex()); });
    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction, &QAction::triggered, this, &MainWindow::close);

    QMenu *viewMenu = menuBar()->addMenu("&View");
    QAction *fullscreenAction = viewMenu->addAction("Toggle &Fullscreen");
    fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    connect(fullscreenAction, &QAction::triggered, this,
            &MainWindow::toggleFullscreen);
    QAction *fitAction = viewMenu->addAction("&Zoom to Model");
    connect(fitAction, &QAction::triggered, this,
            [this]() { m_viewport->simulateKey(KeyCode::KeyF); });
    QAction *projectionAction = viewMenu->addAction("Toggle &Projection");
    connect(projectionAction, &QAction::triggered, this,
            [this]() { m_viewport->simulateKey(KeyCode::KeyF8); });
    QAction *fpsAction = viewMenu->addAction("FPS &Camera");
    connect(fpsAction, &QAction::triggered, this,
            [this]() { m_viewport->simulateKey(KeyCode::KeyF10); });

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *shortcutsAction = helpMenu->addAction("&Keyboard Shortcuts...");
    connect(shortcutsAction, &QAction::triggered, this,
            &MainWindow::showShortcutsDialog);
}

void MainWindow::showShortcutsDialog()
{
    auto *dialog = new ShortcutsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

static QTreeWidgetItem *
buildNodeItem(Node *node,
              std::unordered_map<Node *, QTreeWidgetItem *> &nodeToItem)
{
    QString name = QString::fromStdString(node->getName());
    if (name.isEmpty())
        name = "<node>";

    QString info;
    if (const Mesh *mesh = node->getMesh())
    {
        const auto &table = mesh->getHalfEdgeTable();
        info = QString("%1 v / %2 f")
                   .arg(table.getVertices().size())
                   .arg(table.getFaces().size());
    }

    auto *item = new QTreeWidgetItem(QStringList{name, info});
    // The scene node behind this row, for selection handling.
    item->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void *>(node)));
    nodeToItem[node] = item;
    for (const auto &child : node->getChildren())
    {
        // A gizmo currently attached under this node is not scene content.
        if (child->isManipulator())
            continue;
        item->addChild(buildNodeItem(child.get(), nodeToItem));
    }
    return item;
}

void MainWindow::refreshSceneTree()
{
    if (!m_sceneTree)
        return;

    // Rebuilding the tree churns Qt's current item; block the selection
    // handler so the engine-side selection survives a plain refresh.
    {
        QSignalBlocker block(m_sceneTree);
        m_sceneTree->clear();
    }
    m_nodeToItem.clear();

    Model *model = m_app.getModel();
    if (!model)
        return;

    QString rootLabel = QString::fromStdString(m_app.getFilename());
    if (rootLabel.isEmpty())
        rootLabel = "<empty scene>";
    auto *root = new QTreeWidgetItem(QStringList{rootLabel, ""});
    m_sceneTree->addTopLevelItem(root);

    for (const auto &node : model->getNodes())
    {
        // Manipulator gizmos are transient helper nodes, not scene content.
        if (node->isManipulator())
            continue;
        root->addChild(buildNodeItem(node.get(), m_nodeToItem));
    }
    m_sceneTree->expandAll();

    // Re-point the tree at the still-selected node, if any.
    onEngineNodeSelected(m_app.getSelectedNode());

    // Tab switches / loads / closes reset the engine's operator state; every
    // such path funnels through here, so re-sync the tool highlights too.
    syncToolButtonHighlights();

    // Keep the window title and active tab label in step with the scene
    // (covers Save As renaming the active document).
    updateWindowTitle();

    if (m_statsWidget)
        m_statsWidget->updateStats(m_app.getModel(), m_app.getSelectedNode());
}

void MainWindow::syncToolButtonHighlights()
{
    View *view = m_viewport ? m_viewport->view() : nullptr;
    for (const auto &[key, button] : m_toolButtons)
        button->setChecked(view && view->isOperatorActive(key));
}

void MainWindow::onEngineNodeSelected(Node *node)
{
    if (!m_sceneTree)
        return;

    // Blocked signals: the engine already holds this selection, so the
    // currentItemChanged handler must not re-enter setSelectedNode.
    QSignalBlocker block(m_sceneTree);
    auto it = node ? m_nodeToItem.find(node) : m_nodeToItem.end();
    m_sceneTree->setCurrentItem(it != m_nodeToItem.end() ? it->second
                                                         : nullptr);

    if (m_statsWidget)
        m_statsWidget->updateStats(m_app.getModel(), node);
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen())
    {
        showMaximized();
    }
    else
    {
        showFullScreen();
    }
}

// ---- Multi-document tabs ---------------------------------------------------

QString MainWindow::tabTitle(int docIndex) const
{
    // Show the file's base name; empty (new/unsaved) scenes read "untitled".
    QString file = QString::fromStdString(m_app.filenameAt(docIndex));
    if (file.isEmpty())
        return QStringLiteral("untitled");
    return QFileInfo(file).fileName();
}

void MainWindow::snapshotCameraFor(int docIndex)
{
    View *view = m_viewport ? m_viewport->view() : nullptr;
    if (!view || docIndex < 0)
        return;
    const Camera &cam = view->getViewport().getCamera();
    CameraState &state = m_app.cameraStateAt(docIndex);
    state.eye = cam.getEye();
    state.target = cam.getTarget();
    state.up = cam.getUp();
    state.parallel = view->getViewport().isParallelProjection();
    state.initialized = true;
}

void MainWindow::restoreCameraFor(int docIndex)
{
    View *view = m_viewport ? m_viewport->view() : nullptr;
    if (!view)
        return;
    CameraState &state = m_app.cameraStateAt(docIndex);
    if (state.initialized)
    {
        view->getViewport().getCamera().setEyeTargetUp(state.eye, state.target,
                                                       state.up);
        view->getViewport().setParallelProjection(state.parallel);
    }
    else
    {
        // First time this tab is shown: frame its model.
        zoomViewToModel(*view);
    }
    m_viewport->update();
}

void MainWindow::switchToDocument(int index)
{
    if (index < 0 || index == m_app.activeIndex())
        return;

    snapshotCameraFor(m_app.activeIndex());
    m_app.setActiveDocument(index);
    restoreCameraFor(index);

    refreshSceneTree();
    updateWindowTitle();
}

void MainWindow::onDocumentOpened(int index)
{
    if (!m_tabBar)
        return;

    // Create the matching tab without prematurely triggering a switch.
    {
        QSignalBlocker block(m_tabBar);
        m_tabBar->insertTab(index, tabTitle(index));
    }

    if (m_tabBar->currentIndex() == index)
    {
        // Already current (was the very first tab): apply directly.
        m_app.setActiveDocument(index);
        restoreCameraFor(index);
        refreshSceneTree();
        updateWindowTitle();
    }
    else
    {
        // Activating the new tab runs switchToDocument via currentChanged,
        // which snapshots the outgoing camera and frames the new model.
        m_tabBar->setCurrentIndex(index);
    }
}

void MainWindow::newTab()
{
    // Fires onDocumentOpened, which builds the tab and activates it.
    m_app.addDocument(std::make_unique<Model>(), std::string());
}

void MainWindow::closeTab(int index)
{
    if (m_app.documentCount() <= 1)
        return; // always keep one scene open

    const bool closingActive = (index == m_app.activeIndex());

    {
        QSignalBlocker block(m_tabBar);
        // Destroying the document's Model runs ~Mesh, which frees its GPU
        // buffers — that needs the viewport's GL context current (this runs on
        // the GUI thread, not inside paintGL).
        if (m_viewport)
            m_viewport->makeCurrent();
        m_app.closeDocument(index); // adjusts active index, repoints the view
        if (m_viewport)
            m_viewport->doneCurrent();
        m_tabBar->removeTab(index);
        m_tabBar->setCurrentIndex(m_app.activeIndex());
    }

    // Closing the active tab lands the view on a neighbour whose camera still
    // needs restoring; closing a background tab leaves the view untouched.
    if (closingActive)
        restoreCameraFor(m_app.activeIndex());

    refreshSceneTree();
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    QString file = QString::fromStdString(m_app.getFilename());
    QString scene = file.isEmpty() ? QStringLiteral("untitled")
                                   : QFileInfo(file).fileName();
    setWindowTitle(QStringLiteral("MeshEditor — Lab 6 (Qt) — %1").arg(scene));

    // Keep the active tab's label in sync with its (possibly just-saved) name.
    if (m_tabBar && m_app.activeIndex() >= 0 &&
        m_app.activeIndex() < m_tabBar->count())
        m_tabBar->setTabText(m_app.activeIndex(), scene);
}
