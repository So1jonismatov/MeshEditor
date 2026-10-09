#include "WeldVerticesOperator.h"
#include "Application.h"
#include "Threading/TaskRunner.h"
#include "Model/Graph/Model.h"
#include "Model/Geometry/Mesh.h"
#include "Model/Geometry/Geometry.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>
#include <glm/glm.hpp>

void WeldVerticesOperator::onEnter(View &view)
{
    Node* selectedNode = Application::getInstance()->getSelectedNode();
    if (!selectedNode || !selectedNode->getMesh()) {
        std::cout << "[Weld] No mesh node selected." << std::endl;
        return;
    }

    Mesh*     mesh  = selectedNode->getMesh();
    Geometry* geo   = mesh->getGeometry();
    Model*    model = Application::getInstance()->getModel();

    if (!model) return;
    if (model->isBusy()) {
        std::cout << "[Weld] A background task is already running." << std::endl;
        return;
    }
    model->setBusy(true);

    const float epsilon = std::max(1e-6f, m_epsilon);

    std::cout << "[Weld] Snapping overlapping vertices (tol=" << epsilon
              << ") asynchronously (sort-sweep)..." << std::endl;

    auto asyncCtx = model->getAsyncContext();

    Application::getInstance()->getTaskRunner().run(
        // ── BACKGROUND THREAD: O(N log N) sort-sweep + build buffers ──────
        // Only a brief lock is held (< 0.5ms) to copy the flat vectors.
        [geo, model, epsilon, asyncCtx]() mutable -> std::tuple<int, HalfEdgeTable, PrebuiltBuffers, PrebuiltOctree>
        {
            HalfEdgeTable het;
            {
                std::unique_lock<std::mutex> lock(asyncCtx->mutex);
                if (!asyncCtx->alive.load(std::memory_order_acquire)) return {};
                het = geo->getHalfEdgeTable();
            }

            const auto& vertices = het.getVertices();
            auto positions = het.getPositions();
            const size_t N = vertices.size();

            // Build a sorted index list by X coordinate for sweep.
            std::vector<size_t> sortedIdx;
            sortedIdx.reserve(N);
            for (size_t i = 0; i < N; ++i)
                if (vertices[i].heh.index != -1)
                    sortedIdx.push_back(i);

            std::sort(sortedIdx.begin(), sortedIdx.end(),
                      [&](size_t a, size_t b) {
                          return positions[a].x < positions[b].x;
                      });

            int weldedCount = 0;
            const float epsSq = epsilon * epsilon;

            const size_t M = sortedIdx.size();
            for (size_t ii = 0; ii < M; ++ii)
            {
                size_t i = sortedIdx[ii];
                for (size_t jj = ii + 1; jj < M; ++jj)
                {
                    size_t j = sortedIdx[jj];
                    if (positions[j].x - positions[i].x > epsilon) break;

                    glm::vec3 diff = positions[i] - positions[j];
                    if (glm::dot(diff, diff) < epsSq)
                    {
                        positions[j] = positions[i]; // snap j onto i
                        ++weldedCount;
                    }
                }
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

            return std::make_tuple(weldedCount, std::move(het), std::move(prebuiltBuffers), std::move(prebuiltOctree));
        },
        // ── MAIN THREAD: Adopt prebuilt buffers & prebuilt octree ───────────
        [mesh, geo, model, asyncCtx](std::tuple<int, HalfEdgeTable, PrebuiltBuffers, PrebuiltOctree> result)
        {
            std::unique_lock<std::mutex> lock(asyncCtx->mutex);
            if (!asyncCtx->alive.load(std::memory_order_acquire)) return;

            int weldedCount = std::get<0>(result);
            HalfEdgeTable &newHet = std::get<1>(result);
            PrebuiltBuffers &prebuiltBuffers = std::get<2>(result);
            PrebuiltOctree &prebuiltOctree = std::get<3>(result);

            if (weldedCount > 0)
            {
                IRenderSystem *rs = Application::getInstance()->getRenderSystem();
                if (rs) {
                    geo->adoptPrebuilt(std::move(newHet), std::move(prebuiltBuffers), std::move(prebuiltOctree), *rs);
                } else {
                    geo->setHalfEdgeTable(std::move(newHet));
                }
            } std::cout << "[Weld] Finished! Snapped " << weldedCount << " vertices." << std::endl;
            model->setBusy(false);
        }
    );
}
