#pragma once

#include <type_traits>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

#include "Node.h"
#include "Geometry/AABB.h"

struct AsyncContext {
    std::mutex mutex;
    std::atomic<bool> alive{true};
};

class Model
{
public:
    Model() = default;
    ~Model() { 
        std::lock_guard<std::mutex> lock(m_asyncCtx->mutex);
        m_asyncCtx->alive.store(false, std::memory_order_release); 
    }

    void attachNode(std::unique_ptr<Node> node);
    const std::vector<std::unique_ptr<Node>> &getNodes() const;
    std::vector<std::unique_ptr<Node>> &getNodes() { return nodes; }
    std::vector<std::unique_ptr<Node>> extractNodes() {
        markWorldBoundingBoxDirty();
        return std::move(nodes);
    }

    // Fast cached world bounding box calculation
    const bbox &getWorldBoundingBox() const;
    void markWorldBoundingBoxDirty() const { m_bboxDirty = true; }

    // Thread-safe mutex & busy status for background async tasks
    std::mutex &getModelMutex() { return m_asyncCtx->mutex; }
    bool isBusy() const { return m_busy.load(std::memory_order_acquire); }
    void setBusy(bool b) { m_busy.store(b, std::memory_order_release); }
    
    std::shared_ptr<AsyncContext> getAsyncContext() const { return m_asyncCtx; }

    template <class Fn> void forEachMeshRecursive(Fn &&fn) const
    {
        for (const auto &node : nodes)
        {
            forEachMeshRecursive(node.get(), fn);
        }
    }

private:
    template <class Fn>
    void forEachMeshRecursive(const Node *node, Fn &&fn) const
    {
        if (!node)
            return;

        if (Mesh *mesh = node->getMesh())
        {
            if constexpr (std::is_invocable_v<Fn, const Node *, Mesh *>)
            {
                fn(node, mesh);
            }
            else
            {
                fn(mesh);
            }
        }

        for (const auto &child : node->getChildren())
        {
            forEachMeshRecursive(child.get(), fn);
        }
    }

    std::vector<std::unique_ptr<Node>> nodes;
    std::shared_ptr<AsyncContext> m_asyncCtx = std::make_shared<AsyncContext>();
    std::atomic<bool> m_busy{false};
    mutable bbox m_cachedBBox;
    mutable bool m_bboxDirty = true;
};