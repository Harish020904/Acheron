#pragma once

#include "linear_allocator.h"
#include <cstddef>

namespace acheron::memory {

/**
 * @brief Thread-local frame allocator for transient frame data.
 * 
 * The FrameAllocator provides O(1) transient allocations that are 
 * automatically reclaimed at the end of each simulation or render frame.
 * 
 * Usage:
 *   void* ptr = FrameAllocator::allocate(1024);
 *   ... at end of frame ...
 *   FrameAllocator::reset();
 */
class FrameAllocator {
public:
    /**
     * @brief Initializes the frame allocator for the calling thread.
     * @param capacity Total size of the frame buffer for this thread.
     */
    static void initialize(size_t capacity);

    /**
     * @brief Allocates transient memory from the thread-local frame buffer.
     */
    static void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));

    /**
     * @brief Resets the thread-local frame buffer.
     */
    static void reset();

    /**
     * @brief Cleans up the frame allocator for the calling thread.
     */
    static void shutdown();

    /** @return True if the frame allocator is initialized for the current thread. */
    static bool is_initialized();

private:
    struct ThreadLocalBuffer {
        std::byte* raw_allocation = nullptr;
        std::span<std::byte> storage;
        LinearAllocator allocator;
        size_t capacity = 0;
        bool active = false;
    };

    static thread_local ThreadLocalBuffer s_buffer;
};

} // namespace acheron::memory
