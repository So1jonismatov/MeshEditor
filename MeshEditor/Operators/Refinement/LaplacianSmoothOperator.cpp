#include "LaplacianSmoothOperator.h"
#include "Application.h"
#include "Threading/TaskRunner.h"
#include "Model/Graph/Model.h"
#include "Model/Geometry/Mesh.h"
#include "Model/Geometry/Geometry.h"
#include <iostream>
#include <vector>
#include <mutex>
#include <glm/glm.hpp>

void LaplacianSmoothOperator::onEnter(View &view)
{
    Node* selectedNode = Application::getInstance()->getSelectedNode();
    if (!selectedNode || !selectedNode->getMesh()) {
        std::cout << "[Smooth] No mesh node selected." << std::endl;
        return;
    }

    Mesh*     mesh  = selectedNode->getMesh();
    Geometry* geo   = mesh->getGeometry();
    Model*    model = Application::getInstance()->getModel();

    if (!model) {
        std::cout << "[Smooth] No model available." << std::endl;
        return;
    }

    // Guard: don't start a second task if one is already running.
    if (model->isBusy()) {
        std::cout << "[Smooth] A background task is already running." << std::endl;
        return;
    }
    model->setBusy(true);

    const int passes = std::clamp(m_passes, 1, 30);
    const float lambda = std::clamp(m_lambda, 0.01f, 1.0f);

    std::cout << "[Smooth] Starting Laplacian Smoothing ("
              << passes << " pass" << (passes > 1 ? "es" : "")
              << ", lambda=" << lambda << ") asynchronously..." << std::endl;

    auto asyncCtx = model->getAsyncContext();
    Application::getInstance()->getTaskRunner().run(
        // ── BACKGROUND THREAD: Compute smoothed positions & build buffers ──
        // Only a brief lock is held (< 0.5ms) to copy the flat vectors.
        [geo, model, passes, lambda, asyncCtx]() mutable -> std::tuple<HalfEdgeTable, PrebuiltBuffers, PrebuiltOctree>
        {
            HalfEdgeTable het;
            {
                std::unique_lock<std::mutex> lock(asyncCtx->mutex);
                if (!asyncCtx->alive.load(std::memory_order_acquire)) return {};
                het = geo->getHalfEdgeTable();
            }

            const auto& vertices  = het.getVertices();
            const auto& halfEdges = het.getHalfEdges();
            const int64_t heSize  = static_cast<int64_t>(halfEdges.size());
            const int64_t posSize = static_cast<int64_t>(het.getPositions().size());
            std::vector<glm::vec3> positions = het.getPositions();

            for (int p = 0; p < passes; ++p)
            {
                std::vector<glm::vec3> newPositions = positions;

                for (size_t i = 0; i < vertices.size(); ++i)
                {
                    if (vertices[i].heh.index == -1) continue;

                    glm::vec3 sum(0.0f);
                    int count = 0;

                    HalfEdgeHandle curr  = vertices[i].heh;
                    HalfEdgeHandle start = curr;
                    int guard = 0;
                    do {
                        if (curr.index < 0 || curr.index >= heSize) break;
                        VertexHandle nb = halfEdges[curr.index].dst;
                        if (nb.index >= 0 && nb.index < posSize) {
                            sum += positions[nb.index];
                            ++count;
                        }
                        HalfEdgeHandle tw = halfEdges[curr.index].twin;
                        if (tw.index < 0 || tw.index >= heSize) break;
                        curr = halfEdges[tw.index].next;
                    } while (curr != start && curr.index != -1 && ++guard < 512);

                    if (count > 0)
                    {
                        glm::vec3 avg = sum / static_cast<float>(count);
                        newPositions[i] = positions[i] + lambda * (avg - positions[i]);
                    }
                }

                positions = std::move(newPositions);
            }

            het.setPositions(std::move(positions));

            PrebuiltBuffers prebuiltBuffers = GeometryBuffers::buildBuffersFromHET(het);
            bbox bounds;
            const auto &pos = het.getPositions();
            if (!pos.empty()) {
                bounds = bbox{pos[0], pos[0]};
                for (const auto &p : pos) {
                    bounds.min = glm::min(bounds.min, p);
                    bounds.max = glm::max(bounds.max, p);
                }
            }
            PrebuiltOctree prebuiltOctree = FaceOctree::buildOctreeFromHET(het, bounds);

            return std::make_tuple(std::move(het), std::move(prebuiltBuffers), std::move(prebuiltOctree));
        },
        // ── MAIN THREAD: Atomically adopt pre-built buffers & pre-built octree ─
        [mesh, geo, model, asyncCtx](std::tuple<HalfEdgeTable, PrebuiltBuffers, PrebuiltOctree> result)
        {
            std::unique_lock<std::mutex> lock(asyncCtx->mutex);
            if (!asyncCtx->alive.load(std::memory_order_acquire)) return;

            HalfEdgeTable &newHet = std::get<0>(result);
            PrebuiltBuffers &prebuiltBuffers = std::get<1>(result);
            PrebuiltOctree &prebuiltOctree = std::get<2>(result);

            IRenderSystem *rs = Application::getInstance()->getRenderSystem();
            if (rs) {
                geo->adoptPrebuilt(std::move(newHet), std::move(prebuiltBuffers), std::move(prebuiltOctree), *rs);
            } else {
                geo->setHalfEdgeTable(std::move(newHet));
            }

            std::cout << "[Smooth] Finished!" << std::endl;
            model->setBusy(false);
        }
    );
}
