#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstdarg>

namespace acheron::gameplay::progression {

// =============================================================================
// PROGRESSION STATE STRUCTURES (POD-Compatible)
// =============================================================================

/// Progression event types (event-driven system)
enum class ProgressionEventType : uint8_t
{
    PopulationThresholdReached = 0,
    TradeVolumeExceeded = 1,
    TrafficEfficiencyAchieved = 2,
    PollutionLimitExceeded = 3,
    TaxRevenueIncreased = 4,
    ServiceUptimeReached = 5,
    EconomicStabilityImproved = 6,
    InfrastructureMaintained = 7,
    TechResearchCompleted = 8,
    LogisticsOptimized = 9,
    DistrictDensityIncreased = 10
};

/// Progression unlock flags (bitset for cache-friendly checks)
enum class UnlockFlags : uint64_t
{
    // District unlocks
    UnlockHighDensityResidential = 1ULL << 0,
    UnlockMixedUseDevelopment = 1ULL << 1,
    UnlockSkycrapers = 1ULL << 2,
    UnlockMegaDistricts = 1ULL << 3,
    
    // Logistics unlocks
    UnlockFreightRail = 1ULL << 4,
    UnlockDroneDelivery = 1ULL << 5,
    UnlockCargoHubs = 1ULL << 6,
    UnlockAutomatedRouting = 1ULL << 7,
    
    // Infrastructure unlocks
    UnlockAdvancedRoads = 1ULL << 8,
    UnlockPublicTransit = 1ULL << 9,
    UnlockUnderground = 1ULL << 10,
    UnlockHighSpeedRail = 1ULL << 11,
    
    // Technology unlocks
    UnlockEfficiencyOptimization = 1ULL << 12,
    UnlockPollutionReduction = 1ULL << 13,
    UnlockPredictiveRouting = 1ULL << 14,
    UnlockAutomationSystems = 1ULL << 15,
    
    // Economic unlocks
    UnlockAdvancedTrade = 1ULL << 16,
    UnlockFinancialMarkets = 1ULL << 17,
    UnlockMultiCommodityTrade = 1ULL << 18,
    UnlockEconomicSpecialization = 1ULL << 19
};

/// Event-driven progression event
struct ProgressionEvent
{
    ProgressionEventType type;      // Event classification
    uint32_t district_id;            // Which district triggered (if applicable)
    uint64_t value;                  // Event value (population, revenue, etc)
    float metric;                    // Auxiliary metric (efficiency, stability, etc)
};

/// Core progression state (POD-compatible, serializable)
/// This is the canonical progression data - NOT owned by UI
struct ProgressionState
{
    // --- Population Progression ---
    uint64_t total_population;       // Total city population
    uint32_t population_level;       // Population tier (0-255)
    uint32_t _pad1;
    
    // --- Economic Progression ---
    uint64_t total_income;           // Total tax revenue
    uint32_t economic_level;         // Economic progression tier
    uint32_t _pad2;
    
    // --- Infrastructure Progression ---
    uint64_t total_trade_volume;     // Cumulative trade volume
    uint32_t infrastructure_level;   // Infrastructure tier (roads → rail → etc)
    uint32_t _pad3;
    
    // --- District Progression ---
    uint32_t district_level;         // District zoning tier
    uint32_t district_expansion;     // Number of active districts
    
    // --- Logistics Progression ---
    uint32_t logistics_level;        // Logistics capability tier
    uint32_t logistics_efficiency;   // [0..1000] normalized efficiency
    
    // --- Technology Progression ---
    uint32_t technology_level;       // Tech tree depth
    uint32_t research_progress;      // [0..1000] research points
    
    // --- Simulation Difficulty ---
    float city_stability;            // [0..1] stability metric
    float resource_pressure;         // [0..1] pressure metric
    
    // --- Unlock Flags (bitset) ---
    uint64_t unlocked_flags;         // Bitmask of active unlocks
    
