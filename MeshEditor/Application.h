#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "View/View.h"
#include "Model.h"
#include "QtUI/Widgets/MeasurementCardWidget.h"

class DynamicLibrary;
class TaskRunner;

// A per-tab camera snapshot. The engine keeps live camera state inside each
// View's Viewport; since the Qt shell renders every tab through a single
// shared View/GL context (see PROJECT_STRUCTURE §7), switching tabs must
// stash the outgoing tab's camera here and restore the incoming tab's.
struct CameraState
{
    glm::vec3 eye{0.0f, 0.0f, 1.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    bool parallel = false;
    bool initialized = false; // false ⇒ zoom-to-fit on first activation
};

// One open scene = one tab. Owns its own model + source filename + camera.
struct Document
{
    std::unique_ptr<Model> model;
    std::string filename;
    CameraState camera;
};

class Application
{
public:
    Application(const std::string &filename);
    ~Application();
    // Wires a view around an externally created window (e.g. the Qt viewport
    // adapter). Takes ownership of the window; must be called with the GL
    // context current.
    View *createView(IWindow *window);

    void replaceModel(std::unique_ptr<Model> newModel,
                      const std::string &filename);
    const std::string &getFilename() const;
    // Rename the active document (e.g. after Save As) and notify the GUI.
    void setActiveFilename(const std::string &filename);

    // ---- Multi-document (tab) support --------------------------------------
    // Every getInstance()->getModel()/getFilename() call resolves to the
    // ACTIVE document, so operators keep working unchanged across tabs.
    int addDocument(std::unique_ptr<Model> newModel, const std::string &filename);
    void importModelIntoCurrentScene(std::unique_ptr<Model> importedModel);
    void setActiveDocument(int index); // repoints every view at that document
    void closeDocument(int index);     // never drops below one document
    int documentCount() const;
    int activeIndex() const;
    CameraState &cameraStateAt(int index); // read/write a tab's camera snapshot
    const std::string &filenameAt(int index) const; // any document's filename

    // ---- Measurement HUD State ---------------------------------------------
    void setMeasurementData(const struct MeasurementData &data);
    const struct MeasurementData &getMeasurementData() const;
    void clearMeasurementData();
    void setOnMeasurementChanged(std::function<void(const struct MeasurementData &)> callback);

    // ---- Scene-tree node selection -----------------------------------------
    // Selecting a node highlights every mesh in its subtree (gizmo helper
    // nodes excluded) and lets manipulator tools attach to it without a
    // viewport click. Pass nullptr to clear. The selection is cleared
    // automatically whenever the owning model goes away (load/close/switch).
    void setSelectedNode(Node *node);
    Node *getSelectedNode() const;
    // GUI hook: fired after the selection changed (e.g. viewport picking), so
    // the sidebar tree can mirror it.
    void setOnNodeSelectionChanged(std::function<void(Node *)> callback);

    void requestExit();
    bool shouldExit() const;

    // Optional hooks for a GUI shell (Qt): notified when the scene model is
    // replaced (load/new), when a new document/tab is opened (with its index),
    // and when an operator requests application exit.
    void setOnModelReplaced(std::function<void()> callback);
    void setOnDocumentOpened(std::function<void(int)> callback);
    void setOnExitRequested(std::function<void()> callback);

    static Application *getInstance();

    IRenderSystem *getRenderSystem() const;

    Model *getModel() const;

    // The application-wide thread pool for offloading CPU-bound work.
    TaskRunner &getTaskRunner() const;

private:
    View *finishViewSetup(std::unique_ptr<View> view, uint32_t width,
                          uint32_t height);

    std::function<void()> m_onModelReplaced;
    std::function<void(int)> m_onDocumentOpened;
    std::function<void()> m_onExitRequested;
    std::function<void(Node *)> m_onNodeSelectionChanged;
    std::function<void(const MeasurementData &)> m_onMeasurementChanged;
    MeasurementData m_measurementData;

    // Currently selected scene node (sidebar tree or viewport pick); always a
    // node of the ACTIVE document's model, or nullptr.
    Node *m_selectedNode = nullptr;

    std::unique_ptr<DynamicLibrary> dll;
    std::unique_ptr<IRenderSystem> renderSystem;
    std::unique_ptr<TaskRunner> m_taskRunner;
    std::vector<Document> m_documents;
    int m_activeDoc = 0;
    std::vector<std::unique_ptr<View>> views;
    bool m_shouldExit = false;

    static Application *s_instance;
};