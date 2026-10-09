#include "FaceOctree.h"
#include "Geometry.h"
#include <algorithm>
#include <cmath>
#include <iostream>

FaceOctree::FaceOctree(const Geometry* geometry) : m_geometry(geometry)
{
}

const OctreeNode* FaceOctree::getFaceOctree() const
{
    if (!m_faceOctree)
    {
        updateFaceOctree();
    }
    else if (!m_looseFaces.empty())
    {
        std::vector<size_t> pending(m_looseFaces.begin(), m_looseFaces.end());
        m_looseFaces.clear();

        for (size_t fi : pending)
        {
            reinsertFaceIntoOctree(fi);
            if (m_faceOctreeNodes.find(fi) == m_faceOctreeNodes.end())
                m_looseFaces.insert(fi);
        }
    }
    return m_faceOctree.get();
}

const std::unordered_set<size_t>& FaceOctree::getLooseFaces() const
{
    return m_looseFaces;
}

void FaceOctree::markOctreeDirty()
{
    m_faceOctree.reset();
    clearFaceOctreeCache();
    m_looseFaces.clear();
}

void FaceOctree::flushOctreeLooseFaces()
{
    if (!m_faceOctree || m_looseFaces.empty())
        return;
    std::vector<size_t> pending(m_looseFaces.begin(), m_looseFaces.end());
    m_looseFaces.clear();
    for (size_t fi : pending)
    {
        reinsertFaceIntoOctree(fi);
        if (m_faceOctreeNodes.find(fi) == m_faceOctreeNodes.end())
            m_looseFaces.insert(fi);
    }
}

void FaceOctree::invalidateFaceInOctree(size_t faceIndex) const
{
    if (!m_faceOctree)
        return;

    removeFaceFromOctreeCache(faceIndex);
    m_looseFaces.insert(faceIndex);
}

void FaceOctree::removeFaceFromOctreeCache(size_t faceIndex) const
{
    auto it = m_faceOctreeNodes.find(faceIndex);
    if (it == m_faceOctreeNodes.end())
        return;

    std::vector<OctreeNode *> nodes = it->second;
    m_faceOctreeNodes.erase(it);

    for (OctreeNode *node : nodes)
    {
        if (!node)
            continue;

        auto &faces = node->faceIndices;
        faces.erase(std::remove(faces.begin(), faces.end(), faceIndex),
                    faces.end());
        adjustFaceOctreeCounts(node, -1);
        pruneFaceOctreeBranch(node);
    }
}

void FaceOctree::replaceFaceInOctreeCache(size_t oldIndex, size_t newIndex) const
{
    if (oldIndex == newIndex)
        return;

    auto it = m_faceOctreeNodes.find(oldIndex);
    if (it == m_faceOctreeNodes.end())
        return;

    std::vector<OctreeNode *> nodes = it->second;
    m_faceOctreeNodes.erase(it);

    auto &newNodes = m_faceOctreeNodes[newIndex];
    newNodes.reserve(newNodes.size() + nodes.size());

    for (OctreeNode *node : nodes)
    {
        if (!node)
            continue;

        auto &faces = node->faceIndices;
        auto fit = std::find(faces.begin(), faces.end(), oldIndex);
        if (fit != faces.end())
        {
            *fit = newIndex;
        }
        else
        {
            faces.push_back(newIndex);
        }

        newNodes.push_back(node);
    }
}

void FaceOctree::removeLooseFace(size_t faceIndex) const
{
    m_looseFaces.erase(faceIndex);
}

void FaceOctree::insertLooseFace(size_t faceIndex) const
{
    m_looseFaces.insert(faceIndex);
}

bool FaceOctree::hasLooseFace(size_t faceIndex) const
{
    return m_looseFaces.count(faceIndex) > 0;
}

void FaceOctree::reset()
{
    m_faceOctree.reset();
    clearFaceOctreeCache();
    m_looseFaces.clear();
}

void FaceOctree::updateFaceOctree() const
{
    m_looseFaces.clear();
    const auto &faces = m_geometry->getHalfEdgeTable().getFaces();
    std::vector<std::pair<size_t, bbox>> faceBBoxes;
    faceBBoxes.reserve(faces.size());

    for (size_t fi = 0; fi < faces.size(); ++fi)
    {
        if (faces[fi].heh.index == -1)
            continue;

        std::vector<glm::vec3> poly =
            m_geometry->collectFacePolygon(FaceHandle{static_cast<int64_t>(fi)});
        if (poly.size() < 3)
            continue;

        bbox fb{poly[0], poly[0]};
        for (const auto &v : poly)
        {
            fb.min = glm::min(fb.min, v);
            fb.max = glm::max(fb.max, v);
        }
        faceBBoxes.push_back({fi, fb});
    }

    int totalFaces = (int)(faceBBoxes.size());
    if (totalFaces == 0)
    {
        m_faceOctree.reset();
        clearFaceOctreeCache();
        return;
    }

    m_octreeMinFaces = (int)(8.0 * std::pow(totalFaces, 0.25));
    m_octreeMaxDepth = std::clamp(
        (int)(std::ceil(std::log(totalFaces / 10.0f) / std::log(8.0f))), 4, 8);
    std::cout << "Building face octree for " << totalFaces
              << " faces (minFaces=" << m_octreeMinFaces
              << ", maxDepth=" << m_octreeMaxDepth << ")." << std::endl;
    clearFaceOctreeCache();
    m_faceOctree = buildFaceOctree(faceBBoxes, m_geometry->getBoundingBox(), 0,
                                   m_octreeMaxDepth, m_octreeMinFaces);
    indexFaceOctreeNodes(m_faceOctree.get());
}

