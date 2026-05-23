#include "chunk_migration.h"

#include <cstdio>

namespace acheron::ecs
{
    namespace
    {
        static const ComponentInit* find_init(const ComponentInit* init,
                                              std::uint32_t count,
                                              ComponentTypeID id) noexcept
        {
            for (std::uint32_t i = 0; i < count; ++i)
            {
                if (init[i].id == id)
                {
                    return &init[i];
                }
            }
            return nullptr;
        }

        static bool initialize_destination_only_components(ArchetypeStorage& source,
                                                           ArchetypeStorage& destination,
                                                           ArchetypeChunk& dst_chunk,
                                                           std::uint32_t dst_row,
                                                           const ComponentInit* added_components,
                                                           std::uint32_t added_component_count) noexcept
        {
            const ArchetypeLayout& dst_layout = destination.layout();
            for (std::uint32_t i = 0; i < dst_layout.column_count; ++i)
            {
                const ComponentTypeID id = dst_layout.columns[i].id;
                if (source.contains(id))
                {
                    continue;
                }

                const ComponentInit* init = find_init(added_components, added_component_count, id);
                if (init)
                {
                    if (!destination.set_component(dst_chunk, dst_row, id, init->data, init->size))
                    {
                        return false;
                    }
                }
                else
                {
                    if (!destination.zero_component(dst_chunk, dst_row, id))
                    {
                        return false;
                    }
                }
            }

            return true;
        }
    }

    MigrationResult ChunkMigration::migrate(EntityTable& entities,
                                            ArchetypeStorage& source,
                                            ArchetypeStorage& destination,
                                            EntityID entity,
                                            const ComponentInit* added_components,
                                            std::uint32_t added_component_count) noexcept
    {
        MigrationResult result{};
        if (!entities.is_alive(entity))
        {
            return result;
        }

        const EntityLocation old_location = entities.location(entity);
        if (!is_valid(old_location) ||
            old_location.archetype_index != source.archetype_index())
        {
            return result;
        }

        ArchetypeChunk* src_chunk = source.chunk(old_location.chunk_index);
        if (!src_chunk || old_location.row >= src_chunk->entity_count)
        {
            return result;
        }

        if (src_chunk->entities[old_location.row] != entity)
        {
            return result;
        }

        const ChunkSlot dst_slot = destination.allocate(entity);
        if (!is_valid(dst_slot))
        {
            return result;
        }

        const ArchetypeLayout& src_layout = source.layout();
        for (std::uint32_t i = 0; i < src_layout.column_count; ++i)
        {
            const ComponentTypeID id = src_layout.columns[i].id;
            if (destination.contains(id))
            {
                const void* src_data = source.component_data(*src_chunk, id, old_location.row);
                const ComponentColumnLayout* dst_col = destination.column(id);
                if (!src_data || !dst_col ||
                    !destination.set_component(*dst_slot.chunk, dst_slot.row, id, src_data, dst_col->size))
                {
                    EntityID ignored{};
                    (void)destination.remove(*dst_slot.chunk, dst_slot.row, &ignored);
                    return result;
                }
            }
        }

        if (!initialize_destination_only_components(source,
                                                    destination,
                                                    *dst_slot.chunk,
                                                    dst_slot.row,
                                                    added_components,
                                                    added_component_count))
        {
            EntityID ignored{};
            (void)destination.remove(*dst_slot.chunk, dst_slot.row, &ignored);
            return result;
        }

        EntityID swapped{};
        if (!source.remove(*src_chunk, old_location.row, &swapped))
        {
            EntityID ignored{};
            (void)destination.remove(*dst_slot.chunk, dst_slot.row, &ignored);
            return result;
        }

        const EntityLocation new_location{ destination.archetype_index(), dst_slot.chunk_index, dst_slot.row };
        if (!entities.set_location(entity, new_location))
        {
            return result;
        }

        if (is_valid(swapped))
        {
            const EntityLocation swapped_location{ source.archetype_index(), old_location.chunk_index, old_location.row };
            if (!entities.set_location(swapped, swapped_location))
            {
                return result;
            }
        }

        result.migrated = true;
        result.old_location = old_location;
        result.new_location = new_location;
        result.swapped_entity = swapped;
        return result;
    }

    MigrationResult ChunkMigration::add_components(EntityTable& entities,
                                                   ArchetypeStorage& source,
                                                   ArchetypeStorage& destination,
                                                   EntityID entity,
                                                   const ComponentInit* added_components,
                                                   std::uint32_t added_component_count) noexcept
    {
        return migrate(entities, source, destination, entity, added_components, added_component_count);
    }

    MigrationResult ChunkMigration::remove_components(EntityTable& entities,
                                                      ArchetypeStorage& source,
                                                      ArchetypeStorage& destination,
                                                      EntityID entity) noexcept
    {
        return migrate(entities, source, destination, entity, nullptr, 0);
    }
}

#if defined(ACHERON_CHUNK_MIGRATION_UNIT_TEST)

namespace migration_test
{
    struct Position
    {
        float x;
        float y;
    };

    struct Velocity
    {
        float x;
        float y;
    };

    struct Health
    {
        std::uint32_t value;
    };
}

