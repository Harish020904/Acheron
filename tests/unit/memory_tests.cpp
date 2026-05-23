#include "../../src/utilities/memory/linear_allocator.h"
#include "../../src/utilities/memory/frame_allocator.h"
#include "../../src/utilities/memory/pool_allocator.h"
#include "../../src/utilities/memory/memory_tracker.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <chrono>
#include <thread>
#include <cstring>

using namespace acheron::memory;

void test_linear_allocator() {
    std::cout << "Testing LinearAllocator..." << std::endl;
    
    std::vector<std::byte> buffer(1024);
    LinearAllocator allocator(buffer);

    // Alignment test
    void* p1 = allocator.allocate(10, 16);
    assert(reinterpret_cast<uintptr_t>(p1) % 16 == 0);
    
    void* p2 = allocator.allocate(10, 64);
    assert(reinterpret_cast<uintptr_t>(p2) % 64 == 0);

    // Alignment edge cases
    void* p3 = allocator.allocate(1, 1);
    assert(reinterpret_cast<uintptr_t>(p3) % 1 == 0);
    
    void* p4 = allocator.allocate(8, 8);
    assert(reinterpret_cast<uintptr_t>(p4) % 8 == 0);

    // Capacity test
    allocator.reset();
    assert(allocator.used() == 0);
    
    void* p5 = allocator.allocate(1024);
    assert(p5 != nullptr);
    assert(allocator.remaining() == 0);
    
    void* p6 = allocator.allocate(1);
    assert(p6 == nullptr);

    // Zero allocation test
    allocator.reset();
    void* p7 = allocator.allocate(0);
    assert(p7 == nullptr);

    // Benchmark
    auto start = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < 1000000; ++i) {
        allocator.reset();
        allocator.allocate(16);
        allocator.allocate(32);
        allocator.allocate(64);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    std::cout << "LinearAllocator 3M allocations + 1M resets: " << diff.count() << "s" << std::endl;
    
    std::cout << "LinearAllocator passed." << std::endl;
}

void test_frame_allocator() {
    std::cout << "Testing FrameAllocator..." << std::endl;
    
    FrameAllocator::initialize(1024 * 1024);
    assert(FrameAllocator::is_initialized());

    // Stress test
    for (int frame = 0; frame < 100; ++frame) {
        for (int i = 0; i < 1000; ++i) {
            void* p = FrameAllocator::allocate(128);
            assert(p != nullptr);
        }
        FrameAllocator::reset();
    }

    void* p_large = FrameAllocator::allocate(512 * 1024);
    assert(p_large != nullptr);
    FrameAllocator::reset();
    
    void* p_after_reset = FrameAllocator::allocate(512 * 1024);
    assert(p_after_reset != nullptr);

    // Thread-local test
    std::thread t1([]() {
        assert(!FrameAllocator::is_initialized());
        FrameAllocator::initialize(1024);
        void* p = FrameAllocator::allocate(512);
        assert(p != nullptr);
        FrameAllocator::shutdown();
    });
    t1.join();

    // Verify main thread state is unaffected by other thread
    assert(FrameAllocator::is_initialized());
    
    FrameAllocator::shutdown();
    std::cout << "FrameAllocator passed." << std::endl;
}

void test_pool_allocator() {
    std::cout << "Testing PoolAllocator..." << std::endl;

    const size_t block_size = 32;
    const size_t num_blocks = 10;
    std::vector<std::byte> buffer(block_size * num_blocks + 64);
    PoolAllocator pool(buffer, block_size, 16);

    std::vector<void*> ptrs;
    for (size_t i = 0; i < pool.capacity(); ++i) {
        void* p = pool.allocate();
        assert(p != nullptr);
        assert(reinterpret_cast<uintptr_t>(p) % 16 == 0);
        ptrs.push_back(p);
    }

    assert(pool.allocate() == nullptr);
    assert(pool.used_count() == pool.capacity());

    // Reuse test
    void* last_ptr = ptrs.back();
    pool.deallocate(last_ptr);
    assert(pool.used_count() == pool.capacity() - 1);
    
    void* new_ptr = pool.allocate();
    assert(new_ptr == last_ptr);
    assert(pool.used_count() == pool.capacity());

    // Multiple deallocate/allocate cycle
    for (size_t i = 0; i < 5; ++i) {
        pool.deallocate(ptrs[i]);
    }
    assert(pool.used_count() == pool.capacity() - 5);
    
    for (size_t i = 0; i < 5; ++i) {
        void* p = pool.allocate();
        assert(p != nullptr);
    }
    assert(pool.used_count() == pool.capacity());

    // Null deallocate test
    pool.deallocate(nullptr);

    std::cout << "PoolAllocator passed." << std::endl;
}

