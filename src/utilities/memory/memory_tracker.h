#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace acheron::memory {

enum class AllocationCategory : uint8_t {
    Unknown = 0,
    ECS,
    Renderer,
    Simulation,
    Assets,
    Audio,
    UI,
    Networking,
    Physics,
    Count
};

struct AllocationRecord {
    void* ptr = nullptr;
    size_t size = 0;
    AllocationCategory category = AllocationCategory::Unknown;
    uint32_t owner_id = 0;
    bool active = false;
    const char* file = nullptr;
    int line = 0;
};

struct BudgetInfo {
    size_t max_bytes = 0;
    size_t current_bytes = 0;
    bool exceeded = false;
};

struct TelemetrySnapshot {
    size_t total_allocated = 0;
    size_t total_freed = 0;
    size_t active_allocations = 0;
    size_t peak_usage = 0;
    size_t category_usage[static_cast<size_t>(AllocationCategory::Count)] = {};
    size_t leak_count = 0;
};

class MemoryTracker {
public:
    static void initialize();
    static void shutdown();

    static void track_allocation(void* ptr, size_t size, AllocationCategory category, uint32_t owner_id, const char* file, int line);
    static void track_deallocation(void* ptr);

    static TelemetrySnapshot get_snapshot();
    static BudgetInfo get_budget(AllocationCategory category);
    static void set_budget(AllocationCategory category, size_t max_bytes);

    static bool check_leaks();
    static size_t get_active_allocation_count();
    static size_t get_peak_usage();
    static size_t get_leak_count();

    static const AllocationRecord* get_active_records(size_t& out_count);

private:
    static constexpr size_t MAX_TRACKED_ALLOCATIONS = 65536;

    static AllocationRecord s_records[MAX_TRACKED_ALLOCATIONS];
    static AllocationRecord s_active_records[MAX_TRACKED_ALLOCATIONS];
    static size_t s_active_allocation_count;

    static size_t s_budgets[static_cast<size_t>(AllocationCategory::Count)];
    static size_t s_category_usage[static_cast<size_t>(AllocationCategory::Count)];
    static bool s_budget_exceeded[static_cast<size_t>(AllocationCategory::Count)];

    static size_t s_total_allocated;
    static size_t s_total_freed;
    static size_t s_current_usage;
    static size_t s_peak_usage;
    static bool s_initialized;

    [[nodiscard]] static bool is_valid_category(AllocationCategory category) noexcept;
    [[nodiscard]] static size_t hash_ptr(const void* ptr) noexcept;
    [[nodiscard]] static size_t find_slot_for_insert(const void* ptr) noexcept;
    [[nodiscard]] static size_t find_active_slot(const void* ptr) noexcept;
};

} // namespace acheron::memory
