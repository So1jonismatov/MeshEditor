#include "View.h"
#include "DynamicLibrary.h"
#include "Operator.h"
#include "Application.h"
#include "IWindow.h"
#include "IRenderSystem.h"
#include "Manipulator.h"
#include "utils/CreatePrimitives.h"
#include "Octree.h"
#include "ViewUtils.h"
#include <limits>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <functional>

namespace
{
struct PickSlot
{
    Mesh *mesh = nullptr;
    Node *node = nullptr;
    uint32_t base = 0;
    uint32_t triCount = 0;
};
} // namespace

// The parent's absolute transform is accumulated down the recursion — one
// mat4 multiply per node. Calling Node::calcAbsoluteTransform() here instead
// would re-walk the whole parent chain per node (O(nodes × depth) multiplies
// per frame), which dominated the frame time on many-node scenes.
static void renderNode(IRenderSystem &rs, const Node *node,
                       const glm::mat4 &parentTransform)
{
    if (!node)
        return;

    const glm::mat4 transform = parentTransform * node->getRelativeTransform();

    if (Mesh *mesh = node->getMesh())
    {
        rs.setWorldMatrix(transform);
        mesh->renderIncremental(rs, false);
    }

    for (const auto &child : node->getChildren())
    {
        renderNode(rs, child.get(), transform);
    }
}


View::View(IRenderSystem &rs, IWindow *window)
    : m_renderSystem(&rs), m_window(window)
{
    if (m_window)
    {
        m_viewport.setViewportSize(m_window->getWidth(), m_window->getHeight());
        m_viewport.getCamera().setEyeTargetUp(glm::vec3(3.0f, 2.0f, 4.0f),
                                              glm::vec3(0.0f, 0.0f, 0.0f),
                                              glm::vec3(0.0f, 1.0f, 0.0f));
        setupWindowCallbacks();
    }
}

void View::setupWindowCallbacks()
{
    m_window->setKeyCallback(
        [&](KeyCode key, Action action, Modifier mods)
        {
            operatorDispatcher.processKeyboardInput(*this, key, action, mods);

            if (key == KeyCode::KeyEscape && action == Action::Press)
            {
                if (!operatorDispatcher.hasActiveExitKey(KeyCode::KeyEscape))
                {
                    if (Application::getInstance())
                    {
                        Application::getInstance()->requestExit();
                    }
                }
            }
        });

    m_window->setCursorPosCallback(
        [&](double x, double y)
        { operatorDispatcher.processMouseMove(*this, x, y); });

    m_window->setMouseCallback(
        [&](ButtonCode button, Action action, Modifier mods, double x, double y)
        {
            operatorDispatcher.processMouseInput(*this, button, action, mods, x,
                                                 y);
        });

    m_window->setScrollCallback(
        [&](double xoffset, double yoffset)
        {
            (void)xoffset;
            const double zoomFactor = std::pow(0.9, yoffset);
            m_viewport.getCamera().zoom(zoomFactor);
        });
}

View::~View()
{
    if (!m_window)
        return;
    if (m_destroyWindow)
        m_destroyWindow(m_window); // DLL-created: free on the DLL's heap
    else
        delete m_window; // adopted (e.g. QtWindow): allocated in the exe
}

