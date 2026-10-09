#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <array>
#include <utility>
#include <algorithm>
#include <cmath>
#include <limits>

#include "AABB.h"

struct OctreeNode
{
    bbox box;
    OctreeNode *parent = nullptr;
    int childIndex = -1;
    size_t faceCount = 0;
    std::array<std::unique_ptr<OctreeNode>, 8> children{};
    std::vector<size_t> faceIndices;

    bool isLeaf() const;
};

bool rayAABBIntersection(const glm::vec3 &orig, const glm::vec3 &dir,
                         const glm::vec3 &bMin, const glm::vec3 &bMax,
                         float &tOut);

std::unique_ptr<OctreeNode> buildFaceOctree(
    const std::vector<std::pair<size_t, bbox>> &faceBBoxes, const bbox &bounds,
    int depth, int maxDepth, int minFaces, OctreeNode *parent = nullptr,
    int childIndex = -1);

void queryFaceOctree(const OctreeNode *node, const glm::vec3 &orig,
                     const glm::vec3 &dir, std::vector<size_t> &candidates);

// Incrementally insert a single face into an existing octree.
// Returns the set of leaf nodes where the face was registered so the caller
// can update its faceIndex→nodes cache.  Returns empty if the face bbox
// does not overlap the tree bounds at all.
std::vector<OctreeNode *> insertFaceIntoOctree(OctreeNode *root,
                                                size_t faceIndex,
                                                const bbox &faceBBox,
                                                int maxDepth, int minFaces);
