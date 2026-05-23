#include "world_serializer.h"
#include "../core/logger.h"
#include <fstream>
#include <vector>

namespace acheron::ecs
{
    // A simple binary format header to verify files
    static constexpr std::uint32_t SAVE_MAGIC = 0x41434852; // "ACHR"
    static constexpr std::uint32_t SAVE_VERSION = 1;

    bool WorldSerializer::save_to_file(const World& world, const std::string& filepath)
    {
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open())
        {
            ACH_LOG_ERROR("WorldSerializer: Failed to open file for writing: %s", filepath.c_str());
            return false;
        }

        // Header
        file.write(reinterpret_cast<const char*>(&SAVE_MAGIC), sizeof(SAVE_MAGIC));
        file.write(reinterpret_cast<const char*>(&SAVE_VERSION), sizeof(SAVE_VERSION));

        // Config
        file.write(reinterpret_cast<const char*>(&world.m_config), sizeof(World::Config));

        // Counts
        file.write(reinterpret_cast<const char*>(&world.m_district_count), sizeof(std::uint32_t));
        file.write(reinterpret_cast<const char*>(&world.m_road_count), sizeof(std::uint32_t));
        file.write(reinterpret_cast<const char*>(&world.m_frame_count), sizeof(std::uint64_t));

        // Entities mapping (EntityID inside cells)
        std::uint32_t cell_count = world.m_config.grid_width * world.m_config.grid_height;
        file.write(reinterpret_cast<const char*>(world.m_cell_entities), cell_count * sizeof(EntityID));

        // Data arrays
        file.write(reinterpret_cast<const char*>(world.m_districts), world.m_config.max_districts * sizeof(DistrictComponent));
        file.write(reinterpret_cast<const char*>(world.m_roads), world.m_config.max_roads * sizeof(RoadComponent));

        ACH_LOG_INFO("WorldSerializer: Successfully saved world state to %s", filepath.c_str());
        return true;
    }

    bool WorldSerializer::load_from_file(World& world, const std::string& filepath)
    {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open())
        {
            ACH_LOG_ERROR("WorldSerializer: Failed to open file for reading: %s", filepath.c_str());
            return false;
        }

        std::uint32_t magic = 0;
        std::uint32_t version = 0;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));

        if (magic != SAVE_MAGIC || version != SAVE_VERSION)
        {
            ACH_LOG_ERROR("WorldSerializer: Invalid save file or version mismatch.");
            return false;
        }

        World::Config loaded_config;
        file.read(reinterpret_cast<char*>(&loaded_config), sizeof(World::Config));

        // If config changed dramatically, we would need to re-init. For simplicity in demo, assume same config size constraints.
        // In a real scenario, we'd shutdown and re-initialize the world with the new config.
        if (loaded_config.grid_width != world.m_config.grid_width || loaded_config.max_districts > world.m_config.max_districts)
        {
             world.shutdown();
             world.initialize(loaded_config);
        }

        file.read(reinterpret_cast<char*>(&world.m_district_count), sizeof(std::uint32_t));
        file.read(reinterpret_cast<char*>(&world.m_road_count), sizeof(std::uint32_t));
        file.read(reinterpret_cast<char*>(&world.m_frame_count), sizeof(std::uint64_t));

        std::uint32_t cell_count = world.m_config.grid_width * world.m_config.grid_height;
        file.read(reinterpret_cast<char*>(world.m_cell_entities), cell_count * sizeof(EntityID));

        file.read(reinterpret_cast<char*>(world.m_districts), world.m_config.max_districts * sizeof(DistrictComponent));
        file.read(reinterpret_cast<char*>(world.m_roads), world.m_config.max_roads * sizeof(RoadComponent));

        ACH_LOG_INFO("WorldSerializer: Successfully loaded world state from %s", filepath.c_str());
        return true;
    }
}
