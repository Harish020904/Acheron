#include "pool_allocator.h"
#include <algorithm>
#include <cassert>
#include <cstdint>

namespace acheron::memory {

namespace {

[[nodiscard]] bool is_power_of_two(size_t value) noexcept {
    return value != 0U && (value & (value - 1U)) == 0U;
}

[[nodiscard]] uintptr_t align_up(uintptr_t value, size_t alignment) noexcept {
    const uintptr_t mask = static_cast<uintptr_t>(alignment - 1U);
    return (value + mask) & ~mask;
}

} // namespace

PoolAllocator::PoolAllocator(std::span<std::byte> memory, size_t block_size, size_t alignment)
    : m_memory(memory)
    , m_block_size(0)
    , m_stride(0)
    , m_alignment(alignment)
    , m_aligned_start(nullptr)
    , m_capacity(0)
    , m_used_count(0) {

    if (m_memory.empty() || block_size == 0U) {
        return;
    }

    if (!is_power_of_two(alignment)) {
        assert(false && "PoolAllocator alignment must be a non-zero power of two");
        return;
    }

    m_block_size = std::max(block_size, sizeof(Node));
    m_stride = static_cast<size_t>(align_up(static_cast<uintptr_t>(m_block_size), alignment));
    if (m_stride < m_block_size) {
        assert(false && "PoolAllocator stride overflow");
        m_stride = 0;
        return;
    }

    const uintptr_t base_address = reinterpret_cast<uintptr_t>(m_memory.data());
    const uintptr_t aligned_address = align_up(base_address, alignment);
    if (aligned_address < base_address) {
        assert(false && "PoolAllocator alignment overflow");
        return;
    }

    const size_t leading_padding = static_cast<size_t>(aligned_address - base_address);
    if (leading_padding >= m_memory.size()) {
        return;
    }

    const size_t usable_bytes = m_memory.size() - leading_padding;
    m_capacity = usable_bytes / m_stride;
    if (m_capacity == 0U) {
        return;
    }

    m_aligned_start = reinterpret_cast<std::byte*>(aligned_address);
    m_free_list = nullptr;

    for (size_t i = m_capacity; i > 0U; --i) {
        const size_t block_index = i - 1U;
        Node* const node = reinterpret_cast<Node*>(m_aligned_start + (block_index * m_stride));
        node->next = m_free_list;
        m_free_list = node;
    }
}

bool PoolAllocator::owns_pointer(const void* ptr) const noexcept {
    if (ptr == nullptr || m_aligned_start == nullptr || m_capacity == 0U || m_stride == 0U) {
        return false;
    }

    const auto* const byte_ptr = static_cast<const std::byte*>(ptr);
    const auto* const start = m_aligned_start;
    const auto* const end = m_aligned_start + (m_capacity * m_stride);

    if (byte_ptr < start || byte_ptr >= end) {
        return false;
    }

    const size_t offset = static_cast<size_t>(byte_ptr - start);
    if ((offset % m_stride) != 0U) {
        return false;
    }

    return (reinterpret_cast<uintptr_t>(byte_ptr) & static_cast<uintptr_t>(m_alignment - 1U)) == 0U;
}

void* PoolAllocator::allocate() {
    if (!m_free_list) {
        return nullptr;
    }

    Node* const node = m_free_list;
    m_free_list = node->next;

    if (m_used_count < m_capacity) {
        ++m_used_count;
    }
    
    return static_cast<void*>(node);
}

void PoolAllocator::deallocate(void* ptr) {
    if (ptr == nullptr) {
        return;
    }

    assert(owns_pointer(ptr) && "Pointer does not belong to this pool");
    if (!owns_pointer(ptr)) {
        return;
    }

    assert(m_used_count > 0U && "PoolAllocator deallocate underflow");
    if (m_used_count == 0U) {
        return;
    }

    Node* const node = static_cast<Node*>(ptr);
    node->next = m_free_list;
    m_free_list = node;
    --m_used_count;
}

} // namespace acheron::memory
