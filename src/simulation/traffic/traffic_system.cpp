#include "traffic_system.h"

#include <cmath>
#include <cstring>

namespace acheron::simulation::traffic {

bool TrafficSystem::initialize(const RoadNetworkConfig& config) noexcept
{
    if (config.grid_width == 0 || config.grid_height == 0 || config.max_lanes == 0)
    {
        return false;
    }

    if (config.max_density <= 0.0f)
    {
        return false;
    }

    m_config = config;
    m_grid_size = config.grid_width * config.grid_height;
    m_lane_count = 0;

    // Allocate density grids
    m_density_grid = new DensityCell[m_grid_size];
    m_density_scratch = new DensityCell[m_grid_size];
    m_lanes = new LaneSegment[config.max_lanes];

    if (!m_density_grid || !m_density_scratch || !m_lanes)
    {
        shutdown();
        return false;
    }

    // Initialize all densities to zero
    std::memset(m_density_grid, 0, m_grid_size * sizeof(DensityCell));
    std::memset(m_density_scratch, 0, m_grid_size * sizeof(DensityCell));
    std::memset(m_lanes, 0, config.max_lanes * sizeof(LaneSegment));

    m_total_congestion = 0.0f;
    m_frame_count = 0;

    return true;
}

void TrafficSystem::shutdown() noexcept
{
    delete[] m_density_grid;
    delete[] m_density_scratch;
    delete[] m_lanes;

    m_density_grid = nullptr;
    m_density_scratch = nullptr;
    m_lanes = nullptr;

    m_grid_size = 0;
    m_lane_count = 0;
}

void TrafficSystem::update(float dt) noexcept
{
    if (!is_initialized() || dt <= 0.0f)
    {
        return;
    }

    // Step 1: Apply incoming flow demands
    apply_flow_demands();

    // Step 2: Propagate congestion waves (continuum model)
    propagate_density();

    // Step 3: Update congestion metric
    compute_congestion_metric();

    m_frame_count++;
}

void TrafficSystem::propagate_density() noexcept
{
    // Copy current grid to scratch for update
    std::memcpy(m_density_scratch, m_density_grid, m_grid_size * sizeof(DensityCell));

    // Continuum traffic propagation (simple diffusion with directional bias)
    // Each cell influences its neighbors based on local density pressure
    for (uint32_t y = 0; y < m_config.grid_height; ++y)
    {
        for (uint32_t x = 0; x < m_config.grid_width; ++x)
        {
            uint32_t idx = y * m_config.grid_width + x;
            float current_density = m_density_grid[idx].density;

            // Clamp density to valid range
            current_density = std::min(current_density, 1.0f);
            current_density = std::max(current_density, 0.0f);

            // Forward propagation (vehicles flow ahead when not congested)
            if (x + 1 < m_config.grid_width)
            {
                uint32_t next_idx = y * m_config.grid_width + (x + 1);

                // Flow rate decreases with congestion
                float flow_rate = (1.0f - current_density) * 0.2f;
                float transfer = std::min(current_density * flow_rate, 0.1f);

                m_density_scratch[idx].density -= transfer;
                m_density_scratch[next_idx].density += transfer;
            }

            // Lateral diffusion (vehicles distribute into adjacent lanes)
            if (x > 0)
            {
                uint32_t prev_idx = y * m_config.grid_width + (x - 1);
                float prev_density = m_density_grid[prev_idx].density;
                float diff = (prev_density - current_density) * 0.05f;
                m_density_scratch[idx].density += diff;
                m_density_scratch[prev_idx].density -= diff;
            }

            if (y > 0)
            {
                uint32_t up_idx = (y - 1) * m_config.grid_width + x;
                float up_density = m_density_grid[up_idx].density;
                float diff = (up_density - current_density) * 0.05f;
                m_density_scratch[idx].density += diff;
                m_density_scratch[up_idx].density -= diff;
            }
        }
    }

    // Swap grids
    std::swap(m_density_grid, m_density_scratch);

    // Final clamping to valid range
    for (uint32_t i = 0; i < m_grid_size; ++i)
    {
        m_density_grid[i].density = std::min(std::max(m_density_grid[i].density, 0.0f), 1.0f);
    }
}

void TrafficSystem::apply_flow_demands() noexcept
{
    // Apply lane-based flow demands to density grid
    for (uint32_t i = 0; i < m_lane_count; ++i)
    {
        const LaneSegment& lane = m_lanes[i];
        if (lane.cell_index >= m_grid_size)
        {
            continue;
        }

        // Increase density based on desired flow (clamped by saturation)
        float max_increase = m_config.max_density - m_density_grid[lane.cell_index].density;
        float actual_increase = std::min(lane.desired_flow * 0.05f, max_increase);

        m_density_grid[lane.cell_index].density += actual_increase;
    }

    // Clear flow demands for next frame
    for (uint32_t i = 0; i < m_lane_count; ++i)
    {
        m_lanes[i].desired_flow = 0.0f;
        m_lanes[i].actual_flow = 0.0f;
    }
}

void TrafficSystem::compute_congestion_metric() noexcept
{
    float total = 0.0f;

    for (uint32_t i = 0; i < m_grid_size; ++i)
    {
        total += m_density_grid[i].density;
    }

    m_total_congestion = total / static_cast<float>(m_grid_size);
}

float TrafficSystem::get_density(uint32_t x, uint32_t y) const noexcept
{
    if (x >= m_config.grid_width || y >= m_config.grid_height || !is_initialized())
    {
        return 0.0f;
    }

    uint32_t idx = y * m_config.grid_width + x;
    return m_density_grid[idx].density;
}

void TrafficSystem::set_density(uint32_t x, uint32_t y, float density) noexcept
{
    if (x >= m_config.grid_width || y >= m_config.grid_height || !is_initialized())
    {
        return;
    }

    density = std::min(std::max(density, 0.0f), 1.0f);
    uint32_t idx = y * m_config.grid_width + x;
    m_density_grid[idx].density = density;
}

void TrafficSystem::add_flow_demand(uint32_t x, uint32_t y, float demand) noexcept
{
    if (x >= m_config.grid_width || y >= m_config.grid_height || !is_initialized())
    {
        return;
    }

    if (m_lane_count >= m_config.max_lanes)
    {
        return;
    }

    uint32_t cell_index = y * m_config.grid_width + x;
    uint32_t next_cell = std::min(cell_index + 1, m_grid_size - 1);

    m_lanes[m_lane_count].cell_index = cell_index;
    m_lanes[m_lane_count].next_cell = next_cell;
    m_lanes[m_lane_count].desired_flow = std::min(std::max(demand, 0.0f), 1.0f);
    m_lanes[m_lane_count].actual_flow = 0.0f;

    m_lane_count++;
}

float TrafficSystem::get_congestion_metric() const noexcept
{
    return m_total_congestion;
}

TrafficSystem::~TrafficSystem() noexcept
{
    shutdown();
}

}  // namespace acheron::simulation::traffic

