#include "linear_allocator.h"
#include <cassert>
#include <cstdint>
#include <limits>

namespace acheron::memory {

LinearAllocator::LinearAllocator(std::span<std::byte> memory)
    : m_memory(memory)
    , m_offset(0) {}

bool LinearAllocator::is_power_of_two(size_t value) noexcept {
    return value != 0 && (value & (value - 1U)) == 0U;
}

void* LinearAllocator::allocate(size_t size, size_t alignment) {
    if (m_memory.empty() || size == 0U) {
        return nullptr;
    }

    if (!is_power_of_two(alignment)) {
        assert(false && "LinearAllocator alignment must be a non-zero power of two");
        return nullptr;
    }

    if (m_offset >= m_memory.size()) {
        return nullptr;
    }

    const uintptr_t base_address = reinterpret_cast<uintptr_t>(m_memory.data());
    const uintptr_t current_address = base_address + static_cast<uintptr_t>(m_offset);

    const size_t alignment_mask = alignment - 1U;
    const uintptr_t aligned_address = (current_address + static_cast<uintptr_t>(alignment_mask)) &
                                      ~static_cast<uintptr_t>(alignment_mask);

    if (aligned_address < current_address) {
        return nullptr;
    }

    const size_t padding = static_cast<size_t>(aligned_address - current_address);
    if (padding > std::numeric_limits<size_t>::max() - size) {
        return nullptr;
    }

    const size_t total_consumed = padding + size;
    if (total_consumed > (m_memory.size() - m_offset)) {
        return nullptr;
    }

    m_offset += total_consumed;
    return reinterpret_cast<void*>(aligned_address);
}

void LinearAllocator::reset() {
    m_offset = 0;
}

} // namespace acheron::memory
