#include "Application.h"
#include "DynamicLibrary.h"
#include "Model/IO/SceneIO.h"
#include "Threading/TaskRunner.h"
#include "View/ViewUtils.h"
#include "Operators/Camera/CameraPreset.h"
#include "Operators/Camera/FpsCamera.h"
#include "Operators/Camera/KeyboardOrbit.h"
#include "Operators/Selection/DeleteFaces.h"
#include "Operators/Display/ColorHoles.h"
#include "Operators/Topology/MoveEvenFaces.h"
#include "Operators/Topology/MoveOrthogonalFaces.h"
#include "Operators/IO/LoadScene.h"
#include "Operators/Display/PaintBoundaryFaces.h"
#include "Operators/IO/SaveScene.h"
#include "Operators/Selection/SelectFaces.h"
#include "Operators/Display/ToggleAABB.h"
#include "Operators/Display/ToggleOctree.h"
#include "Operators/Display/ToggleBlackEdges.h"
#include "Operators/Display/TogglePbrOperator.h"
#include "Operators/Camera/ToggleProjection.h"
#include "Operators/Camera/ZoomToModel.h"
#include "../Interfaces/IRenderSystem.h"
#include "../Interfaces/IWindow.h"
#include <algorithm>
#include <iostream>
#include "Operators/Camera/Pan.h"
#include "Operators/Camera/TrackBall.h"
#include "Operators/Editing/EditVertex.h"
#include "Operators/Editing/EditFace.h"
#include "Operators/Measurement/DistanceMeasurementOperator.h"
#include "Operators/Measurement/AngleMeasurementOperator.h"
#include "Operators/Measurement/EdgeMeasurementOperator.h"
#include "Operators/Refinement/LaplacianSmoothOperator.h"
#include "Operators/Refinement/RemoveDegenerateFacesOperator.h"
#include "Operators/Refinement/DecimateOperator.h"
#include "Operators/Refinement/WeldVerticesOperator.h"
#include "Operators/Refinement/SubdivideOperator.h"
#include "Operators/Refinement/EqualizeTrianglesOperator.h"
#include "Operators/IO/ImportScene.h"
#include "Operators/Editing/EditMesh.h"
#include "Operators/Editing/TransformNode.h"
#include "Operators/Editing/ScaleNode.h"

Application *Application::s_instance = nullptr;

namespace
{
// Toggle the selection tint on every mesh of a node's subtree. Gizmo helper
// nodes (triads/manipulators) are transient children of scene nodes and must
// never be tinted, so the recursion stops at them.
void setSubtreeHighlighted(Node *node, bool enable)
{
    if (!node || node->isManipulator())
        return;
    if (Mesh *mesh = node->getMesh())
        mesh->setNodeHighlighted(enable);
    for (const auto &child : node->getChildren())
        setSubtreeHighlighted(child.get(), enable);
}
} // namespace

void Application::setSelectedNode(Node *node)
{
    if (node == m_selectedNode)
        return;
    setSubtreeHighlighted(m_selectedNode, false);
    m_selectedNode = node;
    setSubtreeHighlighted(m_selectedNode, true);
    if (m_onNodeSelectionChanged)
        m_onNodeSelectionChanged(m_selectedNode);
}

Node *Application::getSelectedNode() const
{
    return m_selectedNode;
}

void Application::setOnNodeSelectionChanged(
    std::function<void(Node *)> callback)
{
    m_onNodeSelectionChanged = std::move(callback);
}

Application *Application::getInstance()
{
    return s_instance;
}

IRenderSystem *Application::getRenderSystem() const
{
    return renderSystem.get();
}

Model *Application::getModel() const
{
    if (m_activeDoc < 0 || m_activeDoc >= static_cast<int>(m_documents.size()))
        return nullptr;
    return m_documents[m_activeDoc].model.get();
}

TaskRunner &Application::getTaskRunner() const
{
    return *m_taskRunner;
}

