#include "world.h"

#include <algorithm>
#include <cstring>

namespace acheron::ecs
{
    namespace
    {
        constexpr ComponentTypeID DISTRICT_COMPONENT_ID = 0;
        constexpr ComponentTypeID ROAD_COMPONENT_ID = 1;
    }

    World::~World() noexcept
    {
        shutdown();
    }

    bool World::initialize(const Config& config) noexcept
    {
        if (config.grid_width == 0 || config.grid_height == 0 || config.max_districts == 0 || config.max_roads == 0)
        {
            return false;
        }

        shutdown();

        m_config = config;
        const std::uint32_t cell_count = config.grid_width * config.grid_height;

        m_records = new EntityRecord[config.max_districts];
        m_free_indices = new std::uint32_t[config.max_districts];
        m_entity_to_district = new std::uint32_t[config.max_districts];
        m_districts = new DistrictComponent[config.max_districts];
        m_roads = new RoadComponent[config.max_roads];
        m_cell_entities = new EntityID[cell_count];

        if (!m_records || !m_free_indices || !m_entity_to_district || !m_districts || !m_roads || !m_cell_entities)
        {
            shutdown();
            return false;
        }

        std::memset(m_entity_to_district, 0xFF, sizeof(std::uint32_t) * config.max_districts);
        for (std::uint32_t i = 0; i < cell_count; ++i)
        {
            m_cell_entities[i] = {};
        }
        for (std::uint32_t i = 0; i < config.max_districts; ++i)
        {
            m_districts[i] = {};
        }
        for (std::uint32_t i = 0; i < config.max_roads; ++i)
        {
            m_roads[i] = {};
        }

        if (!m_entities.init(m_records, m_free_indices, config.max_districts))
        {
            shutdown();
            return false;
        }

        m_registry.reset();
        m_registry.register_descriptor({
            DISTRICT_COMPONENT_ID,
            static_cast<std::uint16_t>(sizeof(DistrictComponent)),
            static_cast<std::uint16_t>(alignof(DistrictComponent)),
            "DistrictComponent",
            ComponentRegistry::stable_name_hash("DistrictComponent"),
        });
        m_registry.register_descriptor({
            ROAD_COMPONENT_ID,
            static_cast<std::uint16_t>(sizeof(RoadComponent)),
            static_cast<std::uint16_t>(alignof(RoadComponent)),
            "RoadComponent",
            ComponentRegistry::stable_name_hash("RoadComponent"),
        });

        m_district_count = 0;
        m_road_count = 0;
        m_frame_count = 0;
        return true;
    }

    void World::shutdown() noexcept
    {
        delete[] m_records;
        delete[] m_free_indices;
        delete[] m_entity_to_district;
        delete[] m_districts;
        delete[] m_roads;
        delete[] m_cell_entities;

        m_records = nullptr;
        m_free_indices = nullptr;
        m_entity_to_district = nullptr;
        m_districts = nullptr;
        m_roads = nullptr;
        m_cell_entities = nullptr;

        m_entities.reset();
        m_registry.reset();
        m_config = {};
        m_district_count = 0;
        m_road_count = 0;
        m_frame_count = 0;
    }

    void World::clear() noexcept
    {
        std::memset(m_entity_to_district, 0xFF, sizeof(std::uint32_t) * m_config.max_districts);
        const std::uint32_t cell_count = m_config.grid_width * m_config.grid_height;
        for (std::uint32_t i = 0; i < cell_count; ++i)
        {
            m_cell_entities[i] = {};
        }
        m_entities.clear();
        m_district_count = 0;
        m_road_count = 0;
    }

    bool World::valid_cell(std::uint32_t x, std::uint32_t y) const noexcept
    {
        return x < m_config.grid_width && y < m_config.grid_height;
    }

    std::uint32_t World::cell_index(std::uint32_t x, std::uint32_t y) const noexcept
    {
        return y * m_config.grid_width + x;
    }

    bool World::road_exists(EntityID from, EntityID to) const noexcept
    {
        for (std::uint32_t i = 0; i < m_road_count; ++i)
        {
            const RoadComponent& road = m_roads[i];
            if ((road.from == from && road.to == to) || (road.from == to && road.to == from))
            {
                return true;
            }
        }
        return false;
    }

