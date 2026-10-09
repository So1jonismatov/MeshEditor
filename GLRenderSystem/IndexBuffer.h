#pragma once

#include "Buffer.h"

class IndexBuffer : public Buffer
{
public:
    using Buffer::Buffer;

    void SetSubData(const void *data, size_t offset, size_t size) override;
};