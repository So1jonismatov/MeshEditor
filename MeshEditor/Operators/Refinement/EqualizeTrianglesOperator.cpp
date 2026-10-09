#include "EqualizeTrianglesOperator.h"
#include "Application.h"
#include "Threading/TaskRunner.h"
#include "Model/Graph/Model.h"
#include "Model/Geometry/Mesh.h"
#include "Model/Geometry/Geometry.h"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <glm/glm.hpp>

EqualizeTrianglesOperator::EqualizeTrianglesOperator(float targetEdgeLength, int iterations)
    : m_targetEdgeLength(targetEdgeLength), m_iterations(iterations)
{
}

void EqualizeTrianglesOperator::onEnter(View &view)
{
    Node* selectedNode = Application::getInstance()->getSelectedNode();
    if (!selectedNode || !selectedNode->getMesh()) {
        std::cout << "[EqualizeTriangles] No mesh node selected." << std::endl;
        return;
    }

    Mesh*     mesh  = selectedNode->getMesh();
    Geometry* geo   = mesh->getGeometry();
    Model*    model = Application::getInstance()->getModel();

    if (!model) {
        std::cout << "[EqualizeTriangles] No model available." << std::endl;
        return;
    }

    if (model->isBusy()) {
        std::cout << "[EqualizeTriangles] A background task is already running." << std::endl;
        return;
    }
    model->setBusy(true);

    const float userTargetLength = m_targetEdgeLength;
    const int iterations = std::clamp(m_iterations, 1, 10);

    std::cout << "[EqualizeTriangles] Starting Isotropic Remeshing ("
              << iterations << " iteration" << (iterations > 1 ? "s" : "")
              << ") asynchronously..." << std::endl;

    auto asyncCtx = model->getAsyncContext();
    Application::getInstance()->getTaskRunner().run(
        [geo, model, userTargetLength, iterations, asyncCtx]() mutable
            -> std::tuple<int, HalfEdgeTable, PrebuiltBuffers, PrebuiltOctree>
        {
            HalfEdgeTable het;
            {
                std::unique_lock<std::mutex> lock(asyncCtx->mutex);
                if (!asyncCtx->alive.load(std::memory_order_acquire)) return {};
                het = geo->getHalfEdgeTable();
            }

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

            // Calculate initial average edge length
            float targetLength = userTargetLength;
            if (targetLength <= 0.0f) {
                double totalLen = 0.0;
                int edgeCount = 0;
                const auto &pos = het.getPositions();
                const auto &hes = het.getHalfEdges();
                for (const auto &he : hes) {
                    if (he.fh.index == -1) continue;
                    int64_t vDst = he.dst.index;
                    int64_t vSrc = het.sourceVertex(het.handle(he)).index;
                    if (vSrc >= 0 && vDst >= 0 && vSrc < vDst &&
                        vSrc < static_cast<int64_t>(pos.size()) && vDst < static_cast<int64_t>(pos.size())) {
                        totalLen += glm::length(pos[vDst] - pos[vSrc]);
                        edgeCount++;
                    }
                }
                targetLength = (edgeCount > 0) ? static_cast<float>(totalLen / edgeCount) : 1.0f;
            }

            const float L_max = (4.0f / 3.0f) * targetLength;
            const float L_min = (4.0f / 5.0f) * targetLength;

            for (int iter = 0; iter < iterations; ++iter)
            {
                // ── 1. Split edges longer than L_max ─────────────────────────
                const auto &faces = het.getFaces();
                const auto &halfEdges = het.getHalfEdges();
                const auto &positions = het.getPositions();
                const int64_t heSize = static_cast<int64_t>(halfEdges.size());
                const int64_t posSize = static_cast<int64_t>(positions.size());

                HalfEdgeTable splitHet;
                std::vector<VertexHandle> vHandles;
                vHandles.reserve(positions.size() * 2);
                for (const auto &p : positions) {
                    vHandles.push_back(splitHet.addVertex(p));
                }

                std::unordered_map<EdgeKey, VertexHandle, EdgeKeyHasher> midpoints;
                auto getOrCreateMidpoint = [&](int64_t vA, int64_t vB) -> VertexHandle {
                    int64_t minV = std::min(vA, vB);
                    int64_t maxV = std::max(vA, vB);
                    EdgeKey key{minV, maxV};
                    auto it = midpoints.find(key);
                    if (it != midpoints.end()) return it->second;

                    glm::vec3 mid = 0.5f * (positions[vA] + positions[vB]);
                    VertexHandle vh = splitHet.addVertex(mid);
                    midpoints[key] = vh;
                    return vh;
                };

                for (size_t fi = 0; fi < faces.size(); ++fi) {
                    if (faces[fi].heh.index == -1) continue;
                    HalfEdgeHandle h0 = faces[fi].heh;
                    if (h0.index < 0 || h0.index >= heSize) continue;
                    HalfEdgeHandle h1 = halfEdges[h0.index].next;
                    if (h1.index < 0 || h1.index >= heSize) continue;
                    HalfEdgeHandle h2 = halfEdges[h1.index].next;
                    if (h2.index < 0 || h2.index >= heSize) continue;

                    int64_t v0 = halfEdges[h2.index].dst.index;
                    int64_t v1 = halfEdges[h0.index].dst.index;
                    int64_t v2 = halfEdges[h1.index].dst.index;

                    if (v0 < 0 || v1 < 0 || v2 < 0 || v0 >= posSize || v1 >= posSize || v2 >= posSize)
                        continue;

                    float l0 = glm::length(positions[v1] - positions[v0]);
                    float l1 = glm::length(positions[v2] - positions[v1]);
                    float l2 = glm::length(positions[v0] - positions[v2]);

                    bool s0 = (l0 > L_max);
                    bool s1 = (l1 > L_max);
                    bool s2 = (l2 > L_max);

                    int splitCount = (s0 ? 1 : 0) + (s1 ? 1 : 0) + (s2 ? 1 : 0);

                    if (splitCount == 0) {
                        splitHet.addFace(vHandles[v0], vHandles[v1], vHandles[v2]);
                    } else if (splitCount == 1) {
                        if (s0) {
                            VertexHandle m0 = getOrCreateMidpoint(v0, v1);
                            splitHet.addFace(vHandles[v0], m0, vHandles[v2]);
                            splitHet.addFace(m0, vHandles[v1], vHandles[v2]);
                        } else if (s1) {
                            VertexHandle m1 = getOrCreateMidpoint(v1, v2);
                            splitHet.addFace(vHandles[v0], vHandles[v1], m1);
                            splitHet.addFace(vHandles[v0], m1, vHandles[v2]);
                        } else {
                            VertexHandle m2 = getOrCreateMidpoint(v2, v0);
                            splitHet.addFace(vHandles[v0], vHandles[v1], m2);
                            splitHet.addFace(m2, vHandles[v1], vHandles[v2]);
                        }
                    } else if (splitCount == 2) {
                        if (!s2) { // s0 and s1
                            VertexHandle m0 = getOrCreateMidpoint(v0, v1);
                            VertexHandle m1 = getOrCreateMidpoint(v1, v2);
                            glm::vec3 pm0 = splitHet.getPoint(m0);
                            glm::vec3 pm1 = splitHet.getPoint(m1);
                            if (glm::length(pm0 - positions[v2]) < glm::length(pm1 - positions[v0])) {
                                splitHet.addFace(vHandles[v0], m0, vHandles[v2]);
                                splitHet.addFace(m0, m1, vHandles[v2]);
                                splitHet.addFace(m0, vHandles[v1], m1);
                            } else {
                                splitHet.addFace(vHandles[v0], m0, m1);
                                splitHet.addFace(vHandles[v0], m1, vHandles[v2]);
                                splitHet.addFace(m0, vHandles[v1], m1);
                            }
                        } else if (!s0) { // s1 and s2
                            VertexHandle m1 = getOrCreateMidpoint(v1, v2);
                            VertexHandle m2 = getOrCreateMidpoint(v2, v0);
                            glm::vec3 pm1 = splitHet.getPoint(m1);
                            glm::vec3 pm2 = splitHet.getPoint(m2);
                            if (glm::length(pm1 - positions[v0]) < glm::length(pm2 - positions[v1])) {
                                splitHet.addFace(vHandles[v0], vHandles[v1], m1);
                                splitHet.addFace(vHandles[v0], m1, m2);
                                splitHet.addFace(m2, m1, vHandles[v2]);
                            } else {
                                splitHet.addFace(vHandles[v1], m1, m2);
                                splitHet.addFace(vHandles[v0], vHandles[v1], m2);
                                splitHet.addFace(m2, m1, vHandles[v2]);
                            }
                        } else { // s0 and s2
                            VertexHandle m0 = getOrCreateMidpoint(v0, v1);
                            VertexHandle m2 = getOrCreateMidpoint(v2, v0);
                            glm::vec3 pm0 = splitHet.getPoint(m0);
                            glm::vec3 pm2 = splitHet.getPoint(m2);
                            if (glm::length(pm0 - positions[v2]) < glm::length(pm2 - positions[v1])) {
                                splitHet.addFace(vHandles[v0], m0, m2);
                                splitHet.addFace(m0, vHandles[v2], m2);
                                splitHet.addFace(m0, vHandles[v1], vHandles[v2]);
                            } else {
                                splitHet.addFace(vHandles[v0], m0, m2);
                                splitHet.addFace(m2, m0, vHandles[v1]);
                                splitHet.addFace(m2, vHandles[v1], vHandles[v2]);
                            }
                        }
                    } else {
                        // All 3 split (1-to-4 subdivision)
                        VertexHandle m0 = getOrCreateMidpoint(v0, v1);
                        VertexHandle m1 = getOrCreateMidpoint(v1, v2);
                        VertexHandle m2 = getOrCreateMidpoint(v2, v0);
                        splitHet.addFace(vHandles[v0], m0, m2);
                        splitHet.addFace(m0, vHandles[v1], m1);
                        splitHet.addFace(m2, m1, vHandles[v2]);
                        splitHet.addFace(m0, m1, m2);
                    }
                }

                splitHet.connectTwins();
                het = std::move(splitHet);

                // ── 2. Collapse edges shorter than L_min ─────────────────────
                const auto &cHalfEdges = het.getHalfEdges();
                const auto &cPositions = het.getPositions();
                std::unordered_set<int64_t> visitedEdges;

                for (size_t hei = 0; hei < cHalfEdges.size(); ++hei) {
                    if (cHalfEdges[hei].fh.index == -1) continue;
                    HalfEdgeHandle h = het.handle(cHalfEdges[hei]);
                    int64_t vSrc = het.sourceVertex(h).index;
                    int64_t vDst = cHalfEdges[hei].dst.index;
                    if (vSrc < 0 || vDst < 0) continue;

                    int64_t minV = std::min(vSrc, vDst);
                    int64_t maxV = std::max(vSrc, vDst);
                    int64_t edgeId = (minV << 32) ^ maxV;
                    if (visitedEdges.count(edgeId)) continue;
                    visitedEdges.insert(edgeId);

                    float len = glm::length(cPositions[vDst] - cPositions[vSrc]);
                    if (len < L_min && het.canCollapse(h)) {
                        het.collapseEdge(h);
                    }
                }

                // ── 3. Tangential smoothing (relax vertices) ─────────────────
                auto &curPos = const_cast<std::vector<glm::vec3>&>(het.getPositions());
                const auto &curHEs = het.getHalfEdges();
                const auto &curVerts = het.getVertices();

                std::vector<glm::vec3> newPositions = curPos;

                for (size_t vi = 0; vi < curVerts.size(); ++vi) {
                    HalfEdgeHandle outH = curVerts[vi].heh;
                    if (outH.index == -1 || outH.index >= static_cast<int64_t>(curHEs.size()))
                        continue;

                    // Circulate 1-ring neighbors
                    std::vector<int64_t> neighbors;
                    HalfEdgeHandle curr = outH;
                    bool isBoundary = false;
                    int guard = 0;
                    do {
                        if (curr.index < 0 || curr.index >= static_cast<int64_t>(curHEs.size())) break;
                        int64_t nbr = curHEs[curr.index].dst.index;
                        if (nbr >= 0 && nbr < static_cast<int64_t>(curPos.size()))
                            neighbors.push_back(nbr);

                        HalfEdgeHandle tw = curHEs[curr.index].twin;
                        if (tw.index == -1 || tw.index >= static_cast<int64_t>(curHEs.size())) {
                            isBoundary = true;
                            break;
                        }
                        curr = curHEs[tw.index].next;
                    } while (curr != outH && curr.index != -1 && ++guard < 64);

                    if (isBoundary || neighbors.size() < 3)
                        continue;

                    glm::vec3 centroid(0.0f);
                    for (int64_t nbr : neighbors) {
                        centroid += curPos[nbr];
                    }
                    centroid /= static_cast<float>(neighbors.size());

                    glm::vec3 normal(0.0f);
                    for (size_t ni = 0; ni < neighbors.size(); ++ni) {
                        size_t nextNi = (ni + 1) % neighbors.size();
                        glm::vec3 e1 = curPos[neighbors[ni]] - curPos[vi];
                        glm::vec3 e2 = curPos[neighbors[nextNi]] - curPos[vi];
                        normal += glm::cross(e1, e2);
                    }
                    if (glm::dot(normal, normal) > 1e-8f) {
                        normal = glm::normalize(normal);
                        glm::vec3 disp = centroid - curPos[vi];
                        glm::vec3 tangentialDisp = disp - glm::dot(disp, normal) * normal;
                        newPositions[vi] = curPos[vi] + 0.5f * tangentialDisp;
                    }
                }

                curPos = std::move(newPositions);
            }

            int finalFaceCount = 0;
            for (const auto &f : het.getFaces()) {
                if (f.heh.index != -1) ++finalFaceCount;
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

            return {finalFaceCount, std::move(het), std::move(prebuiltBuffers), std::move(prebuiltOctree)};
        },
        // ── UI THREAD: Atomic UI Adoption (< 0.1ms) ───────────────────────
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
            std::cout << "[EqualizeTriangles] Finished! Mesh has " << faceCount
                      << " equalized triangles." << std::endl;

            model->setBusy(false);
        }
    );
}
