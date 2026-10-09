#pragma once

#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

#include "Octree.h"
#include <HalfEdge.h>

class Geometry;

struct PrebuiltOctree
{
    std::unique_ptr<OctreeNode> root;
    std::unordered_map<size_t, std::vector<OctreeNode *>> faceOctreeNodes;
    int octreeMaxDepth = 6;
    int octreeMinFaces = 8;
};

class FaceOctree
{
public:
    explicit FaceOctree(const Geometry* geometry);
    ~FaceOctree() = default;

    const OctreeNode* getFaceOctree() const;
    const std::unordered_set<size_t>& getLooseFaces() const;

    static PrebuiltOctree buildOctreeFromHET(const HalfEdgeTable &het, const bbox &bounds);
    void adoptPrebuiltOctree(PrebuiltOctree &&prebuilt);

    void markOctreeDirty();
    void flushOctreeLooseFaces();

    void invalidateFaceInOctree(size_t faceIndex) const;
    void removeFaceFromOctreeCache(size_t faceIndex) const;
    void replaceFaceInOctreeCache(size_t oldIndex, size_t newIndex) const;

    void removeLooseFace(size_t faceIndex) const;
    void insertLooseFace(size_t faceIndex) const;
    bool hasLooseFace(size_t faceIndex) const;
    void reset();

private:
    void updateFaceOctree() const;
    void clearFaceOctreeCache() const;
    void reinsertFaceIntoOctree(size_t faceIndex) const;
    void indexFaceOctreeNodes(OctreeNode *node) const;
    void adjustFaceOctreeCounts(OctreeNode *node, int delta) const;
    void pruneFaceOctreeBranch(OctreeNode *node) const;

    const Geometry* m_geometry;

    mutable std::unique_ptr<OctreeNode> m_faceOctree;
    mutable std::unordered_map<size_t, std::vector<OctreeNode *>> m_faceOctreeNodes;
    mutable std::unordered_set<size_t> m_looseFaces;
    mutable int m_octreeMaxDepth = 6;
    mutable int m_octreeMinFaces = 8;
};