    // --- Metadata ---
    uint64_t total_frame_count;      // Total frames for determinism validation
    uint32_t version;                // State version for compatibility
    uint32_t _pad_final;
};

// Compile-time verification that ProgressionState is POD-compatible
static_assert(std::is_trivial_v<ProgressionState>, "ProgressionState must be trivially copyable");
static_assert(std::is_standard_layout_v<ProgressionState>, "ProgressionState must be standard layout");

/// Progression unlock rule (data-driven, not hardcoded)
struct UnlockRule
{
    UnlockFlags unlock_id;           // Which unlock does this rule trigger
    uint64_t population_required;    // Population threshold (0 = no requirement)
    uint64_t income_required;        // Income threshold
    uint64_t trade_volume_required;  // Trade volume threshold
    uint32_t infrastructure_level;   // Min infrastructure level needed
    float stability_required;        // Min stability [0..1] (0 = no requirement)
    float efficiency_required;       // Min efficiency [0..1]
};

// =============================================================================
// PROGRESSION SYSTEM
// =============================================================================

/// Core progression system (manages deterministic progression)
class ProgressionSystem
{
public:
    static constexpr uint32_t MAX_EVENTS = 256;
    static constexpr uint32_t MAX_UNLOCKS = 64;

    /// Initialize progression system
    /// @return true if initialization successful
    bool initialize() noexcept;

    /// Shutdown progression system
    void shutdown() noexcept;

    /// Process a progression event (event-driven)
    /// @param event Input event from simulation
    /// @return true if event was processed successfully
    bool process_event(const ProgressionEvent& event) noexcept;

    /// Update progression system
    /// Called once per simulation frame (after all events processed)
    /// @return true if progression state was modified
    bool update() noexcept;

    /// Get current progression state (const, read-only snapshot)
    const ProgressionState& get_state() const noexcept { return m_state; }

    /// Get mutable state reference for initialization/replay
    ProgressionState& get_mutable_state() noexcept { return m_state; }

    /// Check if a specific unlock is active
    /// @param flag Unlock to check
    /// @return true if unlock is currently active
    bool is_unlocked(UnlockFlags flag) const noexcept
    {
        return (m_state.unlocked_flags & static_cast<uint64_t>(flag)) != 0;
    }

    /// Force unlock (for testing/admin commands - NOT recommended in production)
    void force_unlock(UnlockFlags flag) noexcept
    {
        m_state.unlocked_flags |= static_cast<uint64_t>(flag);
    }

    /// Clear all unlocks (for replay/reset)
    void reset_unlocks() noexcept
    {
        m_state.unlocked_flags = 0;
    }

    /// Register an unlock rule
    /// @param rule Rule to register
    /// @return true if registered successfully
    bool register_unlock_rule(const UnlockRule& rule) noexcept;

    /// Get current frame count (for determinism validation)
    uint64_t get_frame_count() const noexcept
    {
        return m_state.total_frame_count;
    }

    /// Check if system is initialized
    bool is_initialized() const noexcept
    {
        return m_initialized;
    }

    /// Destructor
    ~ProgressionSystem() noexcept;

private:
    // --- Core State ---
    ProgressionState m_state = {};
    UnlockRule m_unlock_rules[MAX_UNLOCKS] = {};
    uint32_t m_unlock_rule_count = 0;
    
    // --- Event Queue (fixed-size, no allocation) ---
    ProgressionEvent m_event_queue[MAX_EVENTS];
    uint32_t m_event_count = 0;
    
    // --- Initialization State ---
    bool m_initialized = false;
    
    // --- Private Methods ---
    void evaluate_unlocks() noexcept;
    bool evaluate_unlock_rule(const UnlockRule& rule) noexcept;
    void process_event_queue() noexcept;
    void update_progression_levels() noexcept;
    