// ============================================================================
// UNIT TESTS
// ============================================================================

#ifdef ACHERON_TRAFFIC_SYSTEM_UNIT_TEST

#include <cstdio>
#include <cmath>

using namespace acheron::simulation::traffic;

void test_initialization()
{
    TrafficSystem traffic;
    RoadNetworkConfig config{
        .grid_width = 16,
        .grid_height = 16,
        .max_lanes = 256,
        .cell_size = 10.0f,
        .max_density = 1.0f,
        .congestion_wave_speed = 0.5f,
    };

    if (!traffic.initialize(config))
    {
        printf("[traffic] initialization: FAIL (init returned false)\n");
        return;
    }

    if (!traffic.is_initialized())
    {
        printf("[traffic] initialization: FAIL (not marked initialized)\n");
        return;
    }

    if (traffic.get_frame_count() != 0)
    {
        printf("[traffic] initialization: FAIL (frame count not zero)\n");
        return;
    }

    if (std::abs(traffic.get_congestion_metric()) > 0.01f)
    {
        printf("[traffic] initialization: FAIL (congestion not zero)\n");
        return;
    }

    printf("[traffic] initialization: OK\n");
}

void test_density_propagation()
{
    TrafficSystem traffic;
    RoadNetworkConfig config{
        .grid_width = 8,
        .grid_height = 8,
        .max_lanes = 64,
        .cell_size = 10.0f,
        .max_density = 1.0f,
        .congestion_wave_speed = 0.5f,
    };

    traffic.initialize(config);

    // Set high density in one cell
    traffic.set_density(3, 3, 0.9f);

    float before = traffic.get_density(3, 3);
    traffic.update(0.016f);  // 60 Hz frame
    float after_center = traffic.get_density(3, 3);

    // Neighbors should increase
    float neighbor_density = traffic.get_density(4, 3);

    if (after_center >= before)
    {
        printf("[traffic] density_propagation: FAIL (center density increased)\n");
        return;
    }

    if (neighbor_density <= 0.01f)
    {
        printf("[traffic] density_propagation: FAIL (neighbor not increased)\n");
        return;
    }

    printf("[traffic] density_propagation: OK\n");
}