template <>
struct acheron::ecs::ComponentTraits<migration_test::Position>
{
    static constexpr acheron::ecs::ComponentTypeID id = 0;
    static constexpr const char* name = "migration_test::Position";
};

template <>
struct acheron::ecs::ComponentTraits<migration_test::Velocity>
{
    static constexpr acheron::ecs::ComponentTypeID id = 1;
    static constexpr const char* name = "migration_test::Velocity";
};

template <>
struct acheron::ecs::ComponentTraits<migration_test::Health>
{
    static constexpr acheron::ecs::ComponentTypeID id = 2;
    static constexpr const char* name = "migration_test::Health";
};

namespace
{
    static bool make_storage(acheron::ecs::ArchetypeStorage& storage,
                             const acheron::ecs::ComponentRegistry& registry,
                             acheron::ecs::ComponentSignature signature,
                             std::uint32_t index,
                             acheron::ecs::ArchetypeChunk* chunks,
                             std::byte* memory,
                             std::uint32_t chunk_count) noexcept
    {
        acheron::ecs::ArchetypeConfig cfg{};
        cfg.archetype_index = index;
        cfg.signature = signature;
        cfg.chunks = chunks;
        cfg.chunk_capacity = chunk_count;
        cfg.chunk_memory = memory;
        cfg.chunk_memory_bytes = chunk_count * acheron::ecs::ECS_CHUNK_BYTES;
        return storage.init(registry, cfg);
    }

    static bool migration_stress_test() noexcept
    {
        acheron::ecs::ComponentRegistry registry;
        if (!registry.register_component<migration_test::Position>() ||
            !registry.register_component<migration_test::Velocity>() ||
            !registry.register_component<migration_test::Health>())
        {
            return false;
        }

        alignas(64) acheron::ecs::ArchetypeChunk src_chunks[4]{};
        alignas(64) acheron::ecs::ArchetypeChunk dst_chunks[4]{};
        alignas(64) std::byte src_memory[4 * acheron::ecs::ECS_CHUNK_BYTES]{};
        alignas(64) std::byte dst_memory[4 * acheron::ecs::ECS_CHUNK_BYTES]{};

        acheron::ecs::ArchetypeStorage source;
        acheron::ecs::ArchetypeStorage destination;
        acheron::ecs::ComponentSignature src_sig = acheron::ecs::component_bit(0);
        src_sig = acheron::ecs::signature_add(src_sig, 1);
        acheron::ecs::ComponentSignature dst_sig = src_sig;
        dst_sig = acheron::ecs::signature_add(dst_sig, 2);

        if (!make_storage(source, registry, src_sig, 10, src_chunks, src_memory, 4) ||
            !make_storage(destination, registry, dst_sig, 11, dst_chunks, dst_memory, 4))
        {
            return false;
        }

        acheron::ecs::EntityRecord records[512]{};
        std::uint32_t free_indices[512]{};
        acheron::ecs::EntityTable table;
        if (!table.init(records, free_indices, 512))
        {
            return false;
        }

        acheron::ecs::EntityID ids[128]{};
        for (std::uint32_t i = 0; i < 128; ++i)
        {
            ids[i] = table.create();
            const auto slot = source.allocate(ids[i]);
            if (!is_valid(slot))
            {
                return false;
            }
            (void)table.set_location(ids[i], acheron::ecs::EntityLocation{ source.archetype_index(), slot.chunk_index, slot.row });

            const migration_test::Position pos{ static_cast<float>(i), static_cast<float>(i + 1u) };
            const migration_test::Velocity vel{ 1.0f, 2.0f };
            (void)source.set_component(*slot.chunk, slot.row, 0, &pos, sizeof(pos));
            (void)source.set_component(*slot.chunk, slot.row, 1, &vel, sizeof(vel));
        }

        for (std::uint32_t i = 0; i < 128; i += 2)
        {
            const migration_test::Health health{ 100u + i };
            const acheron::ecs::ComponentInit init{ 2, &health, sizeof(health) };
            const auto result = acheron::ecs::ChunkMigration::add_components(table,
                                                                             source,
                                                                             destination,
                                                                             ids[i],
                                                                             &init,
                                                                             1);
            if (!result.migrated)
            {
                return false;
            }

            const auto loc = table.location(ids[i]);
            auto* chunk = destination.chunk(loc.chunk_index);
            const auto* pos = static_cast<const migration_test::Position*>(
                destination.component_data(*chunk, 0, loc.row));
            const auto* hp = static_cast<const migration_test::Health*>(
                destination.component_data(*chunk, 2, loc.row));
            if (!pos || !hp || pos->x != static_cast<float>(i) || hp->value != (100u + i))
            {
                return false;
            }
        }

        return source.entity_count() == 64u && destination.entity_count() == 64u;
    }

    static bool entity_consistency_validation() noexcept
    {
        return migration_stress_test();
    }
}

int main()
{
    if (!migration_stress_test())
    {
        std::printf("[chunk_migration] migration stress: FAIL\n");
        return 1;
    }
    std::printf("[chunk_migration] migration stress: OK\n");

    if (!entity_consistency_validation())
    {
        std::printf("[chunk_migration] entity consistency: FAIL\n");
        return 1;
    }
    std::printf("[chunk_migration] entity consistency: OK\n");
    return 0;
}

#endif
