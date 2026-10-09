#pragma once

#include <cstddef>
#include <glad/glad.h>

class Buffer
{
public:
    Buffer(size_t initialSize);
    virtual ~Buffer();

    unsigned int GetID() const;
    size_t GetSize() const;
    virtual void SetData(const void *data, size_t size);
    virtual void SetSubData(const void *data, size_t offset, size_t size);

protected:
    unsigned int m_ID;
    size_t m_Size;
};