void test_flow_demands()
{
    TrafficSystem traffic;
    RoadNetworkConfig config{
        .grid_width = 8,
        .grid_height = 8,
        .max_lanes = 64,
        .cell_size = 10.0f,
        .max_density = 1.0f,
        .congestion_wave_speed = 0.5f,
    };

    traffic.initialize(config);

    // Add flow demand
    traffic.add_flow_demand(0, 0, 0.5f);

    float before = traffic.get_density(0, 0);
    traffic.update(0.016f);
    float after = traffic.get_density(0, 0);

    if (after <= before)
    {
        printf("[traffic] flow_demands: FAIL (density not increased)\n");
        return;
    }

    printf("[traffic] flow_demands: OK\n");
}

void test_congestion_metric()
{
    TrafficSystem traffic;
    RoadNetworkConfig config{
        .grid_width = 4,
        .grid_height = 4,
        .max_lanes = 16,
        .cell_size = 10.0f,
        .max_density = 1.0f,
        .congestion_wave_speed = 0.5f,
    };

    traffic.initialize(config);

    if (traffic.get_congestion_metric() > 0.01f)
    {
        printf("[traffic] congestion_metric: FAIL (initial not zero)\n");
        return;
    }

    // Add some density
    for (uint32_t x = 0; x < 4; ++x)
    {
        for (uint32_t y = 0; y < 4; ++y)
        {
            traffic.set_density(x, y, 0.5f);
        }
    }

    traffic.update(0.016f);
    float metric = traffic.get_congestion_metric();

    if (metric < 0.4f || metric > 0.6f)
    {
        printf("[traffic] congestion_metric: FAIL (metric out of range: %.3f)\n", metric);
        return;
    }

    printf("[traffic] congestion_metric: OK\n");
}

void test_deterministic_updates()
{
    // Initialize two identical traffic systems
    TrafficSystem traffic1, traffic2;
    RoadNetworkConfig config{
        .grid_width = 8,
        .grid_height = 8,
        .max_lanes = 64,
        .cell_size = 10.0f,
        .max_density = 1.0f,
        .congestion_wave_speed = 0.5f,
    };

    traffic1.initialize(config);
    traffic2.initialize(config);

    // Set identical initial state
    traffic1.set_density(3, 3, 0.7f);
    traffic2.set_density(3, 3, 0.7f);

    // Run identical updates
    for (int i = 0; i < 10; ++i)
    {
        traffic1.update(0.016f);
        traffic2.update(0.016f);
    }

    // Compare final states (sample grid cells)
    for (uint32_t x = 0; x < 8; ++x)
    {
        for (uint32_t y = 0; y < 8; ++y)
        {
            float d1 = traffic1.get_density(x, y);
            float d2 = traffic2.get_density(x, y);

            if (std::abs(d1 - d2) > 0.001f)
            {
                printf("[traffic] deterministic_updates: FAIL (mismatch at %u,%u: %.6f vs %.6f)\n",
                       x, y, d1, d2);
                return;
            }
        }
    }

    if (traffic1.get_frame_count() != traffic2.get_frame_count())
    {
        printf("[traffic] deterministic_updates: FAIL (frame counts differ)\n");
        return;
    }

    printf("[traffic] deterministic_updates: OK\n");
}

void test_large_scale_stress()
{
    TrafficSystem traffic;
    RoadNetworkConfig config{
        .grid_width = 128,
        .grid_height = 128,
        .max_lanes = 4096,
        .cell_size = 10.0f,
        .max_density = 1.0f,
        .congestion_wave_speed = 0.5f,
    };

    if (!traffic.initialize(config))
    {
        printf("[traffic] large_scale_stress: FAIL (init failed)\n");
        return;
    }

    // Add many flow demands
    for (uint32_t i = 0; i < 1000; ++i)
    {
        uint32_t x = (i * 7) % 128;
        uint32_t y = (i * 11) % 128;
        float demand = 0.1f + 0.4f * static_cast<float>((i % 5)) / 5.0f;
        traffic.add_flow_demand(x, y, demand);
    }

    // Run many update cycles
    for (int i = 0; i < 100; ++i)
    {
        traffic.update(0.016f);
    }

    if (traffic.get_frame_count() != 100)
    {
        printf("[traffic] large_scale_stress: FAIL (frame count mismatch)\n");
        return;
    }

    float congestion = traffic.get_congestion_metric();
    if (congestion < 0.0f || congestion > 1.0f)
    {
        printf("[traffic] large_scale_stress: FAIL (congestion out of range: %.3f)\n", congestion);
        return;
    }

    printf("[traffic] large_scale_stress: OK\n");
}

int main()
{
    test_initialization();
    test_density_propagation();
    test_flow_demands();
    test_congestion_metric();
    test_deterministic_updates();
    test_large_scale_stress();

    return 0;
}

#endif  // ACHERON_TRAFFIC_SYSTEM_UNIT_TEST