void View::update()
{
    if (!m_model || !m_renderSystem)
        return;

    const auto now = std::chrono::steady_clock::now();
    double dt = 0.0;
    if (m_lastUpdateTime.time_since_epoch().count() != 0)
        dt = std::min(
            std::chrono::duration<double>(now - m_lastUpdateTime).count(), 0.1);
    m_lastUpdateTime = now;
    m_viewport.getCamera().tickTransition(dt);

    operatorDispatcher.processUpdate(*this);

    if (m_model)
    {
        std::lock_guard<std::mutex> lock(m_model->getModelMutex());

        glm::vec3 sceneMin, sceneMax;
        if (getSceneBoundingBox(m_model, sceneMin, sceneMax))
        {
            m_viewport.adjustNearFarPlanes(sceneMin, sceneMax);
        }

        ViewportMatrices matrices = m_viewport.calcMatrices();
        m_renderSystem->setViewProjectionMatrix(matrices.viewProjection);
        m_renderSystem->setViewPosition(matrices.eye);

        const int vpW = static_cast<int>(m_viewport.getWidth());
        const int vpH = static_cast<int>(m_viewport.getHeight());
        m_renderSystem->setScreenSize(vpW, vpH);

        // ── Pass 1: Opaque pre-pass (sky + opaque geometry -> FBO) ──────────
        m_renderSystem->beginOpaqueCapture();

        m_renderSystem->renderEnvironmentBackground();

        m_renderSystem->setWorldMatrix(glm::mat4(1.0f));
        m_renderSystem->setMaterial(glm::vec3(1.0f), glm::vec3(1.0f),
                                    glm::vec3(0.0f), 1.0f);

        static const std::vector<Vertex> axes = Utils::buildCoordinateAxes();
        if (!m_axesUploaded)
        {
            m_renderSystem->uploadTriangleSoup(&axes, axes);
            m_axesUploaded = true;
        }
        m_renderSystem->drawLineBuffer(&axes, axes.size());

        const glm::mat4 identity(1.0f);
        for (const auto &node : m_model->getNodes())
        {
            renderNode(*m_renderSystem, node.get(), identity);
        }

        m_renderSystem->endOpaqueCapture();

        // ── Pass 2: Full HDR scene render (glass fragments read the FBO) ────────
        m_renderSystem->beginHdrCapture();
        m_renderSystem->renderEnvironmentBackground();

        m_renderSystem->setWorldMatrix(glm::mat4(1.0f));
        m_renderSystem->setMaterial(glm::vec3(1.0f), glm::vec3(1.0f),
                                    glm::vec3(0.0f), 1.0f);
        m_renderSystem->drawLineBuffer(&axes, axes.size());

        for (const auto &node : m_model->getNodes())
        {
            renderNode(*m_renderSystem, node.get(), identity);
        }

        // ── Pass 3: Post-Processing (Bloom + Tone Map) ────────
        m_renderSystem->renderPostProcessing();
    }
}

void View::setModel(Model *model)
{
    m_model = model;
}

Model *View::getModel() const
{
    return m_model;
}

void View::addOperator(KeyCode enterKey, KeyCode exitKey,
                       std::unique_ptr<Operator> op)
{
    operatorDispatcher.addOperator(enterKey, exitKey, std::move(op));
}

void View::addOperator(ButtonCode button, std::unique_ptr<Operator> op)
{
    operatorDispatcher.addOperator(button, std::move(op));
}

void View::addOperator(KeyCode key, std::unique_ptr<Operator> op)
{
    operatorDispatcher.addOperator(key, std::move(op));
}

void View::resetOperatorState()
{
    operatorDispatcher.resetActiveOperators(*this);
}

bool View::isOperatorActive(KeyCode enterKey) const
{
    return operatorDispatcher.isOperatorActive(enterKey);
}

Viewport &View::getViewport()
{
    return m_viewport;
}

const Viewport &View::getViewport() const
{
    return m_viewport;
}

bool View::rayTriangleIntersection(const glm::vec3 &orig, const glm::vec3 &dir,
                                   const glm::vec3 &v0, const glm::vec3 &v1,
                                   const glm::vec3 &v2, float &tOut) const
{
    auto dot = [](const glm::vec3 &a, const glm::vec3 &b) -> float
    { return a.x * b.x + a.y * b.y + a.z * b.z; };

    auto cross = [](const glm::vec3 &a, const glm::vec3 &b) -> glm::vec3
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    };

    glm::vec3 n = cross(v1 - v0, v2 - v0);

    float denom = dot(dir, n);
    if (std::abs(denom) < 1e-8f)
        return false;

    float t = dot(v0 - orig, n) / denom;
    if (t < 1e-5f)
        return false;

    glm::vec3 p = {orig.x + t * dir.x, orig.y + t * dir.y, orig.z + t * dir.z};

    float invArea2 = 1.0f / dot(n, n);
    float w0 = dot(cross(v1 - p, v2 - p), n) * invArea2;
    float w1 = dot(cross(v2 - p, v0 - p), n) * invArea2;
    float w2 = 1.0f - w0 - w1;

    if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
        return false;

    tOut = t;
    return true;
}

