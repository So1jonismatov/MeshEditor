#include "Node.h"
#include <stdexcept>
#include <algorithm>

Node::Node()
    : m_parent(nullptr), m_relativeTransform(1.0f), m_isManipulator(false)
{
}

Node::~Node() = default;

void Node::setName(const std::string &inName)
{
    m_name = inName;
}

const std::string &Node::getName() const
{
    return m_name;
}

void Node::attachMesh(std::unique_ptr<Mesh> inMesh)
{
    m_mesh = std::move(inMesh);
}

Mesh *Node::getMesh() const
{
    return m_mesh.get();
}

void Node::setParent(Node *inParent)
{
    m_parent = inParent;
}

Node *Node::getParent() const
{
    return m_parent;
}

void Node::setRelativeTransform(const glm::mat4 &trf)
{
    m_relativeTransform = trf;
}

const glm::mat4 &Node::getRelativeTransform() const
{
    return m_relativeTransform;
}

void Node::applyRelativeTransform(const glm::mat4 &trf)
{
    m_relativeTransform = m_relativeTransform * trf;
}

const std::vector<std::unique_ptr<Node>> &Node::getChildren() const
{
    return m_children;
}

void Node::traverse(const std::function<void(const Node *)> &fn) const
{
    fn(this);
    for (const auto &child : m_children)
    {
        child->traverse(fn);
    }
}

glm::mat4 Node::calcAbsoluteTransform() const
{
    if (m_parent)
    {
        return m_parent->calcAbsoluteTransform() * m_relativeTransform;
    }
    return m_relativeTransform;
}

void Node::attachNode(std::unique_ptr<Node> newNode)
{
    if (newNode->getParent() != nullptr)
    {
        throw std::logic_error("New node already has a parent");
    }
    newNode->setParent(this);
    m_children.push_back(std::move(newNode));
}

void Node::deleteFromParent()
{
    if (m_parent)
    {
        // Extract this node safely from the parent to avoid destroying ourselves
        // while the parent's children vector is still being mutated.
        m_parent->detachChild(this);
    }
}

std::unique_ptr<Node> Node::detachChild(Node *child)
{
    std::unique_ptr<Node> extracted;
    if (!child)
        return extracted;
    for (auto it = m_children.begin(); it != m_children.end(); ++it)
    {
        if (it->get() == child)
        {
            extracted = std::move(*it);
            m_children.erase(it);
            extracted->setParent(nullptr);
            return extracted;
        }
    }
    return extracted;
}

bool Node::isManipulator() const
{
    return m_isManipulator;
}

void Node::setManipulator(bool val)
{
    m_isManipulator = val;
}