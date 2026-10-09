#include "Buffer.h"

Buffer::Buffer(size_t initialSize) : m_Size(initialSize)
{
    glCreateBuffers(1, &m_ID);
    glNamedBufferData(m_ID, m_Size, nullptr, GL_DYNAMIC_DRAW);
}

Buffer::~Buffer()
{
    glDeleteBuffers(1, &m_ID);
}

unsigned int Buffer::GetID() const
{
    return m_ID;
}

size_t Buffer::GetSize() const
{
    return m_Size;
}

void Buffer::SetData(const void *data, size_t size)
{
    if (size > m_Size)
    {
        m_Size = size * 2;
        glNamedBufferData(m_ID, m_Size, nullptr, GL_DYNAMIC_DRAW);
    }

    glNamedBufferSubData(m_ID, 0, size, data);
}

void Buffer::SetSubData(const void *data, size_t offset, size_t size)
{
    if (offset + size > m_Size)
    {
        m_Size = (offset + size) * 2;
        glNamedBufferData(m_ID, m_Size, nullptr, GL_DYNAMIC_DRAW);
    }

    glNamedBufferSubData(m_ID, offset, size, data);
}