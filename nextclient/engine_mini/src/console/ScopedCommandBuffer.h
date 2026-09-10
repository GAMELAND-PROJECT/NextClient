#pragma once
#include <string>

// Reuse capacity without sharing a buffer with an in-flight nested hook.
class ScopedCommandBuffer
{
public:
    explicit ScopedCommandBuffer(std::string& spare) : spare_(spare)
    {
        value.swap(spare_);
        value.clear();
    }
    ~ScopedCommandBuffer()
    {
        if (value.capacity() > spare_.capacity())
            value.swap(spare_);
    }
    ScopedCommandBuffer(const ScopedCommandBuffer&) = delete;
    ScopedCommandBuffer& operator=(const ScopedCommandBuffer&) = delete;
    std::string value;
private:
    std::string& spare_;
};