    void World::connect_neighbor(std::uint32_t x, std::uint32_t y, std::int32_t dx, std::int32_t dy, EntityID from) noexcept
    {
        const std::int32_t nx = static_cast<std::int32_t>(x) + dx;
        const std::int32_t ny = static_cast<std::int32_t>(y) + dy;
        if (nx < 0 || ny < 0)
        {
            return;
        }

        if (!valid_cell(static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(ny)))
        {
            return;
        }

        const EntityID to = m_cell_entities[cell_index(static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(ny))];
        if (!is_valid(to))
        {
            return;
        }

        connect_districts(from, to, 1.0f);
    }

    EntityID World::spawn_district(std::uint32_t x, std::uint32_t y) noexcept
    {
        if (!valid_cell(x, y) || m_district_count >= m_config.max_districts)
        {
            return {};
        }

        const std::uint32_t index = cell_index(x, y);
        if (is_valid(m_cell_entities[index]))
        {
            return m_cell_entities[index];
        }

        const EntityID entity = m_entities.create();
        if (!is_valid(entity))
        {
            return {};
        }

        DistrictComponent& district = m_districts[m_district_count];
        district = {};
        district.entity = entity;
        district.grid_x = x;
        district.grid_y = y;
        district.population = 800.0f + static_cast<float>((x + y) * 120u);
        district.wealth = 250.0f;
        district.traffic = 0.05f;
        district.stability = 0.85f;
        district.instability = 0.10f;
        district.pressure = 0.15f;

        m_cell_entities[index] = entity;
        m_entity_to_district[entity.index] = m_district_count;
        m_entities.set_location(entity, { 0u, index, 0u });
        ++m_district_count;

        connect_neighbor(x, y, -1, 0, entity);
        connect_neighbor(x, y, 1, 0, entity);
        connect_neighbor(x, y, 0, -1, entity);
        connect_neighbor(x, y, 0, 1, entity);

        return entity;
    }

    bool World::connect_districts(EntityID from, EntityID to, float length) noexcept
    {
        if (!is_valid(from) || !is_valid(to) || from == to || m_road_count >= m_config.max_roads)
        {
            return false;
        }

        if (road_exists(from, to))
        {
            return true;
        }

        m_roads[m_road_count] = {};
        m_roads[m_road_count].from = from;
        m_roads[m_road_count].to = to;
        m_roads[m_road_count].length = length;
        m_roads[m_road_count].congestion = 0.0f;
        ++m_road_count;
        return true;
    }

    DistrictComponent* World::district(EntityID id) noexcept
    {
        if (!m_entity_to_district || !is_valid(id) || id.index >= m_config.max_districts)
        {
            return nullptr;
        }

        const std::uint32_t slot = m_entity_to_district[id.index];
        if (slot == INVALID_ENTITY_INDEX || slot >= m_district_count)
        {
            return nullptr;
        }

        DistrictComponent& district = m_districts[slot];
        return district.entity == id ? &district : nullptr;
    }

    const DistrictComponent* World::district(EntityID id) const noexcept
    {
        return const_cast<World*>(this)->district(id);
    }

    DistrictComponent* World::district_at(std::uint32_t x, std::uint32_t y) noexcept
    {
        if (!valid_cell(x, y))
        {
            return nullptr;
        }

        const EntityID id = m_cell_entities[cell_index(x, y)];
        return district(id);
    }

    const DistrictComponent* World::district_at(std::uint32_t x, std::uint32_t y) const noexcept
    {
        return const_cast<World*>(this)->district_at(x, y);
    }

    DistrictComponent* World::district_slot(std::uint32_t index) noexcept
    {
        if (index >= m_district_count)
        {
            return nullptr;
        }
        return &m_districts[index];
    }

    const DistrictComponent* World::district_slot(std::uint32_t index) const noexcept
    {
        if (index >= m_district_count)
        {
            return nullptr;
        }
        return &m_districts[index];
    }

    RoadComponent* World::road_slot(std::uint32_t index) noexcept
    {
        if (index >= m_road_count)
        {
            return nullptr;
        }
        return &m_roads[index];
    }

    const RoadComponent* World::road_slot(std::uint32_t index) const noexcept
    {
        if (index >= m_road_count)
        {
            return nullptr;
        }
        return &m_roads[index];
    }

    void World::set_district_state(EntityID id,
                                   float population,
                                   float wealth,
                                   float traffic,
                                   float stability,
                                   float instability,
                                   float pressure) noexcept
    {
        DistrictComponent* district = this->district(id);
        if (!district)
        {
            return;
        }

        district->population = population;
        district->wealth = wealth;
        district->traffic = traffic;
        district->stability = stability;
        district->instability = instability;
        district->pressure = pressure;
    }
}
