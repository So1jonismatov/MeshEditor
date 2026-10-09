#include "VertexArray.h"

VertexArray::VertexArray()
{
    glCreateVertexArrays(1, &m_ID);
}

VertexArray::~VertexArray()
{
    glDeleteVertexArrays(1, &m_ID);
}

void VertexArray::bind() const
{
    glBindVertexArray(m_ID);
}
void VertexArray::unbind() const
{
    glBindVertexArray(0);
}

void VertexArray::attachVertexBuffer(const VertexBuffer &vbo,
                                     unsigned int stride)
{
    glVertexArrayVertexBuffer(m_ID, 0, vbo.GetID(), 0, stride);
}

void VertexArray::attachIndexBuffer(const IndexBuffer &ibo)
{
    glVertexArrayElementBuffer(m_ID, ibo.GetID());
}

void VertexArray::addAttribute(unsigned int layoutLocation, int numComponents,
                               unsigned int offset)
{
    glEnableVertexArrayAttrib(m_ID, layoutLocation);
    glVertexArrayAttribFormat(m_ID, layoutLocation, numComponents, GL_FLOAT,
                              GL_FALSE, offset);
    glVertexArrayAttribBinding(m_ID, layoutLocation,
                               0); // Bind to vertex buffer at index 0
}