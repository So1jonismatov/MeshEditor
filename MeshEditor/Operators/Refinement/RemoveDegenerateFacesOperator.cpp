#include "RemoveDegenerateFacesOperator.h"
#include "Application.h"
#include "Threading/TaskRunner.h"
#include "Model/Graph/Model.h"
#include "Model/Geometry/Mesh.h"
#include "Model/Geometry/Geometry.h"
#include <iostream>
#include <vector>
#include <mutex>
#include <glm/glm.hpp>
#include <algorithm>

void RemoveDegenerateFacesOperator::onEnter(View &view)
{
    Node* selectedNode = Application::getInstance()->getSelectedNode();
    if (!selectedNode || !selectedNode->getMesh()) {
        std::cout << "[Degenerate] No mesh node selected." << std::endl;
        return;
    }

    Mesh*     mesh  = selectedNode->getMesh();
    Geometry* geo   = mesh->getGeometry();
    Model*    model = Application::getInstance()->getModel();

    if (!model) return;
    if (model->isBusy()) {
        std::cout << "[Degenerate] A background task is already running." << std::endl;
        return;
    }
    model->setBusy(true);

    std::cout << "[Degenerate] Finding degenerate faces asynchronously..." << std::endl;

    auto asyncCtx = model->getAsyncContext();

    Application::getInstance()->getTaskRunner().run(
        // ── BACKGROUND THREAD: Find degenerate faces, delete, and prebuild buffers ──
        // Snapshot taken inside background thread to eliminate UI thread stutter.
        [geo, model, asyncCtx]() mutable -> std::tuple<int, HalfEdgeTable, PrebuiltBuffers, PrebuiltOctree>
        {
            HalfEdgeTable het;
            {
                std::unique_lock<std::mutex> lock(asyncCtx->mutex);
                if (!asyncCtx->alive.load(std::memory_order_acquire)) return {};
                het = geo->getHalfEdgeTable();
            }

            const auto &faces     = het.getFaces();
            const auto &halfEdges = het.getHalfEdges();
            const auto &positions = het.getPositions();
            const int64_t heSize  = static_cast<int64_t>(halfEdges.size());
            const int64_t posSize = static_cast<int64_t>(positions.size());

            std::vector<int64_t> degenerateFaces;
            for (size_t i = 0; i < faces.size(); ++i)
            {
                if (faces[i].heh.index == -1) continue;

                HalfEdgeHandle curr = faces[i].heh;
                std::vector<glm::vec3> pts;
                int guard = 0;
                do {
                    if (curr.index < 0 || curr.index >= heSize) break;
                    VertexHandle dst = halfEdges[curr.index].dst;
                    if (dst.index >= 0 && dst.index < posSize)
                        pts.push_back(positions[dst.index]);
                    curr = halfEdges[curr.index].next;
                    if (++guard > 128) break;
                } while (curr != faces[i].heh && curr.index != -1);

                if (pts.size() < 3) {
                    degenerateFaces.push_back(static_cast<int64_t>(i));
                    continue;
                }

                glm::vec3 crossSum(0.0f);
                for (size_t j = 1; j + 1 < pts.size(); ++j)
                    crossSum += glm::cross(pts[j] - pts[0], pts[j + 1] - pts[0]);

                float area = 0.5f * glm::length(crossSum);
                if (area < 1e-6f)
                    degenerateFaces.push_back(static_cast<int64_t>(i));
            }

            const int count = static_cast<int>(degenerateFaces.size());
            if (count > 0)
            {
                std::sort(degenerateFaces.begin(), degenerateFaces.end(), std::greater<int64_t>());
                for (int64_t fi : degenerateFaces)
                {
                    het.deleteFaceLocally(FaceHandle{fi});
                }
            }

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
        },
        // ── MAIN THREAD: Adopt prebuilt buffers & prebuilt octree ───────────
        [mesh, geo, model, asyncCtx](std::tuple<int, HalfEdgeTable, PrebuiltBuffers, PrebuiltOctree> result)
        {
            std::unique_lock<std::mutex> lock(asyncCtx->mutex);
            if (!asyncCtx->alive.load(std::memory_order_acquire)) return;

            const int count = std::get<0>(result);
            auto &newHet = std::get<1>(result);
            auto &prebuiltBuffers = std::get<2>(result);
            auto &prebuiltOctree = std::get<3>(result);

            if (count == 0) {
                std::cout << "[Degenerate] No degenerate faces found." << std::endl;
            } else {
                std::cout << "[Degenerate] Removed " << count << " degenerate faces." << std::endl;
                IRenderSystem *rs = Application::getInstance()->getRenderSystem();
                if (rs) {
                    geo->adoptPrebuilt(std::move(newHet), std::move(prebuiltBuffers), std::move(prebuiltOctree), *rs);
                } else {
                    geo->setHalfEdgeTable(std::move(newHet));
                }
            }

            model->setBusy(false);
        }
    );
}
