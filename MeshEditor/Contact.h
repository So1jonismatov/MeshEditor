#pragma once
#include <HalfEdge.h>
#include <glm/glm.hpp>

class Node;

struct Contact
{
    FaceHandle face;
    Node *node = nullptr;
    float distance = 0.0f;
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
};