#pragma once
#include "IndexBuffer.h"
#include "VertexBuffer.h"
#include <glad/glad.h>

class VertexArray
{
private:
    unsigned int m_ID;

public:
    VertexArray();
    ~VertexArray();

    void bind() const;
    void unbind() const;
    void attachVertexBuffer(const VertexBuffer &vbo, unsigned int stride);
    void attachIndexBuffer(const IndexBuffer &ibo);

    void addAttribute(unsigned int layoutLocation, int numComponents,
                      unsigned int offset);
};