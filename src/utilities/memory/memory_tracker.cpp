#include "memory_tracker.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>

namespace acheron::memory {

namespace {

constexpr size_t INVALID_SLOT = std::numeric_limits<size_t>::max();

alignas(64) std::atomic_flag g_tracker_lock = ATOMIC_FLAG_INIT;

class SpinLockGuard {
public:
    SpinLockGuard() noexcept {
        while (g_tracker_lock.test_and_set(std::memory_order_acquire)) {
        }
    }

    ~SpinLockGuard() noexcept {
        g_tracker_lock.clear(std::memory_order_release);
    }

    SpinLockGuard(const SpinLockGuard&) = delete;
    SpinLockGuard& operator=(const SpinLockGuard&) = delete;
};

} // namespace

AllocationRecord MemoryTracker::s_records[MAX_TRACKED_ALLOCATIONS];
AllocationRecord MemoryTracker::s_active_records[MAX_TRACKED_ALLOCATIONS];
size_t MemoryTracker::s_active_allocation_count = 0;

size_t MemoryTracker::s_budgets[static_cast<size_t>(AllocationCategory::Count)] = {};
size_t MemoryTracker::s_category_usage[static_cast<size_t>(AllocationCategory::Count)] = {};
bool MemoryTracker::s_budget_exceeded[static_cast<size_t>(AllocationCategory::Count)] = {};

size_t MemoryTracker::s_total_allocated = 0;
size_t MemoryTracker::s_total_freed = 0;
size_t MemoryTracker::s_current_usage = 0;
size_t MemoryTracker::s_peak_usage = 0;
bool MemoryTracker::s_initialized = false;

bool MemoryTracker::is_valid_category(AllocationCategory category) noexcept {
    const auto index = static_cast<size_t>(category);
    return index < static_cast<size_t>(AllocationCategory::Count);
}

size_t MemoryTracker::hash_ptr(const void* ptr) noexcept {
    auto value = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
    value ^= value >> 33U;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33U;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33U;

    static_assert((MAX_TRACKED_ALLOCATIONS & (MAX_TRACKED_ALLOCATIONS - 1U)) == 0U,
        "MAX_TRACKED_ALLOCATIONS must be a power of two");
    return static_cast<size_t>(value) & (MAX_TRACKED_ALLOCATIONS - 1U);
}

size_t MemoryTracker::find_slot_for_insert(const void* ptr) noexcept {
    const size_t start_index = hash_ptr(ptr);
    size_t first_tombstone = INVALID_SLOT;

    for (size_t probe = 0; probe < MAX_TRACKED_ALLOCATIONS; ++probe) {
        const size_t index = (start_index + probe) & (MAX_TRACKED_ALLOCATIONS - 1U);
        const AllocationRecord& slot = s_records[index];

        if (slot.ptr == nullptr) {
            return (first_tombstone != INVALID_SLOT) ? first_tombstone : index;
        }

        if (slot.ptr == ptr) {
            if (slot.active) {
                return INVALID_SLOT;
            }
            return index;
        }

        if (!slot.active && first_tombstone == INVALID_SLOT) {
            first_tombstone = index;
        }
    }

    return first_tombstone;
}

size_t MemoryTracker::find_active_slot(const void* ptr) noexcept {
    const size_t start_index = hash_ptr(ptr);

    for (size_t probe = 0; probe < MAX_TRACKED_ALLOCATIONS; ++probe) {
        const size_t index = (start_index + probe) & (MAX_TRACKED_ALLOCATIONS - 1U);
        const AllocationRecord& slot = s_records[index];

        if (slot.ptr == nullptr && !slot.active) {
            return INVALID_SLOT;
        }

        if (slot.ptr == ptr && slot.active) {
            return index;
        }
    }

    return INVALID_SLOT;
}

void MemoryTracker::initialize() {
    SpinLockGuard guard;
    assert(!s_initialized && "MemoryTracker already initialized");

    std::fill(std::begin(s_records), std::end(s_records), AllocationRecord{});
    std::fill(std::begin(s_active_records), std::end(s_active_records), AllocationRecord{});
    std::fill(std::begin(s_budgets), std::end(s_budgets), 0U);
    std::fill(std::begin(s_category_usage), std::end(s_category_usage), 0U);
    std::fill(std::begin(s_budget_exceeded), std::end(s_budget_exceeded), false);

    s_active_allocation_count = 0;
    s_total_allocated = 0;
    s_total_freed = 0;
    s_current_usage = 0;
    s_peak_usage = 0;
    s_initialized = true;
}

void MemoryTracker::shutdown() {
    SpinLockGuard guard;
    assert(s_initialized && "MemoryTracker not initialized");
    assert(s_active_allocation_count == 0U && "Memory leaks detected at shutdown");
    s_initialized = false;
}

void MemoryTracker::track_allocation(void* ptr, size_t size, AllocationCategory category, uint32_t owner_id, const char* file, int line) {
    SpinLockGuard guard;
    assert(s_initialized && "MemoryTracker not initialized");
    assert(ptr != nullptr && "Cannot track null allocation");
    assert(size > 0U && "Cannot track zero-byte allocation");
    assert(is_valid_category(category) && "Invalid allocation category");
    assert(owner_id != 0U && "Owner id must be non-zero");

    if (!s_initialized || ptr == nullptr || size == 0U || !is_valid_category(category) || owner_id == 0U) {
        return;
    }

    const size_t insert_index = find_slot_for_insert(ptr);
    assert(insert_index != INVALID_SLOT && "MemoryTracker full or duplicate allocation tracking");
    if (insert_index == INVALID_SLOT) {
        return;
    }

    AllocationRecord& record = s_records[insert_index];
    record.ptr = ptr;
    record.size = size;
    record.category = category;
    record.owner_id = owner_id;
    record.file = file;
    record.line = line;
    record.active = true;

    const size_t category_index = static_cast<size_t>(category);
    s_category_usage[category_index] += size;

    s_total_allocated += size;
    s_current_usage += size;
    ++s_active_allocation_count;

    if (s_current_usage > s_peak_usage) {
        s_peak_usage = s_current_usage;
    }

    const size_t budget_limit = s_budgets[category_index];
    s_budget_exceeded[category_index] = (budget_limit > 0U && s_category_usage[category_index] > budget_limit);
}

void MemoryTracker::track_deallocation(void* ptr) {
    SpinLockGuard guard;
    assert(s_initialized && "MemoryTracker not initialized");
    assert(ptr != nullptr && "Cannot track null deallocation");

    if (!s_initialized || ptr == nullptr) {
        return;
    }

    const size_t index = find_active_slot(ptr);
    assert(index != INVALID_SLOT && "Deallocation of untracked memory");
    if (index == INVALID_SLOT) {
        return;
    }

    AllocationRecord& record = s_records[index];
    record.active = false;

    const size_t category_index = static_cast<size_t>(record.category);
    assert(s_category_usage[category_index] >= record.size && "Category usage underflow");
    if (s_category_usage[category_index] >= record.size) {
        s_category_usage[category_index] -= record.size;
    } else {
        s_category_usage[category_index] = 0;
    }

    s_total_freed += record.size;

    if (s_current_usage >= record.size) {
        s_current_usage -= record.size;
    } else {
        s_current_usage = 0;
    }

    if (s_active_allocation_count > 0U) {
        --s_active_allocation_count;
    }

    const size_t budget_limit = s_budgets[category_index];
    s_budget_exceeded[category_index] = (budget_limit > 0U && s_category_usage[category_index] > budget_limit);
}

TelemetrySnapshot MemoryTracker::get_snapshot() {
    SpinLockGuard guard;
    assert(s_initialized && "MemoryTracker not initialized");

    TelemetrySnapshot snapshot;
    snapshot.total_allocated = s_total_allocated;
    snapshot.total_freed = s_total_freed;
    snapshot.active_allocations = s_active_allocation_count;
    snapshot.peak_usage = s_peak_usage;
    snapshot.leak_count = s_active_allocation_count;

    for (size_t i = 0; i < static_cast<size_t>(AllocationCategory::Count); ++i) {
        snapshot.category_usage[i] = s_category_usage[i];
    }

    return snapshot;
}

BudgetInfo MemoryTracker::get_budget(AllocationCategory category) {
    SpinLockGuard guard;
    assert(s_initialized && "MemoryTracker not initialized");
    assert(is_valid_category(category) && "Invalid allocation category");

    BudgetInfo info;
    if (!is_valid_category(category)) {
        return info;
    }

    const size_t category_index = static_cast<size_t>(category);
    info.max_bytes = s_budgets[category_index];
    info.current_bytes = s_category_usage[category_index];
    info.exceeded = s_budget_exceeded[category_index];
    return info;
}

void MemoryTracker::set_budget(AllocationCategory category, size_t max_bytes) {
    SpinLockGuard guard;
    assert(s_initialized && "MemoryTracker not initialized");
    assert(is_valid_category(category) && "Invalid allocation category");

    if (!is_valid_category(category)) {
        return;
    }

    const size_t category_index = static_cast<size_t>(category);
    s_budgets[category_index] = max_bytes;
    s_budget_exceeded[category_index] = (max_bytes > 0U && s_category_usage[category_index] > max_bytes);
}

bool MemoryTracker::check_leaks() {
    SpinLockGuard guard;
    assert(s_initialized && "MemoryTracker not initialized");
    return s_active_allocation_count == 0U;
}

size_t MemoryTracker::get_active_allocation_count() {
    SpinLockGuard guard;
    assert(s_initialized && "MemoryTracker not initialized");
    return s_active_allocation_count;
}

size_t MemoryTracker::get_peak_usage() {
    SpinLockGuard guard;
    assert(s_initialized && "MemoryTracker not initialized");
    return s_peak_usage;
}

size_t MemoryTracker::get_leak_count() {
    SpinLockGuard guard;
    assert(s_initialized && "MemoryTracker not initialized");
    return s_active_allocation_count;
}

const AllocationRecord* MemoryTracker::get_active_records(size_t& out_count) {
    SpinLockGuard guard;
    assert(s_initialized && "MemoryTracker not initialized");

    size_t write_index = 0;
    for (size_t i = 0; i < MAX_TRACKED_ALLOCATIONS; ++i) {
        if (s_records[i].active) {
            s_active_records[write_index] = s_records[i];
            ++write_index;
        }
    }

    out_count = write_index;
    return s_active_records;
}

} // namespace acheron::memory