void FaceOctree::clearFaceOctreeCache() const
{
    m_faceOctreeNodes.clear();
}

void FaceOctree::reinsertFaceIntoOctree(size_t faceIndex) const
{
    if (!m_faceOctree)
        return;

    const auto &faces = m_geometry->getHalfEdgeTable().getFaces();
    if (faceIndex >= faces.size() || faces[faceIndex].heh.index == -1)
        return;

    std::vector<glm::vec3> poly =
        m_geometry->collectFacePolygon(FaceHandle{static_cast<int64_t>(faceIndex)});
    if (poly.size() < 3)
        return;

    bbox fb{poly[0], poly[0]};
    for (const auto &v : poly)
    {
        fb.min = glm::min(fb.min, v);
        fb.max = glm::max(fb.max, v);
    }

    std::vector<OctreeNode *> leaves = insertFaceIntoOctree(
        m_faceOctree.get(), faceIndex, fb, m_octreeMaxDepth, m_octreeMinFaces);

    if (leaves.empty())
        return;

    auto &nodeVec = m_faceOctreeNodes[faceIndex];
    nodeVec.insert(nodeVec.end(), leaves.begin(), leaves.end());
}

void FaceOctree::indexFaceOctreeNodes(OctreeNode *node) const
{
    if (!node)
        return;

    if (!node->faceIndices.empty())
    {
        for (size_t faceIndex : node->faceIndices)
        {
            m_faceOctreeNodes[faceIndex].push_back(node);
        }
    }

    for (const auto &child : node->children)
    {
        indexFaceOctreeNodes(child.get());
    }
}

void FaceOctree::adjustFaceOctreeCounts(OctreeNode *node, int delta) const
{
    while (node)
    {
        node->faceCount =
            static_cast<size_t>(static_cast<int64_t>(node->faceCount) + delta);
        node = node->parent;
    }
}

void FaceOctree::pruneFaceOctreeBranch(OctreeNode *node) const
{
    while (node && node->parent && node->faceCount == 0)
    {
        OctreeNode *parent = node->parent;
        parent->children[node->childIndex].reset();
        node = parent;
    }
}

PrebuiltOctree FaceOctree::buildOctreeFromHET(const HalfEdgeTable &het, const bbox &bounds)
{
    PrebuiltOctree out;
    const auto &faces = het.getFaces();
    const auto &halfEdges = het.getHalfEdges();
    const auto &positions = het.getPositions();
    const int64_t heSize = static_cast<int64_t>(halfEdges.size());
    const int64_t posSize = static_cast<int64_t>(positions.size());

    std::vector<std::pair<size_t, bbox>> faceBBoxes;
    faceBBoxes.reserve(faces.size());

    for (size_t fi = 0; fi < faces.size(); ++fi)
    {
        if (faces[fi].heh.index == -1)
            continue;

        HalfEdgeHandle start = faces[fi].heh;
        HalfEdgeHandle curr = start;
        std::vector<glm::vec3> poly;
        poly.reserve(4);
        int guard = 0;
        do {
            if (curr.index < 0 || curr.index >= heSize) break;
            const HalfEdge &he = halfEdges[curr.index];
            if (he.dst.index >= 0 && he.dst.index < posSize)
                poly.push_back(positions[he.dst.index]);
            curr = he.next;
        } while (curr != start && curr.index != -1 && ++guard < 64);

        if (poly.size() < 3)
            continue;

        bbox fb{poly[0], poly[0]};
        for (const auto &v : poly)
        {
            fb.min = glm::min(fb.min, v);
            fb.max = glm::max(fb.max, v);
        }
        faceBBoxes.push_back({fi, fb});
    }

    int totalFaces = static_cast<int>(faceBBoxes.size());
    if (totalFaces == 0)
        return out;

    out.octreeMinFaces = static_cast<int>(8.0 * std::pow(totalFaces, 0.25));
    out.octreeMaxDepth = std::clamp(
        static_cast<int>(std::ceil(std::log(totalFaces / 10.0f) / std::log(8.0f))), 4, 8);

    out.root = buildFaceOctree(faceBBoxes, bounds, 0, out.octreeMaxDepth, out.octreeMinFaces);

    std::function<void(OctreeNode*)> indexNodes = [&](OctreeNode *node) {
        if (!node) return;
        if (!node->faceIndices.empty()) {
            for (size_t faceIndex : node->faceIndices) {
                out.faceOctreeNodes[faceIndex].push_back(node);
            }
        }
        for (const auto &child : node->children) {
            indexNodes(child.get());
        }
    };
    indexNodes(out.root.get());

    return out;
}

void FaceOctree::adoptPrebuiltOctree(PrebuiltOctree &&prebuilt)
{
    m_faceOctree = std::move(prebuilt.root);
    m_faceOctreeNodes = std::move(prebuilt.faceOctreeNodes);
    m_octreeMaxDepth = prebuilt.octreeMaxDepth;
    m_octreeMinFaces = prebuilt.octreeMinFaces;
    m_looseFaces.clear();
}