void test_memory_tracker() {
    std::cout << "Testing MemoryTracker..." << std::endl;

    MemoryTracker::initialize();

    // Test basic allocation tracking
    int dummy1 = 0, dummy2 = 0, dummy3 = 0;
    MemoryTracker::track_allocation(&dummy1, 64, AllocationCategory::ECS, 1, "test.cpp", 10);
    MemoryTracker::track_allocation(&dummy2, 128, AllocationCategory::Renderer, 2, "test.cpp", 11);
    MemoryTracker::track_allocation(&dummy3, 256, AllocationCategory::Simulation, 3, "test.cpp", 12);

    // Verify active allocation count
    assert(MemoryTracker::get_active_allocation_count() == 3);

    // Verify telemetry snapshot
    TelemetrySnapshot snapshot = MemoryTracker::get_snapshot();
    assert(snapshot.total_allocated == 64 + 128 + 256);
    assert(snapshot.total_freed == 0);
    assert(snapshot.active_allocations == 3);
    assert(snapshot.peak_usage == 64 + 128 + 256);

    // Test deallocation tracking
    MemoryTracker::track_deallocation(&dummy1);
    assert(MemoryTracker::get_active_allocation_count() == 2);

    snapshot = MemoryTracker::get_snapshot();
    assert(snapshot.total_freed == 64);
    assert(snapshot.active_allocations == 2);

    // Test budget enforcement
    MemoryTracker::set_budget(AllocationCategory::ECS, 1024);
    MemoryTracker::set_budget(AllocationCategory::Renderer, 64);

    BudgetInfo ecs_budget = MemoryTracker::get_budget(AllocationCategory::ECS);
    assert(ecs_budget.max_bytes == 1024);
    assert(ecs_budget.current_bytes == 0);
    assert(!ecs_budget.exceeded);

    BudgetInfo renderer_budget = MemoryTracker::get_budget(AllocationCategory::Renderer);
    assert(renderer_budget.max_bytes == 64);
    assert(renderer_budget.current_bytes == 128);
    assert(renderer_budget.exceeded);

    // Test leak detection
    MemoryTracker::track_deallocation(&dummy2);
    MemoryTracker::track_deallocation(&dummy3);
    
    bool no_leaks = MemoryTracker::check_leaks();
    assert(no_leaks);
    assert(MemoryTracker::get_leak_count() == 0);

    // Test leak scenario
    int leak_dummy = 0;
    MemoryTracker::track_allocation(&leak_dummy, 32, AllocationCategory::Audio, 4, "test.cpp", 20);
    
    assert(MemoryTracker::get_leak_count() == 1);
    assert(!MemoryTracker::check_leaks());

    // Test peak usage tracking
    size_t peak = MemoryTracker::get_peak_usage();
    assert(peak > 0);

    // Test active records retrieval
    size_t record_count = 0;
    const AllocationRecord* records = MemoryTracker::get_active_records(record_count);
    assert(record_count == 1);
    assert(records != nullptr);
    bool found_leak = false;
    for (size_t i = 0; i < record_count; ++i) {
        if (records[i].active && records[i].ptr == &leak_dummy) {
            found_leak = true;
            assert(records[i].size == 32);
            assert(records[i].category == AllocationCategory::Audio);
            assert(records[i].owner_id == 4);
            assert(std::string_view(records[i].file) == "test.cpp");
            assert(records[i].line == 20);
        }
    }
    assert(found_leak);

    // Clean up leak
    MemoryTracker::track_deallocation(&leak_dummy);

    MemoryTracker::shutdown();
    std::cout << "MemoryTracker passed." << std::endl;
}

