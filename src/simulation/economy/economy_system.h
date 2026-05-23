#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>

namespace acheron::simulation::economy {

/// Resource type enumeration (8 resources: food, energy, minerals, luxury, labor, capital, tech, water)
enum class ResourceType : uint8_t
{
    Food = 0,
    Energy = 1,
    Minerals = 2,
    Luxury = 3,
    Labor = 4,
    Capital = 5,
    Tech = 6,
    Water = 7,
    Count = 8
};

/// Resource quantity (4 bytes per resource type)
struct ResourceQuantity
{
    float amount;  // Units of resource [0..1000]
};

/// District economic state (64 bytes)
struct DistrictEconomy
{
    uint32_t district_id;
    ResourceQuantity resources[8];  // SoA-friendly: one array per resource
    float population_demand;         // Workers demanding resources
    float production_efficiency;     // [0..1]
    float trade_openness;            // [0..1] (willingness to trade)
    float wealth;                    // Accumulated capital
    float unemployment;              // [0..1]
    uint32_t _padding;               // Alignment
};

/// Trade route (connection between two districts)
struct TradeRoute
{
    uint32_t source_district;
    uint32_t dest_district;
    float monthly_flow;              // Units/month willing to trade
    uint8_t resource_type;           // Primary resource traded
    uint8_t trade_priority;          // [0..255] Higher = more important
    uint16_t _padding;
};

/// Economy system configuration
struct EconomyConfig
{
    uint32_t max_districts;          // Max concurrent districts
    uint32_t max_trade_routes;       // Max trade connections
    float base_production_rate;      // Resources/frame per district
    float trade_efficiency;          // How much resource is retained in trade [0..1]
    float resource_decay_rate;       // Per-frame resource degradation
};

/// Economy system state (one instance per frame)
class EconomySystem
{
public:
    /// Initialize economy system
    /// @param config Economic parameters
    /// @return true if initialization successful
    bool initialize(const EconomyConfig& config) noexcept;

    /// Shutdown and deallocate economy data
    void shutdown() noexcept;

    /// Update economy (resource propagation, trade)
    /// Called once per simulation frame
    /// @param dt Time step (seconds)
    void update(float dt) noexcept;

    /// Add district to economy
    /// @param district_id Unique identifier
    /// @return true if added successfully
    bool add_district(uint32_t district_id) noexcept;

    /// Remove district from economy
    /// @param district_id District to remove
    void remove_district(uint32_t district_id) noexcept;

    /// Set resource quantity in district
    /// @param district_id Target district
    /// @param resource_type Resource to set
    /// @param amount Quantity [0..1000]
    void set_resource(uint32_t district_id, ResourceType resource_type, float amount) noexcept;

    /// Get resource quantity in district
    /// @param district_id Source district
    /// @param resource_type Resource to query
    /// @return Current quantity [0..1000]
    float get_resource(uint32_t district_id, ResourceType resource_type) const noexcept;

    /// Add trade route between districts
    /// @param source_id Origin district
    /// @param dest_id Destination district
    /// @param monthly_flow Trade volume
    /// @param resource_type Primary resource
    /// @return true if route added successfully
    bool add_trade_route(uint32_t source_id, uint32_t dest_id, float monthly_flow,
                         ResourceType resource_type) noexcept;

    /// Get total wealth in economy
    /// @return Sum of all district wealth
    float get_total_wealth() const noexcept;

    /// Get average unemployment
    /// @return Mean unemployment [0..1]
    float get_average_unemployment() const noexcept;

    /// Get frame update count (for determinism validation)
    uint64_t get_frame_count() const noexcept { return m_frame_count; }

    /// Check if system is initialized
    bool is_initialized() const noexcept { return m_districts != nullptr; }

    // Destructor
    ~EconomySystem() noexcept;

private:
    // --- Core State ---
    DistrictEconomy* m_districts = nullptr;
    uint32_t* m_active_districts = nullptr;  // List of active district IDs
    TradeRoute* m_trade_routes = nullptr;

    EconomyConfig m_config = {};
    uint32_t m_district_count = 0;
    uint32_t m_trade_route_count = 0;
    float m_total_wealth = 0.0f;
    float m_average_unemployment = 0.0f;
    uint64_t m_frame_count = 0;

    // Private methods for economic simulation
    void propagate_resources() noexcept;
    void process_trade_routes() noexcept;
    void compute_economic_metrics() noexcept;
    int32_t find_district(uint32_t district_id) const noexcept;
};

}  // namespace acheron::simulation::economy
