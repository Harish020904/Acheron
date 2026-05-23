#include "progression_system.h"
#include <cstring>
#include <algorithm>

namespace acheron::gameplay::progression {

// =============================================================================
// PROGRESSION SYSTEM IMPLEMENTATION
// =============================================================================

bool ProgressionSystem::initialize() noexcept
{
    if (m_initialized)
    {
        return true;  // Already initialized
    }

    // Initialize state to zero
    std::memset(&m_state, 0, sizeof(ProgressionState));
    m_state.version = 1;
    m_state.city_stability = 1.0f;  // Start with high stability
    m_state.resource_pressure = 0.0f;

    // Clear event queue
    m_event_count = 0;
    std::memset(m_event_queue, 0, sizeof(m_event_queue));

    // Clear unlock rules
    m_unlock_rule_count = 0;
    std::memset(m_unlock_rules, 0, sizeof(m_unlock_rules));

    m_initialized = true;
    return true;
}

void ProgressionSystem::shutdown() noexcept
{
    // No dynamic allocations to clean up (POD-only, fixed-size buffers)
    m_initialized = false;
    m_event_count = 0;
    m_unlock_rule_count = 0;
}

bool ProgressionSystem::process_event(const ProgressionEvent& event) noexcept
{
    if (!m_initialized)
    {
        return false;  // System not initialized
    }

    if (m_event_count >= MAX_EVENTS)
    {
        // Event queue full - drop event (deterministic, no allocation)
        return false;
    }

    // Add event to queue (deterministic ordering)
    m_event_queue[m_event_count] = event;
    m_event_count++;
    
    return true;
}

void ProgressionSystem::process_event_queue() noexcept
{
    // Process all queued events in deterministic order
    for (uint32_t i = 0; i < m_event_count; ++i)
    {
        const auto& evt = m_event_queue[i];

        switch (evt.type)
        {
            case ProgressionEventType::PopulationThresholdReached:
                m_state.total_population = std::max(m_state.total_population, evt.value);
                break;

            case ProgressionEventType::TaxRevenueIncreased:
                m_state.total_income += evt.value;
                break;

            case ProgressionEventType::TradeVolumeExceeded:
                m_state.total_trade_volume += evt.value;
                break;

            case ProgressionEventType::TrafficEfficiencyAchieved:
                m_state.logistics_efficiency = 
                    std::min(1000u, m_state.logistics_efficiency + static_cast<uint32_t>(evt.metric * 100));
                break;

            case ProgressionEventType::PollutionLimitExceeded:
                m_state.resource_pressure = std::min(1.0f, m_state.resource_pressure + evt.metric * 0.1f);
                break;

            case ProgressionEventType::ServiceUptimeReached:
                m_state.city_stability = std::min(1.0f, m_state.city_stability + evt.metric * 0.05f);
                break;

            case ProgressionEventType::EconomicStabilityImproved:
                m_state.economic_level = std::max(m_state.economic_level,
                    static_cast<uint32_t>(evt.metric * 100));
                break;

            case ProgressionEventType::InfrastructureMaintained:
                m_state.infrastructure_level = std::max(m_state.infrastructure_level,
                    static_cast<uint32_t>(evt.value));
                break;

            case ProgressionEventType::TechResearchCompleted:
                m_state.technology_level++;
                m_state.research_progress = 0;
                break;

            case ProgressionEventType::LogisticsOptimized:
                m_state.logistics_level = std::max(m_state.logistics_level,
                    static_cast<uint32_t>(evt.value));
                break;

            case ProgressionEventType::DistrictDensityIncreased:
                m_state.district_expansion = std::max(m_state.district_expansion,
                    static_cast<uint32_t>(evt.value));
                break;

            default:
                // Unknown event type - ignore
                break;
        }
    }

    // Clear event queue for next frame
    m_event_count = 0;
}

void ProgressionSystem::update_progression_levels() noexcept
{
    // Update population level based on total population
    if (m_state.total_population < 1000)
    {
        m_state.population_level = 0;
    }
    else if (m_state.total_population < 5000)
    {
        m_state.population_level = 1;
    }
    else if (m_state.total_population < 25000)
    {
        m_state.population_level = 2;
    }
    else if (m_state.total_population < 100000)
    {
        m_state.population_level = 3;
    }
    else if (m_state.total_population < 250000)
    {
        m_state.population_level = 4;
    }
    else
    {
        m_state.population_level = 5;  // Max level
    }

    // Update economic level based on total income
    if (m_state.total_income < 5000)
    {
        m_state.economic_level = 0;
    }
    else if (m_state.total_income < 25000)
    {
        m_state.economic_level = 1;
    }
    else if (m_state.total_income < 100000)
    {
        m_state.economic_level = 2;
    }
    else if (m_state.total_income < 500000)
    {
        m_state.economic_level = 3;
    }
    else
    {
        m_state.economic_level = 4;  // Max level
    }

    // Update district level based on district expansion and stability
    float district_score = static_cast<float>(m_state.district_expansion) * 
                          (1.0f + m_state.city_stability * 0.5f);
    
    if (district_score < 5)
    {
        m_state.district_level = 0;
    }
    else if (district_score < 20)
    {
        m_state.district_level = 1;
    }
    else if (district_score < 50)
    {
        m_state.district_level = 2;
    }
    else if (district_score < 150)
    {
        m_state.district_level = 3;
    }
    else
    {
        m_state.district_level = 4;  // Max level
    }

    // Normalize stability (prevent overflow)
    m_state.city_stability = std::min(1.0f, m_state.city_stability);
    m_state.city_stability = std::max(0.0f, m_state.city_stability);

    // Normalize resource pressure
    m_state.resource_pressure = std::min(1.0f, m_state.resource_pressure);
    m_state.resource_pressure = std::max(0.0f, m_state.resource_pressure);
}

bool ProgressionSystem::evaluate_unlock_rule(const UnlockRule& rule) noexcept
{
    // Check all requirements
    if (rule.population_required > 0 && m_state.total_population < rule.population_required)
    {
        return false;
    }

    if (rule.income_required > 0 && m_state.total_income < rule.income_required)
    {
        return false;
    }

    if (rule.trade_volume_required > 0 && m_state.total_trade_volume < rule.trade_volume_required)
    {
        return false;
    }

    if (rule.infrastructure_level > 0 && m_state.infrastructure_level < rule.infrastructure_level)
    {
        return false;
    }

    if (rule.stability_required > 0.0f && m_state.city_stability < rule.stability_required)
    {
        return false;
    }

    if (rule.efficiency_required > 0.0f)
    {
        float normalized_efficiency = m_state.logistics_efficiency / 1000.0f;
        if (normalized_efficiency < rule.efficiency_required)
        {
            return false;
        }
    }

    // All requirements met
    return true;
}

void ProgressionSystem::evaluate_unlocks() noexcept
{
    // Evaluate all registered unlock rules
    for (uint32_t i = 0; i < m_unlock_rule_count; ++i)
    {
        const auto& rule = m_unlock_rules[i];
        
        // Skip if already unlocked
        if ((m_state.unlocked_flags & static_cast<uint64_t>(rule.unlock_id)) != 0)
        {
            continue;
        }

        // Check if all requirements are met
        if (evaluate_unlock_rule(rule))
        {
            // Unlock the feature (bitwise OR)
            m_state.unlocked_flags |= static_cast<uint64_t>(rule.unlock_id);
        }
    }
}

bool ProgressionSystem::update() noexcept
{
    if (!m_initialized)
    {
        return false;
    }

    uint64_t unlocks_before = m_state.unlocked_flags;

    // Process all queued events (deterministic order)
    process_event_queue();

    // Update progression levels based on accumulated metrics
    update_progression_levels();

    // Evaluate all unlock rules
    evaluate_unlocks();

    // Increment frame count for determinism validation
    increment_frame_count();

    // Return true if any unlocks were triggered
    return (m_state.unlocked_flags != unlocks_before);
}

bool ProgressionSystem::register_unlock_rule(const UnlockRule& rule) noexcept
{
    if (m_unlock_rule_count >= MAX_UNLOCKS)
    {
        return false;  // No space for more rules
    }

    m_unlock_rules[m_unlock_rule_count] = rule;
    m_unlock_rule_count++;
    
    return true;
}

ProgressionSystem::~ProgressionSystem() noexcept
{
    shutdown();
}

}  // namespace acheron::gameplay::progression

// =============================================================================
// UNIT TEST MAIN (if enabled)
// =============================================================================

#ifdef ACHERON_PROGRESSION_SYSTEM_UNIT_TEST

int main()
{
    using namespace acheron::gameplay::progression::tests;
    
    bool all_passed = run_all_tests();
    
    return all_passed ? 0 : 1;
}

#endif  // ACHERON_PROGRESSION_SYSTEM_UNIT_TEST
