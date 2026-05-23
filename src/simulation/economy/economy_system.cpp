#include "economy_system.h"

#include <cmath>
#include <cstring>

namespace acheron::simulation::economy {

bool EconomySystem::initialize(const EconomyConfig& config) noexcept
{
    if (config.max_districts == 0 || config.max_trade_routes == 0)
    {
        return false;
    }

    if (config.trade_efficiency < 0.0f || config.trade_efficiency > 1.0f)
    {
        return false;
    }

    m_config = config;
    m_district_count = 0;
    m_trade_route_count = 0;

    // Allocate economy data
    m_districts = new DistrictEconomy[config.max_districts];
    m_active_districts = new uint32_t[config.max_districts];
    m_trade_routes = new TradeRoute[config.max_trade_routes];

    if (!m_districts || !m_active_districts || !m_trade_routes)
    {
        shutdown();
        return false;
    }

    // Initialize all districts to zero
    std::memset(m_districts, 0, config.max_districts * sizeof(DistrictEconomy));
    std::memset(m_active_districts, 0, config.max_districts * sizeof(uint32_t));
    std::memset(m_trade_routes, 0, config.max_trade_routes * sizeof(TradeRoute));

    m_total_wealth = 0.0f;
    m_average_unemployment = 0.0f;
    m_frame_count = 0;

    return true;
}

void EconomySystem::shutdown() noexcept
{
    delete[] m_districts;
    delete[] m_active_districts;
    delete[] m_trade_routes;

    m_districts = nullptr;
    m_active_districts = nullptr;
    m_trade_routes = nullptr;

    m_district_count = 0;
    m_trade_route_count = 0;
}

void EconomySystem::update(float dt) noexcept
{
    if (!is_initialized() || dt <= 0.0f)
    {
        return;
    }

    // Step 1: Generate resources through production
    propagate_resources();

    // Step 2: Execute trade routes
    process_trade_routes();

    // Step 3: Compute economic metrics
    compute_economic_metrics();

    m_frame_count++;
}

void EconomySystem::propagate_resources() noexcept
{
    // Each district produces resources based on efficiency
    for (uint32_t i = 0; i < m_district_count; ++i)
    {
        uint32_t district_idx = m_active_districts[i];
        if (district_idx >= m_config.max_districts)
        {
            continue;
        }

        DistrictEconomy& economy = m_districts[district_idx];

        // Production varies by resource type (simplified model)
        // Food production is highest, luxury is lowest
        float production_rates[] = {0.3f, 0.2f, 0.15f, 0.05f, 0.25f, 0.1f, 0.08f, 0.12f};

        for (uint8_t res = 0; res < static_cast<uint8_t>(ResourceType::Count); ++res)
        {
            float production = production_rates[res] * economy.production_efficiency * m_config.base_production_rate;
            economy.resources[res].amount += production;

            // Decay resources (perishable goods, capital depreciation)
            economy.resources[res].amount *= (1.0f - m_config.resource_decay_rate);

            // Clamp to valid range
            economy.resources[res].amount = std::min(std::max(economy.resources[res].amount, 0.0f), 1000.0f);
        }

        // Unemployment increases when resources are low
        float avg_resources = 0.0f;
        for (uint8_t res = 0; res < static_cast<uint8_t>(ResourceType::Count); ++res)
        {
            avg_resources += economy.resources[res].amount;
        }
        avg_resources /= static_cast<float>(ResourceType::Count);

        // Unemployment inversely proportional to average resources
        float target_unemployment = std::max(0.0f, std::min(1.0f, 1.0f - avg_resources / 500.0f));
        economy.unemployment = economy.unemployment * 0.9f + target_unemployment * 0.1f;
    }
}

void EconomySystem::process_trade_routes() noexcept
{
    // Execute deterministic trade in order of route priority
    for (uint32_t i = 0; i < m_trade_route_count; ++i)
    {
        TradeRoute& route = m_trade_routes[i];
        int32_t source_idx = find_district(route.source_district);
        int32_t dest_idx = find_district(route.dest_district);

        if (source_idx < 0 || dest_idx < 0)
        {
            continue;
        }

        DistrictEconomy& source = m_districts[source_idx];
        DistrictEconomy& dest = m_districts[dest_idx];

        uint8_t resource_idx = route.resource_type;
        if (resource_idx >= static_cast<uint8_t>(ResourceType::Count))
        {
            continue;
        }

        // Transfer resources: source exports, destination imports
        float available = source.resources[resource_idx].amount;
        float transfer_amount = std::min(available * route.monthly_flow * 0.01f, available);

        // Apply trade efficiency (some loss in transport)
        float actual_transfer = transfer_amount * m_config.trade_efficiency;

        source.resources[resource_idx].amount -= transfer_amount;
        dest.resources[resource_idx].amount += actual_transfer;

        // Update trade flow record
        route.monthly_flow = std::min(route.monthly_flow * 1.05f, 1.0f);  // Slightly increase profitable routes
    }
}

void EconomySystem::compute_economic_metrics() noexcept
{
    m_total_wealth = 0.0f;
    float total_unemployment = 0.0f;

    for (uint32_t i = 0; i < m_district_count; ++i)
    {
        uint32_t district_idx = m_active_districts[i];
        if (district_idx >= m_config.max_districts)
        {
            continue;
        }

        DistrictEconomy& economy = m_districts[district_idx];

        // Wealth = sum of all resources weighted by value
        float district_wealth = 0.0f;
        float resource_weights[] = {1.0f, 1.5f, 1.2f, 2.0f, 0.8f, 1.3f, 2.5f, 0.9f};

        for (uint8_t res = 0; res < static_cast<uint8_t>(ResourceType::Count); ++res)
        {
            district_wealth += economy.resources[res].amount * resource_weights[res];
        }

        economy.wealth = district_wealth;
        m_total_wealth += district_wealth;
        total_unemployment += economy.unemployment;
    }

    // Average unemployment
    if (m_district_count > 0)
    {
        m_average_unemployment = total_unemployment / static_cast<float>(m_district_count);
    }
    else
    {
        m_average_unemployment = 0.0f;
    }
}

bool EconomySystem::add_district(uint32_t district_id) noexcept
{
    if (m_district_count >= m_config.max_districts)
    {
        return false;
    }

    if (find_district(district_id) >= 0)
    {
        return false;  // Already exists
    }

    m_active_districts[m_district_count] = district_id;
    m_districts[district_id].district_id = district_id;

    // Initialize with some base resources
    for (uint8_t res = 0; res < static_cast<uint8_t>(ResourceType::Count); ++res)
    {
        m_districts[district_id].resources[res].amount = 100.0f;
    }

    m_districts[district_id].production_efficiency = 0.7f;
    m_districts[district_id].trade_openness = 0.8f;
    m_districts[district_id].unemployment = 0.1f;

    m_district_count++;
    return true;
}

void EconomySystem::remove_district(uint32_t district_id) noexcept
{
    int32_t idx = find_district(district_id);
    if (idx < 0)
    {
        return;
    }

    // Swap with last and reduce count
    uint32_t last_idx = m_active_districts[m_district_count - 1];
    m_active_districts[idx] = last_idx;
    m_district_count--;

    // Clear district data
    std::memset(&m_districts[district_id], 0, sizeof(DistrictEconomy));
}

void EconomySystem::set_resource(uint32_t district_id, ResourceType resource_type, float amount) noexcept
{
    if (district_id >= m_config.max_districts || !is_initialized())
    {
        return;
    }

    amount = std::min(std::max(amount, 0.0f), 1000.0f);
    uint8_t res_idx = static_cast<uint8_t>(resource_type);

    if (res_idx >= static_cast<uint8_t>(ResourceType::Count))
    {
        return;
    }

    m_districts[district_id].resources[res_idx].amount = amount;
}

float EconomySystem::get_resource(uint32_t district_id, ResourceType resource_type) const noexcept
{
    if (district_id >= m_config.max_districts || !is_initialized())
    {
        return 0.0f;
    }

    uint8_t res_idx = static_cast<uint8_t>(resource_type);
    if (res_idx >= static_cast<uint8_t>(ResourceType::Count))
    {
        return 0.0f;
    }

    return m_districts[district_id].resources[res_idx].amount;
}

bool EconomySystem::add_trade_route(uint32_t source_id, uint32_t dest_id, float monthly_flow,
                                     ResourceType resource_type) noexcept
{
    if (m_trade_route_count >= m_config.max_trade_routes)
    {
        return false;
    }

    if (find_district(source_id) < 0 || find_district(dest_id) < 0)
    {
        return false;
    }

    TradeRoute& route = m_trade_routes[m_trade_route_count];
    route.source_district = source_id;
    route.dest_district = dest_id;
    route.monthly_flow = std::min(std::max(monthly_flow, 0.0f), 1.0f);
    route.resource_type = static_cast<uint8_t>(resource_type);
    route.trade_priority = 128;  // Default medium priority

    m_trade_route_count++;
    return true;
}

float EconomySystem::get_total_wealth() const noexcept
{
    return m_total_wealth;
}

float EconomySystem::get_average_unemployment() const noexcept
{
    return m_average_unemployment;
}

int32_t EconomySystem::find_district(uint32_t district_id) const noexcept
{
    for (uint32_t i = 0; i < m_district_count; ++i)
    {
        if (m_active_districts[i] == district_id)
        {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

EconomySystem::~EconomySystem() noexcept
{
    shutdown();
}

}  // namespace acheron::simulation::economy

// ============================================================================
// UNIT TESTS
// ============================================================================

#ifdef ACHERON_ECONOMY_SYSTEM_UNIT_TEST

#include <cstdio>
#include <cmath>

using namespace acheron::simulation::economy;

void test_initialization()
{
    EconomySystem economy;
    EconomyConfig config{
        .max_districts = 32,
        .max_trade_routes = 128,
        .base_production_rate = 10.0f,
        .trade_efficiency = 0.9f,
        .resource_decay_rate = 0.01f,
    };

    if (!economy.initialize(config))
    {
        printf("[economy] initialization: FAIL (init returned false)\n");
        return;
    }

    if (!economy.is_initialized())
    {
        printf("[economy] initialization: FAIL (not marked initialized)\n");
        return;
    }

    if (economy.get_frame_count() != 0)
    {
        printf("[economy] initialization: FAIL (frame count not zero)\n");
        return;
    }

    if (economy.get_total_wealth() != 0.0f)
    {
        printf("[economy] initialization: FAIL (wealth not zero)\n");
        return;
    }

    printf("[economy] initialization: OK\n");
}

void test_resource_management()
{
    EconomySystem economy;
    EconomyConfig config{
        .max_districts = 32,
        .max_trade_routes = 128,
        .base_production_rate = 10.0f,
        .trade_efficiency = 0.9f,
        .resource_decay_rate = 0.01f,
    };

    economy.initialize(config);
    economy.add_district(0);

    // Set resource
    economy.set_resource(0, ResourceType::Food, 100.0f);
    float retrieved = economy.get_resource(0, ResourceType::Food);

    if (std::abs(retrieved - 100.0f) > 0.01f)
    {
        printf("[economy] resource_management: FAIL (resource mismatch: %.1f vs 100.0)\n", retrieved);
        return;
    }

    // Test clamping
    economy.set_resource(0, ResourceType::Energy, 2000.0f);
    retrieved = economy.get_resource(0, ResourceType::Energy);

    if (retrieved != 1000.0f)
    {
        printf("[economy] resource_management: FAIL (clamping failed: %.1f)\n", retrieved);
        return;
    }

    printf("[economy] resource_management: OK\n");
}

void test_production()
{
    EconomySystem economy;
    EconomyConfig config{
        .max_districts = 32,
        .max_trade_routes = 128,
        .base_production_rate = 10.0f,
        .trade_efficiency = 0.9f,
        .resource_decay_rate = 0.0f,  // No decay for this test
    };

    economy.initialize(config);
    economy.add_district(0);

    float before = economy.get_resource(0, ResourceType::Food);
    economy.update(0.016f);
    float after = economy.get_resource(0, ResourceType::Food);

    if (after <= before)
    {
        printf("[economy] production: FAIL (resources not increased)\n");
        return;
    }

    printf("[economy] production: OK\n");
}

void test_trade_routes()
{
    EconomySystem economy;
    EconomyConfig config{
        .max_districts = 32,
        .max_trade_routes = 128,
        .base_production_rate = 10.0f,
        .trade_efficiency = 0.9f,
        .resource_decay_rate = 0.0f,
    };

    economy.initialize(config);
    economy.add_district(0);
    economy.add_district(1);

    // Set up trade
    economy.set_resource(0, ResourceType::Food, 500.0f);
    economy.set_resource(1, ResourceType::Food, 100.0f);

    economy.add_trade_route(0, 1, 0.5f, ResourceType::Food);

    float source_before = economy.get_resource(0, ResourceType::Food);
    float dest_before = economy.get_resource(1, ResourceType::Food);

    economy.update(0.016f);

    float source_after = economy.get_resource(0, ResourceType::Food);
    float dest_after = economy.get_resource(1, ResourceType::Food);

    if (source_after >= source_before)
    {
        printf("[economy] trade_routes: FAIL (source not reduced)\n");
        return;
    }

    if (dest_after <= dest_before)
    {
        printf("[economy] trade_routes: FAIL (destination not increased)\n");
        return;
    }

    printf("[economy] trade_routes: OK\n");
}

void test_wealth_computation()
{
    EconomySystem economy;
    EconomyConfig config{
        .max_districts = 32,
        .max_trade_routes = 128,
        .base_production_rate = 10.0f,
        .trade_efficiency = 0.9f,
        .resource_decay_rate = 0.0f,
    };

    economy.initialize(config);
    economy.add_district(0);

    // Set some resources
    economy.set_resource(0, ResourceType::Food, 100.0f);
    economy.set_resource(0, ResourceType::Luxury, 50.0f);

    economy.update(0.016f);

    float wealth = economy.get_total_wealth();
    if (wealth <= 0.0f)
    {
        printf("[economy] wealth_computation: FAIL (wealth not computed)\n");
        return;
    }

    printf("[economy] wealth_computation: OK\n");
}

void test_deterministic_updates()
{
    // Initialize two identical economies
    EconomySystem economy1, economy2;
    EconomyConfig config{
        .max_districts = 32,
        .max_trade_routes = 128,
        .base_production_rate = 10.0f,
        .trade_efficiency = 0.9f,
        .resource_decay_rate = 0.01f,
    };

    economy1.initialize(config);
    economy2.initialize(config);

    // Set identical initial state
    economy1.add_district(0);
    economy1.add_district(1);
    economy2.add_district(0);
    economy2.add_district(1);

    economy1.set_resource(0, ResourceType::Food, 200.0f);
    economy2.set_resource(0, ResourceType::Food, 200.0f);

    economy1.add_trade_route(0, 1, 0.5f, ResourceType::Food);
    economy2.add_trade_route(0, 1, 0.5f, ResourceType::Food);

    // Run identical updates
    for (int i = 0; i < 10; ++i)
    {
        economy1.update(0.016f);
        economy2.update(0.016f);
    }

    // Compare final states
    for (uint32_t dist = 0; dist < 2; ++dist)
    {
        for (uint8_t res = 0; res < 8; ++res)
        {
            float r1 = economy1.get_resource(dist, static_cast<ResourceType>(res));
            float r2 = economy2.get_resource(dist, static_cast<ResourceType>(res));

            if (std::abs(r1 - r2) > 0.01f)
            {
                printf("[economy] deterministic_updates: FAIL (mismatch at district %u res %u: %.6f vs %.6f)\n",
                       dist, res, r1, r2);
                return;
            }
        }
    }

    if (economy1.get_frame_count() != economy2.get_frame_count())
    {
        printf("[economy] deterministic_updates: FAIL (frame counts differ)\n");
        return;
    }

    printf("[economy] deterministic_updates: OK\n");
}

void test_large_scale_stress()
{
    EconomySystem economy;
    EconomyConfig config{
        .max_districts = 256,
        .max_trade_routes = 2048,
        .base_production_rate = 10.0f,
        .trade_efficiency = 0.85f,
        .resource_decay_rate = 0.02f,
    };

    if (!economy.initialize(config))
    {
        printf("[economy] large_scale_stress: FAIL (init failed)\n");
        return;
    }

    // Add many districts
    for (uint32_t i = 0; i < 200; ++i)
    {
        economy.add_district(i);
    }

    // Add many trade routes
    for (uint32_t i = 0; i < 1000; ++i)
    {
        uint32_t src = i % 200;
        uint32_t dst = (i + 1 + (i / 200)) % 200;
        ResourceType res = static_cast<ResourceType>(i % 8);
        economy.add_trade_route(src, dst, 0.3f, res);
    }

    // Run many update cycles
    for (int i = 0; i < 100; ++i)
    {
        economy.update(0.016f);
    }

    if (economy.get_frame_count() != 100)
    {
        printf("[economy] large_scale_stress: FAIL (frame count mismatch)\n");
        return;
    }

    float wealth = economy.get_total_wealth();
    if (wealth < 0.0f)
    {
        printf("[economy] large_scale_stress: FAIL (negative wealth)\n");
        return;
    }

    float unemployment = economy.get_average_unemployment();
    if (unemployment < 0.0f || unemployment > 1.0f)
    {
        printf("[economy] large_scale_stress: FAIL (unemployment out of range: %.3f)\n", unemployment);
        return;
    }

    printf("[economy] large_scale_stress: OK\n");
}

int main()
{
    test_initialization();
    test_resource_management();
    test_production();
    test_trade_routes();
    test_wealth_computation();
    test_deterministic_updates();
    test_large_scale_stress();

    return 0;
}

#endif  // ACHERON_ECONOMY_SYSTEM_UNIT_TEST
