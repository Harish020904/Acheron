#include "frame_allocator.h"
#include <cstdlib>
#include <cassert>

namespace acheron::memory {

thread_local FrameAllocator::ThreadLocalBuffer FrameAllocator::s_buffer;

void FrameAllocator::initialize(size_t capacity) {
    assert(!s_buffer.active && "FrameAllocator already initialized for this thread");
    if (s_buffer.active) {
        return;
    }

    assert(capacity > 0U && "FrameAllocator capacity must be greater than zero");

    if (capacity == 0U) {
        return;
    }

    void* raw_memory = std::malloc(capacity);
    if (raw_memory == nullptr) {
        assert(false && "Failed to allocate memory for FrameAllocator");
        return;
    }

    s_buffer.raw_allocation = static_cast<std::byte*>(raw_memory);
    s_buffer.storage = std::span<std::byte>(s_buffer.raw_allocation, capacity);
    s_buffer.allocator = LinearAllocator(s_buffer.storage);
    s_buffer.capacity = capacity;
    s_buffer.active = true;
}

void* FrameAllocator::allocate(size_t size, size_t alignment) {
    assert(s_buffer.active && "FrameAllocator not initialized for this thread");
    if (!s_buffer.active) {
        return nullptr;
    }

    return s_buffer.allocator.allocate(size, alignment);
}

void FrameAllocator::reset() {
    assert(s_buffer.active && "FrameAllocator not initialized for this thread");
    if (!s_buffer.active) {
        return;
    }

    s_buffer.allocator.reset();
}

void FrameAllocator::shutdown() {
    if (s_buffer.active) {
        std::free(s_buffer.raw_allocation);
        s_buffer.raw_allocation = nullptr;
        s_buffer.storage = {};
        s_buffer.allocator = LinearAllocator();
        s_buffer.capacity = 0;
        s_buffer.active = false;
    }
}

bool FrameAllocator::is_initialized() {
    return s_buffer.active;
}

} // namespace acheron::memory
