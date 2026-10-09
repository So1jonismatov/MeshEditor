#include "Model.h"
#include <stdexcept>
#include <limits>
#include <functional>



void Model::attachNode(std::unique_ptr<Node> node)
{
    if (node->getParent() != nullptr)
    {
        throw std::logic_error("Node already has a parent");
    }
    nodes.push_back(std::move(node));
    m_bboxDirty = true;
}

const std::vector<std::unique_ptr<Node>> &Model::getNodes() const
{
    return nodes;
}

const bbox &Model::getWorldBoundingBox() const
{
    if (!m_bboxDirty)
        return m_cachedBBox;

    glm::vec3 globalMin(std::numeric_limits<float>::max());
    glm::vec3 globalMax(std::numeric_limits<float>::lowest());
    bool hasMesh = false;

    // Single-pass top-down traversal (O(N) with zero parent walks)
    std::function<void(const Node *, const glm::mat4 &)> traverse =
        [&](const Node *node, const glm::mat4 &parentTrf)
    {
        if (!node || node->isManipulator())
            return;

        const glm::mat4 absTrf = parentTrf * node->getRelativeTransform();
        if (Mesh *mesh = node->getMesh())
        {
            const bbox worldBox = mesh->getBoundingBox().transformed(absTrf);
            globalMin = glm::min(globalMin, worldBox.min);
            globalMax = glm::max(globalMax, worldBox.max);
            hasMesh = true;
        }

        for (const auto &child : node->getChildren())
        {
            traverse(child.get(), absTrf);
        }
    };

    const glm::mat4 identity(1.0f);
    for (const auto &node : nodes)
    {
        traverse(node.get(), identity);
    }

    if (hasMesh)
    {
        m_cachedBBox.min = globalMin;
        m_cachedBBox.max = globalMax;
    }
    else
    {
        m_cachedBBox.min = m_cachedBBox.max = glm::vec3(0.0f);
    }

    m_bboxDirty = false;
    return m_cachedBBox;
}