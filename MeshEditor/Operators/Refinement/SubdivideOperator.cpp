#include "SubdivideOperator.h"
#include "Application.h"
#include "Threading/TaskRunner.h"
#include "Model/Graph/Model.h"
#include "Model/Geometry/Mesh.h"
#include "Model/Geometry/Geometry.h"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <mutex>
#include <glm/glm.hpp>

SubdivideOperator::SubdivideOperator(int levels, bool onlySelected)
    : m_levels(levels), m_onlySelected(onlySelected)
{
}

void SubdivideOperator::onEnter(View &view)
{
    Node* selectedNode = Application::getInstance()->getSelectedNode();
    if (!selectedNode || !selectedNode->getMesh()) {
        std::cout << "[Subdivide] No mesh node selected." << std::endl;
        return;
    }

    Mesh*     mesh  = selectedNode->getMesh();
    Geometry* geo   = mesh->getGeometry();
    Model*    model = Application::getInstance()->getModel();

    if (!model) {
        std::cout << "[Subdivide] No model available." << std::endl;
        return;
    }

    if (model->isBusy()) {
        std::cout << "[Subdivide] A background task is already running." << std::endl;
        return;
    }
    model->setBusy(true);

    const int levels = std::clamp(m_levels, 1, 4);
    const bool onlySelected = m_onlySelected && (mesh->getSelectedFace().index >= 0);
    const int64_t selectedFaceIdx = onlySelected ? mesh->getSelectedFace().index : -1;

    std::cout << "[Subdivide] Starting 1-to-4 Subdivision ("
              << levels << " level" << (levels > 1 ? "s" : "")
              << (onlySelected ? ", selected face only" : ", entire mesh")
              << ") asynchronously..." << std::endl;

    auto asyncCtx = model->getAsyncContext();
    Application::getInstance()->getTaskRunner().run(
        // ── BACKGROUND THREAD: Pure CPU Math & Prebuilding ──────────────────
        [geo, model, levels, onlySelected, selectedFaceIdx, asyncCtx]() mutable
            -> std::tuple<int, HalfEdgeTable, PrebuiltBuffers, PrebuiltOctree>
        {
            HalfEdgeTable currentHet;
            {
                std::unique_lock<std::mutex> lock(asyncCtx->mutex);
                if (!asyncCtx->alive.load(std::memory_order_acquire)) return {};
                currentHet = geo->getHalfEdgeTable();
            }

            int generatedFaces = 0;

            struct EdgeKey
            {
                int64_t v1;
                int64_t v2;
                bool operator==(const EdgeKey &o) const { return v1 == o.v1 && v2 == o.v2; }
            };
            struct EdgeKeyHasher
            {
                std::size_t operator()(const EdgeKey &k) const noexcept
                {
                    return std::hash<int64_t>()(k.v1) ^ (std::hash<int64_t>()(k.v2) << 1);
                }
            };

            for (int lvl = 0; lvl < levels; ++lvl)
            {
                const auto &faces = currentHet.getFaces();
                const auto &halfEdges = currentHet.getHalfEdges();
                const auto &positions = currentHet.getPositions();
                const auto &uvs = currentHet.getUVs();

                const int64_t heSize = static_cast<int64_t>(halfEdges.size());
                const int64_t posSize = static_cast<int64_t>(positions.size());
                const int64_t uvSize = static_cast<int64_t>(uvs.size());

                HalfEdgeTable nextHet;
                std::vector<VertexHandle> nextVertexHandles;
                nextVertexHandles.reserve(positions.size() * 2);

                for (const auto &p : positions)
                {
                    nextVertexHandles.push_back(nextHet.addVertex(p));
                }

                std::unordered_map<EdgeKey, VertexHandle, EdgeKeyHasher> midpointMap;

                auto getOrCreateMidpoint = [&](int64_t vA, int64_t vB,
                                               HalfEdgeHandle heA, HalfEdgeHandle heB) -> VertexHandle
                {
                    int64_t minV = std::min(vA, vB);
                    int64_t maxV = std::max(vA, vB);
                    EdgeKey key{minV, maxV};

                    auto it = midpointMap.find(key);
                    if (it != midpointMap.end())
                        return it->second;

                    glm::vec3 pMid = 0.5f * (positions[vA] + positions[vB]);
                    VertexHandle vhMid = nextHet.addVertex(pMid);
                    midpointMap[key] = vhMid;
                    return vhMid;
                };

                for (size_t fi = 0; fi < faces.size(); ++fi)
                {
                    if (faces[fi].heh.index == -1) continue;

                    // If subdividing only a selected face on level 0
                    if (onlySelected && lvl == 0 && static_cast<int64_t>(fi) != selectedFaceIdx)
                    {
                        // Copy unselected face as-is
                        HalfEdgeHandle start = faces[fi].heh;
                        HalfEdgeHandle curr = start;
                        std::vector<VertexHandle> polyVerts;
                        int guard = 0;
                        do {
                            if (curr.index < 0 || curr.index >= heSize) break;
                            const HalfEdge &he = halfEdges[curr.index];
                            if (he.dst.index >= 0 && he.dst.index < posSize)
                                polyVerts.push_back(nextVertexHandles[he.dst.index]);
                            curr = he.next;
                        } while (curr != start && curr.index != -1 && ++guard < 64);

                        if (polyVerts.size() >= 3)
                        {
                            for (size_t j = 1; j + 1 < polyVerts.size(); ++j)
                                nextHet.addFace(polyVerts[0], polyVerts[j], polyVerts[j + 1]);
                        }
                        continue;
                    }

                    // Collect polygon vertices for this face
                    HalfEdgeHandle start = faces[fi].heh;
                    HalfEdgeHandle curr = start;
                    std::vector<int64_t> polyVIdx;
                    std::vector<HalfEdgeHandle> polyHEs;
                    int guard = 0;
                    do {
                        if (curr.index < 0 || curr.index >= heSize) break;
                        const HalfEdge &he = halfEdges[curr.index];
                        if (he.dst.index >= 0 && he.dst.index < posSize)
                        {
                            polyVIdx.push_back(he.dst.index);
                            polyHEs.push_back(curr);
                        }
                        curr = he.next;
                    } while (curr != start && curr.index != -1 && ++guard < 64);

                    if (polyVIdx.size() < 3) continue;

                    // Triangulate into triangles if polygon has > 3 vertices
                    for (size_t t = 1; t + 1 < polyVIdx.size(); ++t)
                    {
                        int64_t v0 = polyVIdx[0];
                        int64_t v1 = polyVIdx[t];
                        int64_t v2 = polyVIdx[t + 1];

                        VertexHandle vh0 = nextVertexHandles[v0];
                        VertexHandle vh1 = nextVertexHandles[v1];
                        VertexHandle vh2 = nextVertexHandles[v2];

                        VertexHandle m01 = getOrCreateMidpoint(v0, v1, polyHEs[0], polyHEs[t]);
                        VertexHandle m12 = getOrCreateMidpoint(v1, v2, polyHEs[t], polyHEs[t + 1]);
                        VertexHandle m20 = getOrCreateMidpoint(v2, v0, polyHEs[t + 1], polyHEs[0]);

                        // 4 sub-triangles
                        nextHet.addFace(vh0, m01, m20);
                        nextHet.addFace(vh1, m12, m01);
                        nextHet.addFace(vh2, m20, m12);
                        nextHet.addFace(m01, m12, m20);

                        generatedFaces += 4;
                    }
                }

                nextHet.connectTwins();
                currentHet = std::move(nextHet);
            }

            // Prebuild GPU buffers & Face Octree asynchronously
            PrebuiltBuffers prebuiltBuffers = GeometryBuffers::buildBuffersFromHET(currentHet);
            bbox bounds;
            const auto &pos = currentHet.getPositions();
            if (!pos.empty()) {
                bounds = bbox{pos[0], pos[0]};
                for (const auto &p : pos) {
                    bounds.min = glm::min(bounds.min, p);
                    bounds.max = glm::max(bounds.max, p);
                }
            }
            PrebuiltOctree prebuiltOctree = FaceOctree::buildOctreeFromHET(currentHet, bounds);

            return std::make_tuple(generatedFaces, std::move(currentHet),
                                   std::move(prebuiltBuffers), std::move(prebuiltOctree));
        },
        // ── MAIN THREAD: Atomic UI Adoption (< 0.1ms) ───────────────────────
        [mesh, geo, model, asyncCtx](std::tuple<int, HalfEdgeTable, PrebuiltBuffers, PrebuiltOctree> result)
        {
            std::unique_lock<std::mutex> lock(asyncCtx->mutex);
            if (!asyncCtx->alive.load(std::memory_order_acquire)) return;

            int faceCount = std::get<0>(result);
            HalfEdgeTable &newHet = std::get<1>(result);
            PrebuiltBuffers &prebuiltBuffers = std::get<2>(result);
            PrebuiltOctree &prebuiltOctree = std::get<3>(result);

            IRenderSystem *rs = Application::getInstance()->getRenderSystem();
            if (rs) {
                geo->adoptPrebuilt(std::move(newHet), std::move(prebuiltBuffers), std::move(prebuiltOctree), *rs);
            } else {
                geo->setHalfEdgeTable(std::move(newHet));
            }

            mesh->clearSelectedFace();
            std::cout << "[Subdivide] Finished! Mesh has " << faceCount
                      << " subdivided faces." << std::endl;

            model->setBusy(false);
        }
    );
}
