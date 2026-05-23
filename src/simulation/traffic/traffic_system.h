#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>

namespace acheron::simulation::traffic {

/// Traffic density grid cell (4 bytes)
struct DensityCell
{
    float density;  // vehicles per unit area [0..1]
};

/// Lane segment for flow computation (16 bytes)
struct LaneSegment
{
    uint32_t cell_index;     // Into density grid
    uint32_t next_cell;      // Forward link (for flow)
    float desired_flow;      // Vehicles/frame wanting to enter
    float actual_flow;       // Actual flow achieved this frame
};

/// Road network configuration
struct RoadNetworkConfig
{
    uint32_t grid_width;              // Cells in X
    uint32_t grid_height;             // Cells in Y
    uint32_t max_lanes;               // Total lane segments
    float cell_size;                  // Meters per cell
    float max_density;                // Saturated density
    float congestion_wave_speed;      // Backward wave speed
};

/// Traffic system state (one instance per frame)
class TrafficSystem
{
public:
    /// Initialize traffic system
    /// @param config Road network dimensions and behavior
    /// @return true if initialization successful
    bool initialize(const RoadNetworkConfig& config) noexcept;

    /// Shutdown and deallocate traffic data
    void shutdown() noexcept;

    /// Update traffic densities (continuum model)
    /// Called once per simulation frame
    /// @param dt Time step (seconds)
    void update(float dt) noexcept;

    /// Get current density at grid cell
    /// @param x Grid coordinate X [0..width)
    /// @param y Grid coordinate Y [0..height)
    /// @return Vehicle density [0..1]
    float get_density(uint32_t x, uint32_t y) const noexcept;

    /// Set initial density at grid cell
    /// @param x Grid coordinate X
    /// @param y Grid coordinate Y
    /// @param density Vehicle density [0..1]
    void set_density(uint32_t x, uint32_t y, float density) noexcept;

    /// Add traffic flow demand to road network
    /// @param x Grid coordinate X
    /// @param y Grid coordinate Y
    /// @param demand Number of vehicles wanting to flow [0..1]
    void add_flow_demand(uint32_t x, uint32_t y, float demand) noexcept;

    /// Get total congestion metric [0..1]
    /// @return Average congestion across all cells
    float get_congestion_metric() const noexcept;

    /// Get frame update count (for determinism validation)
    uint64_t get_frame_count() const noexcept { return m_frame_count; }

    /// Check if system is initialized
    bool is_initialized() const noexcept { return m_density_grid != nullptr; }

    // Destructor
    ~TrafficSystem() noexcept;

private:
    // --- Core State ---
    DensityCell* m_density_grid = nullptr;
    DensityCell* m_density_scratch = nullptr;  // Temporary buffer for propagation
    LaneSegment* m_lanes = nullptr;

    RoadNetworkConfig m_config = {};
    uint32_t m_grid_size = 0;
    uint32_t m_lane_count = 0;
    float m_total_congestion = 0.0f;
    uint64_t m_frame_count = 0;

    // Private methods for continuum model
    void propagate_density() noexcept;
    void apply_flow_demands() noexcept;
    void compute_congestion_metric() noexcept;
};

}  // namespace acheron::simulation::traffic
