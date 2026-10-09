#include "Octree.h"

bool OctreeNode::isLeaf() const
{
    for (int i = 0; i < 8; ++i)
        if (children[i])
            return false;
    return true;
}

bool rayAABBIntersection(const glm::vec3 &orig, const glm::vec3 &dir,
                         const glm::vec3 &bMin, const glm::vec3 &bMax,
                         float &tOut)
{
    return AABB{bMin, bMax}.intersectRay(orig, dir, tOut);
}

std::unique_ptr<OctreeNode> buildFaceOctree(
    const std::vector<std::pair<size_t, bbox>> &faceBBoxes, const bbox &bounds,
    int depth, int maxDepth, int minFaces, OctreeNode *parent, int childIndex)
{
    auto node = std::make_unique<OctreeNode>();
    node->box = bounds;
    node->parent = parent;
    node->childIndex = childIndex;

    if (depth >= maxDepth || static_cast<int>(faceBBoxes.size()) <= minFaces)
    {
        node->faceIndices.reserve(faceBBoxes.size());
        for (const auto &[fi, fb] : faceBBoxes)
            node->faceIndices.push_back(fi);
        node->faceCount = node->faceIndices.size();
        return node;
    }

    const glm::vec3 size = bounds.max - bounds.min;
    if (size.x < 1e-5f && size.y < 1e-5f && size.z < 1e-5f)
    {
        node->faceIndices.reserve(faceBBoxes.size());
        for (const auto &[fi, fb] : faceBBoxes)
            node->faceIndices.push_back(fi);
        node->faceCount = node->faceIndices.size();
        return node;
    }

    glm::vec3 mid = {(bounds.min.x + bounds.max.x) * 0.5f,
                     (bounds.min.y + bounds.max.y) * 0.5f,
                     (bounds.min.z + bounds.max.z) * 0.5f};

    bool createdAnyChild = false;

    for (int i = 0; i < 8; ++i)
    {
        bbox cb;
        cb.min.x = (i & 1) ? mid.x : bounds.min.x;
        cb.min.y = (i & 2) ? mid.y : bounds.min.y;
        cb.min.z = (i & 4) ? mid.z : bounds.min.z;
        cb.max.x = (i & 1) ? bounds.max.x : mid.x;
        cb.max.y = (i & 2) ? bounds.max.y : mid.y;
        cb.max.z = (i & 4) ? bounds.max.z : mid.z;

        std::vector<std::pair<size_t, bbox>> childFaces;
        for (const auto &[fi, fb] : faceBBoxes)
        {
            if (fb.max.x >= cb.min.x && fb.min.x <= cb.max.x &&
                fb.max.y >= cb.min.y && fb.min.y <= cb.max.y &&
                fb.max.z >= cb.min.z && fb.min.z <= cb.max.z)
                childFaces.push_back({fi, fb});
        }

        // Only split if the child actually reduces the face count. If childFaces contains
        // all faceBBoxes, splitting doesn't isolate any geometry — recursing would duplicate
        // all faces across 8 children down to maxDepth, causing gigabytes of RAM allocation!
        if (!childFaces.empty() && childFaces.size() < faceBBoxes.size())
        {
            node->children[i] = buildFaceOctree(
                childFaces, cb, depth + 1, maxDepth, minFaces, node.get(), i);
            node->faceCount += node->children[i]->faceCount;
            createdAnyChild = true;
        }
    }

    // If no child was created because all face bounding boxes spanned across child midpoints,
    // convert this node into a leaf containing all faces.
    if (!createdAnyChild)
    {
        node->faceIndices.clear();
        node->faceIndices.reserve(faceBBoxes.size());
        for (const auto &[fi, fb] : faceBBoxes)
            node->faceIndices.push_back(fi);
        node->faceCount = node->faceIndices.size();
    }

    return node;
}

void queryFaceOctree(const OctreeNode *node, const glm::vec3 &orig,
                     const glm::vec3 &dir, std::vector<size_t> &candidates)
{
    if (!node)
        return;

    float t;
    if (!rayAABBIntersection(orig, dir, node->box.min, node->box.max, t))
        return;

    if (node->isLeaf())
    {
        if (!node->faceIndices.empty())
        {
            candidates.insert(candidates.end(), node->faceIndices.data(),
                              node->faceIndices.data() + node->faceIndices.size());
        }
        return;
    }

    for (const auto &child : node->children)
    {
        if (child)
            queryFaceOctree(child.get(), orig, dir, candidates);
    }
}

// ---------------------------------------------------------------------------
// Incremental single-face insertion
// ---------------------------------------------------------------------------
static bool bboxOverlap(const bbox &a, const bbox &b)
{
    return a.max.x >= b.min.x && a.min.x <= b.max.x &&
           a.max.y >= b.min.y && a.min.y <= b.max.y &&
           a.max.z >= b.min.z && a.min.z <= b.max.z;
}

std::vector<OctreeNode *> insertFaceIntoOctree(OctreeNode *root,
                                                size_t faceIndex,
                                                const bbox &faceBBox,
                                                int maxDepth, int minFaces)
{
    std::vector<OctreeNode *> registeredLeaves;
    if (!root)
        return registeredLeaves;
    if (!bboxOverlap(faceBBox, root->box))
        return registeredLeaves;

    struct Frame { OctreeNode *node; int depth; };
    std::vector<Frame> stack;
    stack.push_back({root, 0});

    while (!stack.empty())
    {
        auto [node, depth] = stack.back();
        stack.pop_back();

        if (!bboxOverlap(faceBBox, node->box))
            continue;

        ++node->faceCount;

        if (node->isLeaf())
        {
            node->faceIndices.push_back(faceIndex);
            registeredLeaves.push_back(node);
        }
        else
        {
            for (auto &child : node->children)
            {
                if (child && bboxOverlap(faceBBox, child->box))
                    stack.push_back({child.get(), depth + 1});
            }
        }
    }

    return registeredLeaves;
}