void View::collectNodeContacts(const Node *node, const ray &rayWorld,
                               FilterValue filterValues,
                               std::vector<Contact> &contacts,
                               const glm::mat4 &parentTransform) const
{
    if (!node)
        return;

    // Accumulated like in renderNode: one multiply per node instead of a
    // full parent-chain walk per node (this runs per mouse move during
    // shift-drag selection).
    const glm::mat4 absTrf = parentTransform * node->getRelativeTransform();

    const Mesh *mesh = node->getMesh();
    if (mesh)
    {
        // Check if this node or any ancestor is a manipulator.
        bool isManip = false;
        const Node *curr = node;
        while (curr)
        {
            if (curr->isManipulator())
            {
                isManip = true;
                break;
            }
            curr = curr->getParent();
        }

        // If we only want manipulators, skip non-manipulator meshes.
        // If we only want nodes (model meshes), skip manipulator meshes.
        bool shouldTestMesh = true;
        if (filterValues == FilterValue::Manipulator && !isManip)
            shouldTestMesh = false;
        else if (filterValues == FilterValue::Node && isManip)
            shouldTestMesh = false;

        if (shouldTestMesh)
        {
            glm::mat4 invM = glm::inverse(absTrf);

            glm::vec3 localOrig =
                glm::vec3(invM * glm::vec4(rayWorld.orig, 1.0f));
            glm::vec3 localDir =
                glm::normalize(glm::vec3(invM * glm::vec4(rayWorld.dir, 0.0f)));

            bbox meshBox = mesh->getBoundingBox();
            float tMesh;
            if (rayAABBIntersection(localOrig, localDir, meshBox.min,
                                    meshBox.max, tMesh))
            {
                const auto &faces = mesh->getHalfEdgeTable().getFaces();

                const OctreeNode *octree = mesh->getFaceOctree();
                std::vector<size_t> octCandidates;
                octCandidates.reserve(256);

                if (octree)
                {
                    queryFaceOctree(octree, localOrig, localDir, octCandidates);
                }

                for (size_t lf : mesh->getLooseFaces())
                {
                    octCandidates.push_back(lf);
                }

                std::sort(octCandidates.begin(), octCandidates.end());
                octCandidates.erase(
                    std::unique(octCandidates.begin(), octCandidates.end()),
                    octCandidates.end());

                // One scratch polygon reused across all candidate faces —
                // this loop runs per mouse move during shift-drag selection,
                // so a fresh vector per face was a measurable allocation cost.
                std::vector<glm::vec3> poly;
                for (size_t fi : octCandidates)
                {
                    if (fi >= faces.size() || faces[fi].heh.index == -1)
                        continue;

                    mesh->collectFacePolygon(
                        FaceHandle{static_cast<int64_t>(fi)}, poly);
                    if (poly.size() < 3)
                        continue;

                    for (size_t j = 1; j + 1 < poly.size(); ++j)
                    {
                        float tLocal;
                        if (rayTriangleIntersection(localOrig, localDir,
                                                    poly[0], poly[j],
                                                    poly[j + 1], tLocal))
                        {
                            glm::vec3 pLocal = {
                                localOrig.x + tLocal * localDir.x,
                                localOrig.y + tLocal * localDir.y,
                                localOrig.z + tLocal * localDir.z};
                            glm::vec3 pWorld =
                                glm::vec3(absTrf * glm::vec4(pLocal, 1.0f));

                            Contact c;
                            c.face = FaceHandle{static_cast<int64_t>(fi)};
                            c.node = const_cast<Node *>(node);
                            c.distance = glm::distance(rayWorld.orig, pWorld);
                            c.position = pWorld;
                            contacts.push_back(c);
                            break;
                        }
                    }
                }
            }
        }
    }

    for (const auto &child : node->getChildren())
        collectNodeContacts(child.get(), rayWorld, filterValues, contacts,
                            absTrf);
}

std::vector<Contact> View::raycast(double x, double y,
                                   FilterValue filterValues) const
{
    std::vector<Contact> contacts;
    if (!m_model)
        return contacts;

    ray rayWorld = m_viewport.calcCursorRay(x, y);

    const glm::mat4 identity(1.0f);
    for (const auto &node : m_model->getNodes())
        collectNodeContacts(node.get(), rayWorld, filterValues, contacts,
                            identity);
    std::vector<Contact> filtered;
    filtered.reserve(contacts.size());
    for (const auto &c : contacts)
    {
        Node *pickedNode = c.node;
        Manipulator *pickedManipulator = nullptr;
        if (pickedNode)
        {
            Node *current = pickedNode;
            while (current)
            {
                auto manip = dynamic_cast<Manipulator *>(current);
                if (manip)
                {
                    pickedManipulator = manip;
                    pickedNode = manip;
                    break;
                }
                current = current->getParent();
            }
        }

        const bool isManip = pickedNode && pickedNode->isManipulator();
        if (filterValues == FilterValue::Node && isManip)
            continue;
        if (filterValues == FilterValue::Manipulator && !pickedManipulator)
            continue;

        Contact picked = c;
        if (pickedManipulator)
            picked.node = pickedManipulator;
        filtered.push_back(picked);
    }
    std::sort(filtered.begin(), filtered.end(),
              [](const Contact &a, const Contact &b)
              { return a.distance < b.distance; });
    return filtered;
}

