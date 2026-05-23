#pragma once

#include "archetype.h"

namespace acheron::ecs
{
    struct ComponentInit
    {
        ComponentTypeID id = INVALID_COMPONENT_TYPE;
        const void* data = nullptr;
        std::uint32_t size = 0;
    };

    struct MigrationResult
    {
        bool migrated = false;
        EntityLocation old_location{};
        EntityLocation new_location{};
        EntityID swapped_entity{};
    };

    class ChunkMigration final
    {
    public:
        static MigrationResult migrate(EntityTable& entities,
                                       ArchetypeStorage& source,
                                       ArchetypeStorage& destination,
                                       EntityID entity,
                                       const ComponentInit* added_components,
                                       std::uint32_t added_component_count) noexcept;

        static MigrationResult add_components(EntityTable& entities,
                                              ArchetypeStorage& source,
                                              ArchetypeStorage& destination,
                                              EntityID entity,
                                              const ComponentInit* added_components,
                                              std::uint32_t added_component_count) noexcept;

        static MigrationResult remove_components(EntityTable& entities,
                                                 ArchetypeStorage& source,
                                                 ArchetypeStorage& destination,
                                                 EntityID entity) noexcept;
    };
}