    // --- Determinism Helpers ---
    void increment_frame_count() noexcept
    {
        m_state.total_frame_count++;
    }
};

}  // namespace acheron::gameplay::progression

// =============================================================================
// UNIT TESTS (ifdef ACHERON_PROGRESSION_SYSTEM_UNIT_TEST)
// =============================================================================

#ifdef ACHERON_PROGRESSION_SYSTEM_UNIT_TEST

#include <cassert>
#include <cstdio>

namespace acheron::gameplay::progression::tests {

constexpr uint32_t test_output_buffer_size = 16384;
static char test_output[test_output_buffer_size];
static size_t test_output_index = 0;

void test_printf(const char* fmt, ...) noexcept
{
    va_list args;
    va_start(args, fmt);
    int written = std::vsnprintf(test_output + test_output_index,
                                 test_output_buffer_size - test_output_index,
                                 fmt, args);
    va_end(args);
    if (written > 0)
    {
        test_output_index += written;
    }
}

bool run_all_tests() noexcept
{
    test_output_index = 0;
    test_printf("=== PROGRESSION SYSTEM UNIT TESTS ===\n");
    
    // Test 1: Initialization
    {
        test_printf("\n[TEST 1] Initialization\n");
        ProgressionSystem sys;
        assert(!sys.is_initialized());
        
        bool init_ok = sys.initialize();
        assert(init_ok);
        assert(sys.is_initialized());
        
        const auto& state = sys.get_state();
        assert(state.total_population == 0);
        assert(state.total_income == 0);
        assert(state.unlocked_flags == 0);
        assert(state.total_frame_count == 0);
        
        test_printf("✓ Initialization test passed\n");
        sys.shutdown();
    }
    
    // Test 2: Event Processing (deterministic)
    {
        test_printf("\n[TEST 2] Event Processing - Determinism\n");
        ProgressionSystem sys1, sys2;
        sys1.initialize();
        sys2.initialize();
        
        // Process identical events in both systems
        ProgressionEvent event1 = {
            ProgressionEventType::PopulationThresholdReached,
            0,
            5000,  // population value
            0.8f   // stability
        };
        
        sys1.process_event(event1);
        sys1.update();
        
        sys2.process_event(event1);
        sys2.update();
        
        // Verify both systems have identical state
        const auto& state1 = sys1.get_state();
        const auto& state2 = sys2.get_state();
        
        assert(state1.total_population == state2.total_population);
        assert(state1.population_level == state2.population_level);
        assert(state1.unlocked_flags == state2.unlocked_flags);
        assert(state1.total_frame_count == state2.total_frame_count);
        
        test_printf("✓ Event processing determinism test passed\n");
        sys1.shutdown();
        sys2.shutdown();
    }
    
    // Test 3: Progression State Updates
    {
        test_printf("\n[TEST 3] Progression State Updates\n");
        ProgressionSystem sys;
        sys.initialize();
        
        auto& state = sys.get_mutable_state();
        state.total_population = 10000;
        state.total_income = 5000;
        state.city_stability = 0.85f;
        
        sys.update();  // May return true/false depending on unlocks
        
        const auto& read_state = sys.get_state();
        assert(read_state.total_population == 10000);
        assert(read_state.total_income == 5000);
        assert(read_state.city_stability == 0.85f);
        
        test_printf("✓ State update test passed\n");
        sys.shutdown();
    }
    
    // Test 4: Unlock Registration and Evaluation
    {
        test_printf("\n[TEST 4] Unlock Registration and Evaluation\n");
        ProgressionSystem sys;
        sys.initialize();
        
        UnlockRule rule = {
            UnlockFlags::UnlockHighDensityResidential,
            5000,      // population_required
            0,         // income_required
            0,         // trade_volume_required
            1,         // infrastructure_level
            0.75f,     // stability_required
            0.0f       // efficiency_required
        };
        
        bool reg_ok = sys.register_unlock_rule(rule);
        assert(reg_ok);
        
        // Before meeting requirements - no unlock
        assert(!sys.is_unlocked(UnlockFlags::UnlockHighDensityResidential));
        
        // Meet requirements and update
        auto& state = sys.get_mutable_state();
        state.total_population = 5000;
        state.infrastructure_level = 1;
        state.city_stability = 0.8f;
        
        sys.update();
        
        // After meeting all requirements - should unlock
        assert(sys.is_unlocked(UnlockFlags::UnlockHighDensityResidential));
        
        test_printf("✓ Unlock evaluation test passed\n");
        sys.shutdown();
    }
    
    // Test 5: Replay Safety (identical state progression)
    {
        test_printf("\n[TEST 5] Replay Safety\n");
        
        // Simulate twice with identical events
        ProgressionSystem sys1, sys2;
        sys1.initialize();
        sys2.initialize();
        
        // Register same rules
        UnlockRule rule = {
            UnlockFlags::UnlockFreightRail,
            0,         // population_required
            10000,     // income_required (focus on income)
            0,         // trade_volume_required
            2,         // infrastructure_level
            0.0f,      // stability_required
            0.0f       // efficiency_required
        };
        
        sys1.register_unlock_rule(rule);
        sys2.register_unlock_rule(rule);
        
        // Identical event sequence
        ProgressionEvent events[] = {
            { ProgressionEventType::TaxRevenueIncreased, 0, 5000, 0.7f },
            { ProgressionEventType::TaxRevenueIncreased, 0, 5000, 0.7f },
            { ProgressionEventType::InfrastructureMaintained, 0, 1, 0.8f },
        };
        
        for (const auto& evt : events)
        {
            sys1.process_event(evt);
            sys2.process_event(evt);
        }
        
        sys1.update();
        sys2.update();
        
        const auto& state1 = sys1.get_state();
        const auto& state2 = sys2.get_state();
        
        // Verify complete state equality (replay safety)
        assert(state1.total_income == state2.total_income);
        assert(state1.unlocked_flags == state2.unlocked_flags);
        assert(state1.total_frame_count == state2.total_frame_count);
        
        test_printf("✓ Replay safety test passed\n");
        sys1.shutdown();
        sys2.shutdown();
    }
    
    // Test 6: POD Validation (no virtual functions, trivially copyable)
    {
        test_printf("\n[TEST 6] POD Validation\n");
        
        // Verify ProgressionState is POD
        static_assert(std::is_trivial_v<ProgressionState>);
        static_assert(std::is_standard_layout_v<ProgressionState>);
        
        // Test memory copy (POD requirement)
        ProgressionState state1 = {};
        state1.total_population = 12345;
        state1.unlocked_flags = 42;
        
        ProgressionState state2;
        std::memcpy(&state2, &state1, sizeof(ProgressionState));
        
        assert(state2.total_population == state1.total_population);
        assert(state2.unlocked_flags == state1.unlocked_flags);
        
        test_printf("✓ POD validation test passed\n");
    }
    
    // Test 7: Unlock Flag Bitset Operations
    {
        test_printf("\n[TEST 7] Unlock Flag Bitset Operations\n");
        ProgressionSystem sys;
        sys.initialize();
        
        // Test force_unlock
        sys.force_unlock(UnlockFlags::UnlockHighDensityResidential);
        assert(sys.is_unlocked(UnlockFlags::UnlockHighDensityResidential));
        
        // Other unlocks should still be inactive
        assert(!sys.is_unlocked(UnlockFlags::UnlockFreightRail));
        
        // Unlock multiple
        sys.force_unlock(UnlockFlags::UnlockFreightRail);
        sys.force_unlock(UnlockFlags::UnlockDroneDelivery);
        
        assert(sys.is_unlocked(UnlockFlags::UnlockHighDensityResidential));
        assert(sys.is_unlocked(UnlockFlags::UnlockFreightRail));
        assert(sys.is_unlocked(UnlockFlags::UnlockDroneDelivery));
        
        // Test reset
        sys.reset_unlocks();
        assert(!sys.is_unlocked(UnlockFlags::UnlockHighDensityResidential));
        assert(!sys.is_unlocked(UnlockFlags::UnlockFreightRail));
        assert(!sys.is_unlocked(UnlockFlags::UnlockDroneDelivery));
        
        test_printf("✓ Unlock flag bitset test passed\n");
        sys.shutdown();
    }
    
    // Test 8: Frame Count Incrementing (determinism validation)
    {
        test_printf("\n[TEST 8] Frame Count Incrementing\n");
        ProgressionSystem sys;
        sys.initialize();
        
        assert(sys.get_frame_count() == 0);
        
        sys.update();
        uint64_t count1 = sys.get_frame_count();
        
        sys.update();
        uint64_t count2 = sys.get_frame_count();
        
        sys.update();
        uint64_t count3 = sys.get_frame_count();
        
        assert(count1 == 1);
        assert(count2 == 2);
        assert(count3 == 3);
        
        test_printf("✓ Frame count incrementing test passed\n");
        sys.shutdown();
    }
    
    // Test 9: Scaling/Progression Levels
    {
        test_printf("\n[TEST 9] Scaling/Progression Levels\n");
        ProgressionSystem sys;
        sys.initialize();
        
        auto& state = sys.get_mutable_state();
        
        // Test population level scaling
        state.total_population = 0;
        sys.update();
        uint32_t level0 = state.population_level;
        
        state.total_population = 5000;
        sys.update();
        uint32_t level1 = state.population_level;
        
        state.total_population = 50000;
        sys.update();
        uint32_t level2 = state.population_level;
        
        // Levels should scale with population
        assert(level2 >= level1);
        assert(level1 >= level0);
        
        test_printf("✓ Scaling/progression levels test passed (levels: %u -> %u -> %u)\n",
                   level0, level1, level2);
        sys.shutdown();
    }
    
    // Test 10: Multiple Concurrent Rules
    {
        test_printf("\n[TEST 10] Multiple Concurrent Rules\n");
        ProgressionSystem sys;
        sys.initialize();
        
        // Register multiple rules
        UnlockRule rule1 = {
            UnlockFlags::UnlockHighDensityResidential,
            5000, 0, 0, 1, 0.75f, 0.0f
        };
        UnlockRule rule2 = {
            UnlockFlags::UnlockMixedUseDevelopment,
            10000, 0, 0, 2, 0.8f, 0.0f
        };
        UnlockRule rule3 = {
            UnlockFlags::UnlockFreightRail,
            0, 15000, 0, 1, 0.0f, 0.0f
        };
        
        sys.register_unlock_rule(rule1);
        sys.register_unlock_rule(rule2);
        sys.register_unlock_rule(rule3);
        
        // Set state to trigger rule1 and rule3 but not rule2
        auto& state = sys.get_mutable_state();
        state.total_population = 5000;
        state.infrastructure_level = 1;
        state.city_stability = 0.8f;
        state.total_income = 15000;
        
        sys.update();
        
        // Verify correct unlocks
        assert(sys.is_unlocked(UnlockFlags::UnlockHighDensityResidential));
        assert(!sys.is_unlocked(UnlockFlags::UnlockMixedUseDevelopment));
        assert(sys.is_unlocked(UnlockFlags::UnlockFreightRail));
        
        test_printf("✓ Multiple concurrent rules test passed\n");
        sys.shutdown();
    }
    
    test_printf("\n=== ALL TESTS PASSED ===\n");
    test_printf("\nTest Summary: 10/10 tests passed\n");
    
    // Output results
    std::printf("%s", test_output);
    
    return true;
}

}  // namespace acheron::gameplay::progression::tests

#endif  // ACHERON_PROGRESSION_SYSTEM_UNIT_TEST
