#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace acheron::memory {

/**
 * @brief A high-performance bump allocator for contiguous memory.
 * 
 * The LinearAllocator provides O(1) allocation with zero fragmentation
 * by moving a pointer forward through a pre-allocated memory block.
 * It does not support individual deallocations; memory is reclaimed
 * entirely via reset().
 */
class LinearAllocator {
public:
    LinearAllocator() = default;
    explicit LinearAllocator(std::span<std::byte> memory);

    // Non-copyable, movable
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator(LinearAllocator&&) noexcept = default;
    LinearAllocator& operator=(LinearAllocator&&) noexcept = default;

    /**
     * @brief Allocates a block of memory with the specified size and alignment.
     * 
     * @param size Size in bytes to allocate.
     * @param alignment Alignment requirement (must be a power of 2).
     * @return void* Pointer to the allocated memory, or nullptr if allocation fails.
     */
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));

    /**
     * @brief Resets the allocator, making all its memory available again.
     */
    void reset();

    /** @return Total capacity of the allocator in bytes. */
    [[nodiscard]] size_t capacity() const noexcept { return m_memory.size(); }
    
    /** @return Number of bytes currently allocated. */
    [[nodiscard]] size_t used() const noexcept { return m_offset; }
    
    /** @return Number of bytes remaining in the buffer. */
    [[nodiscard]] size_t remaining() const noexcept { return m_memory.size() - m_offset; }

private:
    [[nodiscard]] static bool is_power_of_two(size_t value) noexcept;

    std::span<std::byte> m_memory;
    size_t m_offset = 0;
};

} // namespace acheron::memory