Application::Application(const std::string &filename)
{
    s_instance = this;
    m_taskRunner = std::make_unique<TaskRunner>();
#ifdef _WIN32
    const std::string renderLib = "GLRenderSystem.dll";
#else
    const std::string renderLib = "libGLRenderSystem.so";
#endif
    std::cerr << "[App] Loading " << renderLib << "..." << std::endl;
    dll = std::make_unique<DynamicLibrary>(renderLib);
    auto createRenderSystem =
        dll->getSymbol<IRenderSystem *(*)()>("createRenderSystem");
    if (createRenderSystem)
    {
        std::cerr << "[App] createRenderSystem symbol found, calling..."
                  << std::endl;
        renderSystem = std::unique_ptr<IRenderSystem>(createRenderSystem());
        std::cerr << "[App] renderSystem created" << std::endl;
    }
    else
    {
        std::cerr << "[App] ERROR: createRenderSystem symbol NOT found in DLL!"
                  << std::endl;
    }

    std::cerr << "[App] Loading scene model..." << std::endl;
    // Seed the first document (tab 0) directly — the GUI shell is not
    // constructed yet, so the onDocumentOpened hook would be a no-op here.
    Document doc;
    doc.model = loadSceneModel(filename);
    if (!doc.model)
        doc.model = std::make_unique<Model>();
    doc.filename = filename;
    m_documents.push_back(std::move(doc));
    m_activeDoc = 0;
    std::cerr << "[App] constructor done" << std::endl;
}

void Application::replaceModel(std::unique_ptr<Model> newModel,
                               const std::string &filename)
{
    if (!newModel)
    {
        newModel = std::make_unique<Model>();
    }

    if (m_documents.empty())
        m_documents.emplace_back();

    for (auto &existingView : views)
    {
        existingView->resetOperatorState();
    }

    // Clear the node selection while the outgoing model is still alive — the
    // highlight reset walks its nodes.
    setSelectedNode(nullptr);

    Document &doc = m_documents[m_activeDoc];
    doc.model = std::move(newModel);
    doc.filename = filename;
    for (auto &existingView : views)
    {
        existingView->setModel(doc.model.get());
    }

    if (m_onModelReplaced)
    {
        m_onModelReplaced();
    }
}

int Application::addDocument(std::unique_ptr<Model> newModel,
                             const std::string &filename)
{
    if (!newModel)
        newModel = std::make_unique<Model>();

    Document doc;
    doc.model = std::move(newModel);
    doc.filename = filename;
    m_documents.push_back(std::move(doc));
    const int index = static_cast<int>(m_documents.size()) - 1;

    
    if (m_onDocumentOpened)
        m_onDocumentOpened(index);
    return index;
}

void Application::importModelIntoCurrentScene(std::unique_ptr<Model> importedModel)
{
    if (!importedModel)
        return;

    Model *currentModel = getModel();
    if (!currentModel)
    {
        replaceModel(std::move(importedModel), "Imported Scene");
        return;
    }

    auto nodes = importedModel->extractNodes();
    for (auto &node : nodes)
    {
        if (node)
            currentModel->attachNode(std::move(node));
    }

    currentModel->markWorldBoundingBoxDirty();

    if (m_onModelReplaced)
    {
        m_onModelReplaced();
    }
}

void Application::setMeasurementData(const MeasurementData &data)
{
    m_measurementData = data;
    if (m_onMeasurementChanged)
        m_onMeasurementChanged(m_measurementData);
}

const MeasurementData &Application::getMeasurementData() const
{
    return m_measurementData;
}

void Application::clearMeasurementData()
{
    m_measurementData.type = MeasurementType::None;
    if (m_onMeasurementChanged)
        m_onMeasurementChanged(m_measurementData);
}

void Application::setOnMeasurementChanged(std::function<void(const MeasurementData &)> callback)
{
    m_onMeasurementChanged = std::move(callback);
}

void Application::setActiveDocument(int index)
{
    if (index < 0 || index >= static_cast<int>(m_documents.size()))
        return;

    // The selection belongs to the outgoing document's model.
    setSelectedNode(nullptr);

    m_activeDoc = index;

    Model *activeModel = m_documents[m_activeDoc].model.get();
    for (auto &existingView : views)
    {
        existingView->resetOperatorState();
        existingView->setModel(activeModel);
    }
}

void Application::closeDocument(int index)
{
    if (index < 0 || index >= static_cast<int>(m_documents.size()))
        return;
    if (m_documents.size() == 1)
        return; // always keep at least one document open

    // Detach any active operator (e.g. a manipulator gizmo) BEFORE freeing the
    // document. resetOperatorState() calls Node::deleteFromParent() on the
    // gizmo node; if we erased the document first and it happened to be the
    // active one, that node — and its parent — would already be freed, so the
    // detach would dereference dangling pointers (access violation on close).
    for (auto &existingView : views)
        existingView->resetOperatorState();

    // Same dangling-pointer hazard as the gizmo above: drop the node selection
    // before its model can be freed.
    setSelectedNode(nullptr);

    m_documents.erase(m_documents.begin() + index);

    // Keep m_activeDoc pointing at a valid, sensible neighbour.
    if (m_activeDoc > index)
        --m_activeDoc;
    else if (m_activeDoc == index)
        m_activeDoc =
            std::min(m_activeDoc, static_cast<int>(m_documents.size()) - 1);

    Model *activeModel = m_documents[m_activeDoc].model.get();
    for (auto &existingView : views)
        existingView->setModel(activeModel);
}

