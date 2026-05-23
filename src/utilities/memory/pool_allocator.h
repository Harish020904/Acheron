#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace acheron::memory {

/**
 * @brief Allocator for fixed-size memory blocks.
 * 
 * The PoolAllocator divides a contiguous memory region into equal-sized slots.
 * It provides O(1) allocation and deallocation by maintaining an internal 
 * free-list. Memory addresses of blocks remain stable until the pool is destroyed.
 */
class PoolAllocator {
public:
    PoolAllocator() = default;

    /**
     * @brief Initializes the pool allocator.
     * @param memory Pre-allocated memory region to manage.
     * @param block_size Size of each block in bytes.
     * @param alignment Alignment for each block (must be a power of 2).
     */
    PoolAllocator(std::span<std::byte> memory, size_t block_size, size_t alignment = alignof(std::max_align_t));

    // Non-copyable, movable
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&&) noexcept = default;
    PoolAllocator& operator=(PoolAllocator&&) noexcept = default;

    /**
     * @brief Allocates one fixed-size block from the pool.
     * @return void* Pointer to the block, or nullptr if the pool is exhausted.
     */
    void* allocate();

    /**
     * @brief Returns a block to the pool's free-list.
     * @param ptr Pointer to the block to deallocate. Must have been allocated by this pool.
     */
    void deallocate(void* ptr);

    /** @return Size of each block in bytes. */
    [[nodiscard]] size_t block_size() const noexcept { return m_block_size; }
    
    /** @return Total number of blocks the pool can hold. */
    [[nodiscard]] size_t capacity() const noexcept { return m_capacity; }
    
    /** @return Number of blocks currently in use. */
    [[nodiscard]] size_t used_count() const noexcept { return m_used_count; }

private:
    struct Node {
        Node* next;
    };

    [[nodiscard]] bool owns_pointer(const void* ptr) const noexcept;

    std::span<std::byte> m_memory;
    Node* m_free_list = nullptr;
    size_t m_block_size = 0;
    size_t m_stride = 0;
    size_t m_alignment = alignof(std::max_align_t);
    std::byte* m_aligned_start = nullptr;
    size_t m_capacity = 0;
    size_t m_used_count = 0;
};

} // namespace acheron::memory