std::optional<Contact> View::pickFace(double x, double y)
{
    if (m_pickMode == PickMode::Fbo)
        return fboPickFace(x, y);

    std::vector<Contact> contacts = raycast(x, y, FilterValue::Node);
    if (contacts.empty())
        return std::nullopt;
    return contacts.front();
}

std::optional<Contact> View::fboPickFace(double x, double y)
{
    if (!m_model || !m_renderSystem)
        return std::nullopt;

    const int w = static_cast<int>(m_viewport.getWidth());
    const int h = static_cast<int>(m_viewport.getHeight());
    if (w <= 0 || h <= 0)
        return std::nullopt;

    glm::vec3 sceneMin, sceneMax;
    if (getSceneBoundingBox(m_model, sceneMin, sceneMax))
    {
        m_viewport.adjustNearFarPlanes(sceneMin, sceneMax);
    }

    ViewportMatrices matrices = m_viewport.calcMatrices();
    m_renderSystem->setViewProjectionMatrix(matrices.viewProjection);

    std::vector<PickSlot> pickSlots;
    uint32_t running = 0;

    m_renderSystem->beginPickPass(w, h);

    std::function<void(Node *, const glm::mat4 &)> drawNode;
    drawNode = [&](Node *node, const glm::mat4 &parentTransform)
    {
        if (!node)
            return;

        const glm::mat4 absTrf = parentTransform * node->getRelativeTransform();

        // Skip manipulator gizmos — only model faces are pickable.
        bool isManip = false;
        for (Node *c = node; c; c = c->getParent())
        {
            if (c->isManipulator())
            {
                isManip = true;
                break;
            }
        }

        Mesh *mesh = node->getMesh();
        if (mesh && !isManip)
        {
            const size_t indexCount = mesh->getIndexCount();
            if (indexCount >= 3)
            {
                m_renderSystem->setWorldMatrix(absTrf);
                // The vertex/index buffer is keyed by the shared Geometry, so
                // instances of one geometry re-draw the same buffer with
                // their own world matrix and id range.
                m_renderSystem->drawIndexedForPick(mesh->getGeometry(),
                                                   indexCount, running);
                const uint32_t tri = static_cast<uint32_t>(indexCount / 3);
                PickSlot ps;
                ps.mesh = mesh;
                ps.node = node;
                ps.base = running;
                ps.triCount = tri;
                pickSlots.push_back(ps);
                running += tri;
            }
        }

        for (const auto &child : node->getChildren())
            drawNode(child.get(), absTrf);
    };

    const glm::mat4 identity(1.0f);
    for (const auto &node : m_model->getNodes())
        drawNode(node.get(), identity);

    // Window coordinates are top-left origin; glReadPixels is bottom-left.
    const int px = static_cast<int>(x);
    const int py = h - 1 - static_cast<int>(y);
    const uint32_t id = m_renderSystem->readPickId(px, py);
    m_renderSystem->endPickPass();

    if (id == 0x00FFFFFFu) // background clear colour == no hit
        return std::nullopt;

    for (const PickSlot &s : pickSlots)
    {
        if (id >= s.base && id < s.base + s.triCount)
        {
            FaceHandle fh = s.mesh->faceForTriangle(id - s.base);
            if (fh.index < 0)
                return std::nullopt;
            Contact c;
            c.face = fh;
            c.node = s.node;
            c.distance = 0.0f;
            return c;
        }
    }
    return std::nullopt;
}

IWindow *View::getWindow() const
{
    return m_window;
}

void View::setPickMode(PickMode mode)
{
    m_pickMode = mode;
}
PickMode View::getPickMode() const
{
    return m_pickMode;
}