int Application::documentCount() const
{
    return static_cast<int>(m_documents.size());
}

int Application::activeIndex() const
{
    return m_activeDoc;
}

CameraState &Application::cameraStateAt(int index)
{
    static CameraState fallback;
    if (index < 0 || index >= static_cast<int>(m_documents.size()))
        return fallback;
    return m_documents[index].camera;
}

const std::string &Application::filenameAt(int index) const
{
    static const std::string empty;
    if (index < 0 || index >= static_cast<int>(m_documents.size()))
        return empty;
    return m_documents[index].filename;
}

const std::string &Application::getFilename() const
{
    static const std::string empty;
    if (m_activeDoc < 0 || m_activeDoc >= static_cast<int>(m_documents.size()))
        return empty;
    return m_documents[m_activeDoc].filename;
}

void Application::setActiveFilename(const std::string &filename)
{
    if (m_activeDoc < 0 || m_activeDoc >= static_cast<int>(m_documents.size()))
        return;
    m_documents[m_activeDoc].filename = filename;
    if (m_onModelReplaced)
        m_onModelReplaced(); // triggers a GUI refresh (tab title / hierarchy)
}

Application::~Application()
{
    for (auto &existingView : views)
    {
        if (existingView)
            existingView->resetOperatorState();
    }
    setSelectedNode(nullptr);
    views.clear();
    if (m_taskRunner)
        m_taskRunner.reset();
    m_documents.clear();
    if (s_instance == this)
        s_instance = nullptr;
}


View *Application::createView(IWindow *window)
{
    if (!renderSystem || !window)
    {
        std::cerr << "[App] renderSystem or window is null, cannot create view."
                  << std::endl;
        delete window;
        return nullptr;
    }

    auto view = std::make_unique<View>(*renderSystem, window);
    return finishViewSetup(std::move(view), window->getWidth(),
                           window->getHeight());
}

