#include "DecimateOperator.h"
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

void DecimateOperator::onEnter(View &view)
{
    Node* selectedNode = Application::getInstance()->getSelectedNode();
    if (!selectedNode || !selectedNode->getMesh()) {
        std::cout << "[Decimate] No mesh node selected." << std::endl;
        return;
    }

    Mesh*     mesh  = selectedNode->getMesh();
    Geometry* geo   = mesh->getGeometry();
    Model*    model = Application::getInstance()->getModel();

    if (!model) {
        std::cout << "[Decimate] No model available." << std::endl;
        return;
    }

    if (model->isBusy()) {
        std::cout << "[Decimate] A background task is already running." << std::endl;
        return;
    }
    model->setBusy(true);

    const double percent = std::clamp(m_percent, 1.0, 95.0);

    std::cout << "[Decimate] Starting Decimation ("
              << percent << "%) asynchronously..." << std::endl;

    auto asyncCtx = model->getAsyncContext();
    Application::getInstance()->getTaskRunner().run(
        // ── BACKGROUND THREAD: Zero locks held during computation! ─────────
        // Snapshot taken inside background thread to prevent UI thread stutter.
        [geo, model, percent, asyncCtx]() mutable -> std::tuple<int, HalfEdgeTable, PrebuiltBuffers, PrebuiltOctree>
        {
            HalfEdgeTable het;
            {
                std::unique_lock<std::mutex> lock(asyncCtx->mutex);
                if (!asyncCtx->alive.load(std::memory_order_acquire)) return {};
                het = geo->getHalfEdgeTable();
            }

            int collapsedCount = 0;

            // Count only LIVE faces for the target (dead faces have heh==-1).
            int liveFaces = 0;
            for (const auto& f : het.getFaces())
                if (f.heh.index != -1) ++liveFaces;

            auto makePayload = [&](int count) {
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
                return std::make_tuple(count, std::move(het), std::move(prebuiltBuffers), std::move(prebuiltOctree));
            };

            if (liveFaces == 0) {
                std::cout << "[Decimate] No live faces — mesh is empty." << std::endl;
                return makePayload(0);
            }

            // Target: collapse enough edges to remove ~percent% of live faces.
            const int targetCollapses = std::max(1, static_cast<int>(
                liveFaces * (percent / 100.0) / 2.0));

            struct EdgeCandidate {
                int64_t heh;
                float lenSq;
            };

            for (int pass = 0; pass < 8 && collapsedCount < targetCollapses; ++pass)
            {
                const auto& halfEdges = het.getHalfEdges();
                const auto& positions = het.getPositions();
                const int64_t posSize = static_cast<int64_t>(positions.size());

                std::vector<EdgeCandidate> candidates;
                candidates.reserve(halfEdges.size() / 2);

                for (size_t i = 0; i < halfEdges.size(); ++i)
                {
                    const auto& he = halfEdges[i];
                    // Interior edges only, taking one half of each twin pair to avoid duplicate testing
                    if (he.dst.index != -1 && he.fh.index != -1 && he.twin.index != -1)
                    {
                        if (he.twin.index > static_cast<int64_t>(i))
                        {
                            if (het.getHalfEdges()[he.twin.index].fh.index != -1)
                            {
                                int64_t src = het.sourceVertex(HalfEdgeHandle{static_cast<int64_t>(i)}).index;
                                int64_t dst = he.dst.index;
                                if (src >= 0 && dst >= 0 && src < posSize && dst < posSize)
                                {
                                    glm::vec3 d = positions[src] - positions[dst];
                                    float lenSq = glm::dot(d, d);
                                    candidates.push_back({static_cast<int64_t>(i), lenSq});
                                }
                            }
                        }
                    }
                }

                if (candidates.empty()) break;

                // Sort by shortest edge length for optimal geometric quality
                std::sort(candidates.begin(), candidates.end(),
                          [](const EdgeCandidate& a, const EdgeCandidate& b) {
                              return a.lenSq < b.lenSq;
                          });

                bool collapsedAny = false;
                for (const auto& cand : candidates)
                {
                    if (collapsedCount >= targetCollapses) break;

                    HalfEdgeHandle heh{cand.heh};
                    if (heh.index < 0 ||
                        heh.index >= static_cast<int64_t>(het.getHalfEdges().size()))
                        continue;

                    const auto& he = het.getHalfEdges()[heh.index];
                    if (he.dst.index == -1 || he.fh.index == -1)
                        continue;

                    if (het.canCollapse(heh) && het.collapseEdge(heh))
                    {
                        ++collapsedCount;
                        collapsedAny = true;
                    }
                }
                if (!collapsedAny) break;
            }

            return makePayload(collapsedCount);
        },
        // ── MAIN THREAD: Atomically adopt pre-built buffers & pre-built octree ─
        [mesh, geo, model, asyncCtx](std::tuple<int, HalfEdgeTable, PrebuiltBuffers, PrebuiltOctree> result)
        {
            std::unique_lock<std::mutex> lock(asyncCtx->mutex);
            if (!asyncCtx->alive.load(std::memory_order_acquire)) return;

            int collapsedCount = std::get<0>(result);
            HalfEdgeTable &newHet = std::get<1>(result);
            PrebuiltBuffers &prebuiltBuffers = std::get<2>(result);
            PrebuiltOctree &prebuiltOctree = std::get<3>(result);

            IRenderSystem *rs = Application::getInstance()->getRenderSystem();
            if (rs) {
                geo->adoptPrebuilt(std::move(newHet), std::move(prebuiltBuffers), std::move(prebuiltOctree), *rs);
            } else {
                geo->setHalfEdgeTable(std::move(newHet));
            }

            std::cout << "[Decimate] Finished! Collapsed " << collapsedCount
                      << " edges." << std::endl;

            model->setBusy(false);
        }
    );
}
