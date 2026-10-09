#pragma once

#include <QMainWindow>
#include <unordered_map>
#include <vector>

#include "../../Interfaces/Keys.h"

class Application;
class LogConsole;
class StatisticsWidget;
class Node;
class QPushButton;
class QTabBar;
class QTreeWidget;
class QTreeWidgetItem;
class ViewportWidget;

// Top-level Qt shell: 30% left sidebar (scene hierarchy + display/tool
// options), 70% right side with the GL viewport on top and the integrated
// log console at the bottom.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(Application &app, QWidget *parent = nullptr);

public slots:
    void refreshSceneTree();
    void toggleFullscreen();
    void showShortcutsDialog();

private slots:
    // A tab was clicked: stash the outgoing tab's camera, activate the new
    // document, and restore its camera.
    void switchToDocument(int index);
    // The tab's × was clicked.
    void closeTab(int index);
    // The ＋ button: open a fresh empty scene in a new tab.
    void newTab();

private:
    QWidget *buildSidebar();
    QWidget *buildOptionsPanel();
    QWidget *buildViewportPane();
    class QToolButton *buildOptionsMenuButton(); // top-right ☰ camera/display
    void buildMenus();
    void addOperatorButton(class QGridLayout *grid, int row, int col,
                           const QString &text, const QString &tooltip,
                           KeyCode key);
    // Adds a checkable, mutually-exclusive manipulator-tool button (the engine
    // enforces the exclusivity; the UI mirrors it — see buildOptionsPanel).
    void addToolButton(class QGridLayout *grid, int row, int col,
                       const QString &text, const QString &tooltip, KeyCode key);

    // Called by the Application hook when a document is appended (Open / New).
    void onDocumentOpened(int index);
    // Snapshot / restore the live camera into/from a document's CameraState.
    void snapshotCameraFor(int docIndex);
    void restoreCameraFor(int docIndex);
    QString tabTitle(int docIndex) const;
    void updateWindowTitle();

    // The engine-side node selection changed (tree click or viewport pick):
    // mirror it in the tree without re-firing the selection handler.
    void onEngineNodeSelected(Node *node);

    Application &m_app;
    ViewportWidget *m_viewport = nullptr;
    QTabBar *m_tabBar = nullptr;
    QTreeWidget *m_sceneTree = nullptr;
    LogConsole *m_logConsole = nullptr;
    StatisticsWidget *m_statsWidget = nullptr;

    // Scene node -> its tree item, rebuilt by refreshSceneTree.
    std::unordered_map<Node *, QTreeWidgetItem *> m_nodeToItem;

    // Manipulator-tool buttons and their operator enter keys. Check state is
    // never tracked locally: syncToolButtonHighlights() re-reads the engine's
    // active-operator state (per document/tab), so the highlight can't drift
    // from what the dispatcher actually has active.
    std::vector<std::pair<KeyCode, QPushButton *>> m_toolButtons;
    void syncToolButtonHighlights();
};
