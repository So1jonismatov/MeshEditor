#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "Mesh.h"

class Node
{
public:
    Node();
    virtual ~Node();

    void setName(const std::string &inName);
    const std::string &getName() const;

    void attachMesh(std::unique_ptr<Mesh> inMesh);
    Mesh *getMesh() const;

    void setParent(Node *inParent);
    Node *getParent() const;

    void setRelativeTransform(const glm::mat4 &trf);
    const glm::mat4 &getRelativeTransform() const;

    void applyRelativeTransform(const glm::mat4 &trf);

    const std::vector<std::unique_ptr<Node>> &getChildren() const;

    void traverse(const std::function<void(const Node *)> &fn) const;

    glm::mat4 calcAbsoluteTransform() const;

    void attachNode(std::unique_ptr<Node> newNode);
    void deleteFromParent();
    std::unique_ptr<Node> detachChild(Node *child);

    bool isManipulator() const;
    void setManipulator(bool val);

private:
    std::string m_name;
    std::unique_ptr<Mesh> m_mesh;
    Node *m_parent;
    glm::mat4 m_relativeTransform;
    std::vector<std::unique_ptr<Node>> m_children;
    bool m_isManipulator = false;
};