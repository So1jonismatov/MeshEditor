#pragma once
#include <chrono>
#include <memory>
#include <vector>
#include <string>
#include <concepts>
#include <optional>

#include "ViewPort.h"
#include "Model.h"
#include "OperatorDispatcher.h"
#include "Contact.h"
#include "FilterValue.h"

#include "IRenderSystem.h"

class IWindow;

// How face selection resolves a click: CPU octree raycast (default) or a
// GPU render-to-texture colour-id pass.
enum class PickMode
{
    Octree,
    Fbo
};

class View
{
public:
    // Takes ownership of an externally created window (e.g. a Qt viewport
    // adapter) instead of creating one through GLRenderSystem.dll. The
    // adopted window is deleted by ~View — the caller must not delete it.
    View(IRenderSystem &rs, IWindow *window);
    ~View();

    void update();

    void setModel(Model *model);
    Model *getModel() const;

    void addOperator(KeyCode enterKey, KeyCode exitKey,
                     std::unique_ptr<Operator> op);
    void addOperator(ButtonCode button, std::unique_ptr<Operator> op);
    void addOperator(KeyCode key, std::unique_ptr<Operator> op);

    void resetOperatorState();

    // True while the enter/exit operator registered under `enterKey` (e.g. a
    // manipulator tool) is active — used by the GUI to mirror tool state.
    bool isOperatorActive(KeyCode enterKey) const;

    template <class Lambda>
    requires std::invocable<Lambda, View &, Action, Modifier> void addOperator(
        KeyCode key, Lambda lambda)
    {
        operatorDispatcher.addOperator(key, lambda);
    }

    Viewport &getViewport();
    const Viewport &getViewport() const;

    std::vector<Contact> raycast(double x, double y,
                                 FilterValue filterValues) const;

    // Mode-aware face pick used by SelectFacesOperator. Dispatches to the
    // octree raycast or the FBO colour-id pass depending on the pick mode.
    std::optional<Contact> pickFace(double x, double y);
    void setPickMode(PickMode mode);
    PickMode getPickMode() const;

    IWindow *getWindow() const;

private:
    void setupWindowCallbacks();
    std::optional<Contact> fboPickFace(double x, double y);

    bool rayTriangleIntersection(const glm::vec3 &orig, const glm::vec3 &dir,
                                 const glm::vec3 &v0, const glm::vec3 &v1,
                                 const glm::vec3 &v2, float &tOut) const;

    void collectNodeContacts(const Node *node, const ray &rayWorld,
                             FilterValue filterValues,
                             std::vector<Contact> &contacts,
                             const glm::mat4 &parentTransform) const;

    OperatorDispatcher operatorDispatcher;
    IRenderSystem *m_renderSystem = nullptr;
    IWindow *m_window = nullptr;
    // Set when m_window came from GLRenderSystem.dll's createWindow: the
    // matching deallocator lives in the DLL. Null for adopted windows, which
    // were allocated in the executable and are freed with plain delete.
    // (Safe to store: Application holds its own handle to the DLL, so it
    // stays loaded for the lifetime of every View.)
    void (*m_destroyWindow)(IWindow *) = nullptr;
    Model *m_model = nullptr;
    Viewport m_viewport;
    PickMode m_pickMode = PickMode::Octree;
    // One-time upload latch for the persistent coordinate-axes line buffer.
    bool m_axesUploaded = false;
    // Per-frame clock driving frame-rate-independent camera transitions.
    std::chrono::steady_clock::time_point m_lastUpdateTime{};
};