void test_memory_tracker_budget_enforcement() {
    std::cout << "Testing MemoryTracker budget enforcement..." << std::endl;

    MemoryTracker::initialize();

    // Set a strict budget
    MemoryTracker::set_budget(AllocationCategory::ECS, 256);

    // Allocate within budget
    int ptr1 = 0, ptr2 = 0;
    MemoryTracker::track_allocation(&ptr1, 128, AllocationCategory::ECS, 1, "test.cpp", 30);
    MemoryTracker::track_allocation(&ptr2, 64, AllocationCategory::ECS, 1, "test.cpp", 31);

    BudgetInfo budget = MemoryTracker::get_budget(AllocationCategory::ECS);
    assert(budget.current_bytes == 192);
    assert(!budget.exceeded);

    // This would exceed budget (asserts in debug)
    // int ptr3 = 0;
    // MemoryTracker::track_allocation(&ptr3, 128, AllocationCategory::ECS, 1, "test.cpp", 32);

    // Free and verify budget updates
    MemoryTracker::track_deallocation(&ptr1);
    budget = MemoryTracker::get_budget(AllocationCategory::ECS);
    assert(budget.current_bytes == 64);

    MemoryTracker::track_deallocation(&ptr2);
    budget = MemoryTracker::get_budget(AllocationCategory::ECS);
    assert(budget.current_bytes == 0);

    MemoryTracker::shutdown();
    std::cout << "MemoryTracker budget enforcement passed." << std::endl;
}

void test_memory_tracker_categories() {
    std::cout << "Testing MemoryTracker categories..." << std::endl;

    MemoryTracker::initialize();

    // Allocate in different categories
    int ecs_ptr = 0, renderer_ptr = 0, sim_ptr = 0;
    MemoryTracker::track_allocation(&ecs_ptr, 100, AllocationCategory::ECS, 1, "test.cpp", 40);
    MemoryTracker::track_allocation(&renderer_ptr, 200, AllocationCategory::Renderer, 2, "test.cpp", 41);
    MemoryTracker::track_allocation(&sim_ptr, 300, AllocationCategory::Simulation, 3, "test.cpp", 42);

    TelemetrySnapshot snapshot = MemoryTracker::get_snapshot();
    assert(snapshot.category_usage[static_cast<size_t>(AllocationCategory::ECS)] == 100);
    assert(snapshot.category_usage[static_cast<size_t>(AllocationCategory::Renderer)] == 200);
    assert(snapshot.category_usage[static_cast<size_t>(AllocationCategory::Simulation)] == 300);

    // Verify total
    size_t total_category_usage = 0;
    for (size_t i = 0; i < static_cast<size_t>(AllocationCategory::Count); ++i) {
        total_category_usage += snapshot.category_usage[i];
    }
    assert(total_category_usage == 600);

    MemoryTracker::track_deallocation(&ecs_ptr);
    MemoryTracker::track_deallocation(&renderer_ptr);
    MemoryTracker::track_deallocation(&sim_ptr);

    MemoryTracker::shutdown();
    std::cout << "MemoryTracker categories passed." << std::endl;
}

void test_memory_tracker_leak_detection() {
    std::cout << "Testing MemoryTracker leak detection..." << std::endl;

    MemoryTracker::initialize();

    // Create some allocations
    int ptr1 = 0, ptr2 = 0, ptr3 = 0;
    MemoryTracker::track_allocation(&ptr1, 50, AllocationCategory::ECS, 1, "test.cpp", 50);
    MemoryTracker::track_allocation(&ptr2, 75, AllocationCategory::Renderer, 2, "test.cpp", 51);
    MemoryTracker::track_allocation(&ptr3, 100, AllocationCategory::Simulation, 3, "test.cpp", 52);

    // Free only some
    MemoryTracker::track_deallocation(&ptr1);
    MemoryTracker::track_deallocation(&ptr3);

    // Should detect 1 leak
    assert(MemoryTracker::get_leak_count() == 1);
    assert(!MemoryTracker::check_leaks());

    // Get active records to find the leak
    size_t count = 0;
    const AllocationRecord* records = MemoryTracker::get_active_records(count);
    assert(count >= 1);

    bool found_leak = false;
    for (size_t i = 0; i < count; ++i) {
        if (records[i].active && records[i].ptr == &ptr2) {
            found_leak = true;
            assert(records[i].size == 75);
            assert(records[i].category == AllocationCategory::Renderer);
            assert(records[i].owner_id == 2);
        }
    }
    assert(found_leak);

    // Clean up
    MemoryTracker::track_deallocation(&ptr2);
    assert(MemoryTracker::get_leak_count() == 0);
    assert(MemoryTracker::check_leaks());

    MemoryTracker::shutdown();
    std::cout << "MemoryTracker leak detection passed." << std::endl;
}

int main() {
    test_linear_allocator();
    test_frame_allocator();
    test_pool_allocator();
    test_memory_tracker();
    test_memory_tracker_budget_enforcement();
    test_memory_tracker_categories();
    test_memory_tracker_leak_detection();
    std::cout << "All memory tests passed!" << std::endl;
    return 0;
}
