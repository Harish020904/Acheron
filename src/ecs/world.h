#pragma once

#include "component_registry.h"
#include "entity_id.h"

#include <cstdint>

namespace acheron::ecs
{
    enum class DistrictType : std::uint8_t
    {
        Residential,
        Commercial,
        Industrial,
        Empty
    };

    struct DistrictComponent
    {
        EntityID entity{};
        std::uint32_t grid_x = 0;
        std::uint32_t grid_y = 0;
        DistrictType type = DistrictType::Empty;
        std::uint32_t level = 1;
        bool powered = true;
        
        float population = 0.0f;
        float wealth = 0.0f;
        float traffic = 0.0f;
        float stability = 1.0f;
        float instability = 0.0f;
        float pressure = 0.0f;
    };

    struct RoadComponent
    {
        EntityID from{};
        EntityID to{};
        float length = 1.0f;
        float congestion = 0.0f;
    };

    class World final
    {
    public:
        struct Config
        {
            std::uint32_t grid_width = 8;
            std::uint32_t grid_height = 8;
            std::uint32_t max_districts = 64;
            std::uint32_t max_roads = 128;
        };

        World() noexcept = default;
        ~World() noexcept;

        World(const World&) = delete;
        World& operator=(const World&) = delete;

        bool initialize(const Config& config) noexcept;
        void shutdown() noexcept;

        void clear() noexcept;

        EntityID spawn_district(std::uint32_t x, std::uint32_t y) noexcept;
        bool connect_districts(EntityID from, EntityID to, float length = 1.0f) noexcept;

        DistrictComponent* district(EntityID id) noexcept;
        const DistrictComponent* district(EntityID id) const noexcept;
        DistrictComponent* district_at(std::uint32_t x, std::uint32_t y) noexcept;
        const DistrictComponent* district_at(std::uint32_t x, std::uint32_t y) const noexcept;
        DistrictComponent* district_slot(std::uint32_t index) noexcept;
        const DistrictComponent* district_slot(std::uint32_t index) const noexcept;

        RoadComponent* road_slot(std::uint32_t index) noexcept;
        const RoadComponent* road_slot(std::uint32_t index) const noexcept;

        void set_district_state(EntityID id,
                                float population,
                                float wealth,
                                float traffic,
                                float stability,
                                float instability,
                                float pressure) noexcept;

        void update() noexcept { ++m_frame_count; }

        std::uint32_t district_count() const noexcept { return m_district_count; }
        std::uint32_t road_count() const noexcept { return m_road_count; }
        std::uint64_t frame_count() const noexcept { return m_frame_count; }

        std::uint32_t grid_width() const noexcept { return m_config.grid_width; }
        std::uint32_t grid_height() const noexcept { return m_config.grid_height; }

        friend class WorldSerializer;

    private:
        bool valid_cell(std::uint32_t x, std::uint32_t y) const noexcept;
        std::uint32_t cell_index(std::uint32_t x, std::uint32_t y) const noexcept;
        bool road_exists(EntityID from, EntityID to) const noexcept;
        void connect_neighbor(std::uint32_t x, std::uint32_t y, std::int32_t dx, std::int32_t dy, EntityID from) noexcept;

        Config m_config{};
        ComponentRegistry m_registry{};
        EntityRecord* m_records = nullptr;
        std::uint32_t* m_free_indices = nullptr;
        std::uint32_t* m_entity_to_district = nullptr;
        EntityTable m_entities{};
        DistrictComponent* m_districts = nullptr;
        RoadComponent* m_roads = nullptr;
        EntityID* m_cell_entities = nullptr;
        std::uint32_t m_district_count = 0;
        std::uint32_t m_road_count = 0;
        std::uint64_t m_frame_count = 0;
    };
}