View *Application::finishViewSetup(std::unique_ptr<View> view, uint32_t width,
                                   uint32_t height)
{
    // Init the render system now that a GL context is current
    renderSystem->init();
    std::cerr << "[App] init() done, setting up lights/viewport" << std::endl;

    renderSystem->setViewport(0.0, 0.0, width, height);
    renderSystem->setupDirectionalLight(
        0, glm::normalize(glm::vec3(-0.6f, -1.0f, -0.35f)),
        glm::vec3(1.0f, 1.0f, 1.0f));
    renderSystem->setupLight(1, glm::vec3(-3.0f, 2.0f, 1.0f),
                             glm::vec3(0.7f, 0.8f, 1.0f));
    renderSystem->turnLight(0, true);
    renderSystem->turnLight(1, true);
    std::cerr << "[App] lights set" << std::endl;

    view->setModel(getModel());
    zoomViewToModel(*view);

    view->addOperator(ButtonCode::MouseButtonLeft, std::make_unique<Pan>());
    view->addOperator(ButtonCode::MouseButtonMiddle,
                      std::make_unique<SelectFacesOperator>());
    view->addOperator(ButtonCode::MouseButtonRight,
                      std::make_unique<TrackBall>());

    view->addOperator(KeyCode::KeyF1, std::make_unique<CameraPresetOperator>(
                                          CameraPresetOperator::Preset::Front));
    view->addOperator(KeyCode::KeyF2, std::make_unique<CameraPresetOperator>(
                                          CameraPresetOperator::Preset::Rear));
    view->addOperator(KeyCode::KeyF3, std::make_unique<CameraPresetOperator>(
                                          CameraPresetOperator::Preset::Right));
    view->addOperator(KeyCode::KeyF4, std::make_unique<CameraPresetOperator>(
                                          CameraPresetOperator::Preset::Left));
    view->addOperator(KeyCode::KeyF5, std::make_unique<CameraPresetOperator>(
                                          CameraPresetOperator::Preset::Top));
    view->addOperator(KeyCode::KeyF6,
                      std::make_unique<CameraPresetOperator>(
                          CameraPresetOperator::Preset::Bottom));
    view->addOperator(KeyCode::KeyF7, std::make_unique<CameraPresetOperator>(
                                          CameraPresetOperator::Preset::Iso));

    // Arrow keys: animated 15° orbit steps, same behaviour as the navigation
    // gizmo's arrow buttons. When FPS mode (F10) is active it claims these
    // keys for movement instead (see FpsCameraOperator::consumesKey).
    view->addOperator(KeyCode::KeyLeft, std::make_unique<KeyboardOrbitOperator>(
                                            Camera::OrbitStepDirection::Left));
    view->addOperator(KeyCode::KeyRight,
                      std::make_unique<KeyboardOrbitOperator>(
                          Camera::OrbitStepDirection::Right));
    view->addOperator(KeyCode::KeyUp, std::make_unique<KeyboardOrbitOperator>(
                                          Camera::OrbitStepDirection::Up));
    view->addOperator(KeyCode::KeyDown, std::make_unique<KeyboardOrbitOperator>(
                                            Camera::OrbitStepDirection::Down));

    view->addOperator(KeyCode::KeyF8,
                      std::make_unique<ToggleProjectionOperator>());

    view->addOperator(KeyCode::KeyF9, std::make_unique<ZoomToModelOperator>());
    view->addOperator(KeyCode::KeyF, std::make_unique<ZoomToModelOperator>());

    view->addOperator(KeyCode::KeyF10, KeyCode::KeyF10,
                      std::make_unique<FpsCameraOperator>());

    view->addOperator(KeyCode::KeyO, std::make_unique<LoadSceneOperator>());
    view->addOperator(KeyCode::KeyI, std::make_unique<ImportSceneOperator>());

    view->addOperator(KeyCode::KeyS, std::make_unique<SaveSceneOperator>());

    view->addOperator(KeyCode::KeyBackspace,
                      std::make_unique<DeleteSelectedFacesOperator>());
    view->addOperator(KeyCode::KeyDelete,
                      std::make_unique<DeleteSelectedFacesOperator>());

    view->addOperator(KeyCode::KeyH, std::make_unique<ColorHolesOperator>());
    view->addOperator(KeyCode::KeyL,
                      std::make_unique<PaintBoundaryFacesOperator>());

    view->addOperator(KeyCode::KeyE, KeyCode::KeyE,
                      std::make_unique<EditMeshOperator>());
    view->addOperator(KeyCode::KeyT, KeyCode::KeyT,
                      std::make_unique<TransformMeshOperator>());

    view->addOperator(KeyCode::KeyB, KeyCode::KeyB,
                      std::make_unique<ScaleNodeOperator>());

    view->addOperator(KeyCode::KeyG, std::make_unique<ToggleOctreeOperator>());

    view->addOperator(KeyCode::KeyN,
                      std::make_unique<MoveOrthogonalFacesOperator>());

    view->addOperator(KeyCode::KeyJ, std::make_unique<ToggleAABBOperator>());

    view->addOperator(KeyCode::KeyR,
                      std::make_unique<ToggleBlackEdgesOperator>());

    view->addOperator(KeyCode::KeyP,
                      std::make_unique<TogglePbrOperator>());

    view->addOperator(KeyCode::KeyV, KeyCode::KeyV,
                      std::make_unique<EditVertexOperator>());
    view->addOperator(KeyCode::KeyY, KeyCode::KeyY,
                      std::make_unique<EditFaceOperator>());
    view->addOperator(KeyCode::KeyM, KeyCode::KeyM,
                      std::make_unique<DistanceMeasurementOperator>());
    view->addOperator(KeyCode::KeyA, KeyCode::KeyA,
                      std::make_unique<AngleMeasurementOperator>());
    view->addOperator(KeyCode::KeyU, KeyCode::KeyU,
                      std::make_unique<EdgeMeasurementOperator>());

    view->addOperator(KeyCode::KeyNum1, std::make_unique<LaplacianSmoothOperator>());
    view->addOperator(KeyCode::KeyNum2, std::make_unique<RemoveDegenerateFacesOperator>());
    view->addOperator(KeyCode::KeyNum3, std::make_unique<WeldVerticesOperator>());
    view->addOperator(KeyCode::KeyNum4, std::make_unique<DecimateOperator>());
    view->addOperator(KeyCode::KeyNum5, std::make_unique<SubdivideOperator>());
    view->addOperator(KeyCode::KeyNum6, std::make_unique<EqualizeTrianglesOperator>());

    View *viewPtr = view.get();
    views.push_back(std::move(view));
    return viewPtr;
}


void Application::requestExit()
{
    m_shouldExit = true;
    if (m_onExitRequested)
    {
        m_onExitRequested();
    }
}

bool Application::shouldExit() const
{
    return m_shouldExit;
}

void Application::setOnModelReplaced(std::function<void()> callback)
{
    m_onModelReplaced = std::move(callback);
}

void Application::setOnDocumentOpened(std::function<void(int)> callback)
{
    m_onDocumentOpened = std::move(callback);
}

void Application::setOnExitRequested(std::function<void()> callback)
{
    m_onExitRequested = std::move(callback);
}