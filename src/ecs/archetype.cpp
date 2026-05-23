#include "archetype.h"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace acheron::ecs
{
    namespace
    {
        static bool is_power_of_two(std::uint32_t value) noexcept
        {
            return value != 0u && ((value & (value - 1u)) == 0u);
        }

        static bool compute_layout_for_capacity(ArchetypeLayout& layout, std::uint32_t capacity) noexcept
        {
            std::uint32_t offset = 0;
            for (std::uint32_t i = 0; i < layout.column_count; ++i)
            {
                ComponentColumnLayout& column = layout.columns[i];
                const std::uint32_t alignment = column.alignment;
                const std::uint32_t mask = alignment - 1u;
                offset = (offset + mask) & ~mask;

                column.offset = offset;
                const std::uint64_t bytes = static_cast<std::uint64_t>(column.size) * capacity;
                if (bytes > 0xFFFF'FFFFull || offset > (ECS_CHUNK_BYTES - static_cast<std::uint32_t>(bytes)))
                {
                    return false;
                }
                offset += static_cast<std::uint32_t>(bytes);
            }

            layout.entity_capacity = capacity;
            layout.bytes_used = offset;
            return true;
        }
    }

    bool ArchetypeStorage::init(const ComponentRegistry& registry, const ArchetypeConfig& config) noexcept
    {
        if (!config.chunks || !config.chunk_memory || config.chunk_capacity == 0u)
        {
            reset();
            return false;
        }

        const std::uint64_t required_memory = static_cast<std::uint64_t>(config.chunk_capacity) * ECS_CHUNK_BYTES;
        if (config.chunk_memory_bytes < required_memory)
        {
            reset();
            return false;
        }

        if (!build_layout(registry, config.signature))
        {
            reset();
            return false;
        }

        m_archetype_index = config.archetype_index;
        m_chunks = config.chunks;
        m_chunk_capacity = config.chunk_capacity;
        m_chunk_count = 0;
        m_entity_count = 0;
        m_chunk_memory = config.chunk_memory;
        m_chunk_memory_bytes = config.chunk_memory_bytes;

        for (std::uint32_t i = 0; i < m_chunk_capacity; ++i)
        {
            m_chunks[i] = {};
            m_chunks[i].chunk_index = i;
        }

        return true;
    }

    void ArchetypeStorage::reset() noexcept
    {
        m_archetype_index = 0;
        m_layout = {};
        m_chunks = nullptr;
        m_chunk_capacity = 0;
        m_chunk_count = 0;
        m_entity_count = 0;
        m_chunk_memory = nullptr;
        m_chunk_memory_bytes = 0;
    }

    ChunkSlot ArchetypeStorage::allocate(EntityID entity) noexcept
    {
        if (!is_valid(entity) || !m_chunks)
        {
            return {};
        }

        for (std::uint32_t i = 0; i < m_chunk_count; ++i)
        {
            ArchetypeChunk& candidate = m_chunks[i];
            if (candidate.entity_count < candidate.entity_capacity)
            {
                const std::uint32_t row = candidate.entity_count;
                candidate.entities[row] = entity;
                ++candidate.entity_count;
                ++m_entity_count;
                return ChunkSlot{ &candidate, i, row };
            }
        }

        if (m_chunk_count >= m_chunk_capacity)
        {
            return {};
        }

        const std::uint32_t chunk_index_value = m_chunk_count;
        ArchetypeChunk& created = m_chunks[chunk_index_value];
        if (!create_chunk(created, chunk_index_value))
        {
            return {};
        }
        ++m_chunk_count;

        created.entities[0] = entity;
        created.entity_count = 1;
        ++m_entity_count;
        return ChunkSlot{ &created, chunk_index_value, 0 };
    }

    bool ArchetypeStorage::remove(ArchetypeChunk& chunk_ref, std::uint32_t row, EntityID* moved_entity) noexcept
    {
        if (chunk_ref.chunk_index >= m_chunk_count || row >= chunk_ref.entity_count)
        {
            return false;
        }

        const std::uint32_t last_row = chunk_ref.entity_count - 1u;
        if (moved_entity)
        {
            *moved_entity = {};
        }

        if (row != last_row)
        {
            const EntityID moved = chunk_ref.entities[last_row];
            chunk_ref.entities[row] = moved;
            for (std::uint32_t i = 0; i < m_layout.column_count; ++i)
            {
                const ComponentColumnLayout& col = m_layout.columns[i];
                std::byte* base = chunk_ref.memory + col.offset;
                std::memmove(base + (static_cast<std::size_t>(row) * col.size),
                             base + (static_cast<std::size_t>(last_row) * col.size),
                             col.size);
            }
            if (moved_entity)
            {
                *moved_entity = moved;
            }
        }

        chunk_ref.entities[last_row] = {};
        --chunk_ref.entity_count;
        if (m_entity_count != 0u)
        {
            --m_entity_count;
        }
        return true;
    }

    bool ArchetypeStorage::set_component(ArchetypeChunk& chunk_ref,
                                         std::uint32_t row,
                                         ComponentTypeID component,
                                         const void* data,
                                         std::uint32_t size) noexcept
    {
        const ComponentColumnLayout* col = column(component);
        if (!col || !data || size != col->size || row >= chunk_ref.entity_count)
        {
            return false;
        }

        void* dst = component_data(chunk_ref, component, row);
        if (!dst)
        {
            return false;
        }

        std::memcpy(dst, data, col->size);
        return true;
    }

    bool ArchetypeStorage::zero_component(ArchetypeChunk& chunk_ref, std::uint32_t row, ComponentTypeID component) noexcept
    {
        const ComponentColumnLayout* col = column(component);
        if (!col || row >= chunk_ref.entity_count)
        {
            return false;
        }

        void* dst = component_data(chunk_ref, component, row);
        if (!dst)
        {
            return false;
        }

        std::memset(dst, 0, col->size);
        return true;
    }

    void* ArchetypeStorage::component_data(ArchetypeChunk& chunk_ref, ComponentTypeID component, std::uint32_t row) noexcept
    {
        const ComponentColumnLayout* col = column(component);
        if (!col || row >= chunk_ref.entity_count)
        {
            return nullptr;
        }

        return chunk_ref.memory + col->offset + (static_cast<std::size_t>(row) * col->size);
    }

    const void* ArchetypeStorage::component_data(const ArchetypeChunk& chunk_ref,
                                                ComponentTypeID component,
                                                std::uint32_t row) const noexcept
    {
        const ComponentColumnLayout* col = column(component);
        if (!col || row >= chunk_ref.entity_count)
        {
            return nullptr;
        }

        return chunk_ref.memory + col->offset + (static_cast<std::size_t>(row) * col->size);
    }

    void* ArchetypeStorage::column_data(ArchetypeChunk& chunk_ref, ComponentTypeID component) noexcept
    {
        const ComponentColumnLayout* col = column(component);
        return col ? static_cast<void*>(chunk_ref.memory + col->offset) : nullptr;
    }

    const void* ArchetypeStorage::column_data(const ArchetypeChunk& chunk_ref, ComponentTypeID component) const noexcept
    {
        const ComponentColumnLayout* col = column(component);
        return col ? static_cast<const void*>(chunk_ref.memory + col->offset) : nullptr;
    }

    bool ArchetypeStorage::copy_component(const ArchetypeChunk& src,
                                          std::uint32_t src_row,
                                          ArchetypeChunk& dst,
                                          std::uint32_t dst_row,
                                          ComponentTypeID component) noexcept
    {
        const ComponentColumnLayout* col = column(component);
        if (!col || src_row >= src.entity_count || dst_row >= dst.entity_count)
        {
            return false;
        }

        const void* src_data = component_data(src, component, src_row);
        void* dst_data = component_data(dst, component, dst_row);
        if (!src_data || !dst_data)
        {
            return false;
        }

        std::memcpy(dst_data, src_data, col->size);
        return true;
    }

    bool ArchetypeStorage::contains(ComponentTypeID component) const noexcept
    {
        return signature_contains(m_layout.signature, component);
    }

    const ComponentColumnLayout* ArchetypeStorage::column(ComponentTypeID component) const noexcept
    {
        const std::uint32_t index = column_index(component);
        return (index != INVALID_COMPONENT_TYPE) ? &m_layout.columns[index] : nullptr;
    }

    std::uint32_t ArchetypeStorage::column_index(ComponentTypeID component) const noexcept
    {
        for (std::uint32_t i = 0; i < m_layout.column_count; ++i)
        {
            if (m_layout.columns[i].id == component)
            {
                return i;
            }
        }
        return INVALID_COMPONENT_TYPE;
    }

    ArchetypeChunk* ArchetypeStorage::chunk(std::uint32_t chunk_index_value) noexcept
    {
        return (chunk_index_value < m_chunk_count) ? &m_chunks[chunk_index_value] : nullptr;
    }

    const ArchetypeChunk* ArchetypeStorage::chunk(std::uint32_t chunk_index_value) const noexcept
    {
        return (chunk_index_value < m_chunk_count) ? &m_chunks[chunk_index_value] : nullptr;
    }

    bool ArchetypeStorage::build_layout(const ComponentRegistry& registry, ComponentSignature signature_value) noexcept
    {
        m_layout = {};
        m_layout.signature = signature_value;

        for (ComponentTypeID id = 0; id < MAX_COMPONENT_TYPES; ++id)
        {
            if (!signature_contains(signature_value, id))
            {
                continue;
            }

            const ComponentDescriptor* desc = registry.descriptor(id);
            if (!desc || m_layout.column_count >= MAX_COMPONENTS_PER_ARCHETYPE)
            {
                return false;
            }

            if (!is_power_of_two(desc->alignment) || desc->size == 0u)
            {
                return false;
            }

            ComponentColumnLayout& col = m_layout.columns[m_layout.column_count];
            col.id = id;
            col.size = desc->size;
            col.alignment = desc->alignment;
            col.offset = 0;
            ++m_layout.column_count;
        }

        if (m_layout.column_count == 0u)
        {
            return false;
        }

        std::uint32_t capacity = MAX_ENTITIES_PER_CHUNK;
        while (capacity != 0u)
        {
            ArchetypeLayout candidate = m_layout;
            if (compute_layout_for_capacity(candidate, capacity))
            {
                m_layout = candidate;
                return true;
            }
            --capacity;
        }

        return false;
    }

    bool ArchetypeStorage::create_chunk(ArchetypeChunk& chunk_ref, std::uint32_t chunk_index_value) noexcept
    {
        const std::uint64_t byte_offset = static_cast<std::uint64_t>(chunk_index_value) * ECS_CHUNK_BYTES;
        if ((byte_offset + ECS_CHUNK_BYTES) > m_chunk_memory_bytes)
        {
            return false;
        }

        std::byte* base = m_chunk_memory + byte_offset;
        for (std::uint32_t i = 0; i < m_layout.column_count; ++i)
        {
            const ComponentColumnLayout& col = m_layout.columns[i];
            const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(base + col.offset);
            if ((address & static_cast<std::uintptr_t>(col.alignment - 1u)) != 0u)
            {
                return false;
            }
        }

        chunk_ref.memory = base;
        chunk_ref.chunk_index = chunk_index_value;
        chunk_ref.entity_count = 0;
        chunk_ref.entity_capacity = m_layout.entity_capacity;
        for (std::uint32_t i = 0; i < MAX_ENTITIES_PER_CHUNK; ++i)
        {
            chunk_ref.entities[i] = {};
        }
        return true;
    }
}

#if defined(ACHERON_ARCHETYPE_UNIT_TEST)

namespace archetype_test
{
    struct Position
    {
        float x;
        float y;
        float z;
        float w;
    };

    struct Velocity
    {
        float x;
        float y;
        float z;
        float w;
    };
}

template <>
struct acheron::ecs::ComponentTraits<archetype_test::Position>
{
    static constexpr acheron::ecs::ComponentTypeID id = 0;
    static constexpr const char* name = "archetype_test::Position";
};

template <>
struct acheron::ecs::ComponentTraits<archetype_test::Velocity>
{
    static constexpr acheron::ecs::ComponentTypeID id = 1;
    static constexpr const char* name = "archetype_test::Velocity";
};

namespace
{
    static bool build_test_storage(acheron::ecs::ArchetypeStorage& storage,
                                   acheron::ecs::ComponentRegistry& registry,
                                   acheron::ecs::ArchetypeChunk* chunks,
                                   std::byte* memory,
                                   std::uint32_t chunk_count) noexcept
    {
        registry.reset();
        if (!registry.register_component<archetype_test::Position>() ||
            !registry.register_component<archetype_test::Velocity>())
        {
            return false;
        }

        acheron::ecs::ComponentSignature sig = acheron::ecs::component_bit(0);
        sig = acheron::ecs::signature_add(sig, 1);

        acheron::ecs::ArchetypeConfig cfg{};
        cfg.archetype_index = 3;
        cfg.signature = sig;
        cfg.chunks = chunks;
        cfg.chunk_capacity = chunk_count;
        cfg.chunk_memory = memory;
        cfg.chunk_memory_bytes = chunk_count * acheron::ecs::ECS_CHUNK_BYTES;
        return storage.init(registry, cfg);
    }

    static bool simd_iteration_validation() noexcept
    {
        alignas(64) acheron::ecs::ArchetypeChunk chunks[2]{};
        alignas(64) std::byte memory[2 * acheron::ecs::ECS_CHUNK_BYTES]{};
        acheron::ecs::ComponentRegistry registry;
        acheron::ecs::ArchetypeStorage storage;
        if (!build_test_storage(storage, registry, chunks, memory, 2))
        {
            return false;
        }

        for (std::uint32_t i = 0; i < 256; ++i)
        {
            const acheron::ecs::EntityID entity{ i, 1 };
            const auto slot = storage.allocate(entity);
            if (!is_valid(slot))
            {
                return false;
            }

            const archetype_test::Position pos{ static_cast<float>(i), 0.0f, 0.0f, 1.0f };
            const archetype_test::Velocity vel{ 1.0f, 2.0f, 3.0f, 0.0f };
            if (!storage.set_component(*slot.chunk, slot.row, 0, &pos, sizeof(pos)) ||
                !storage.set_component(*slot.chunk, slot.row, 1, &vel, sizeof(vel)))
            {
                return false;
            }
        }

        for (std::uint32_t c = 0; c < storage.chunk_count(); ++c)
        {
            auto* chunk = storage.chunk(c);
            auto* pos = static_cast<archetype_test::Position*>(storage.column_data(*chunk, 0));
            auto* vel = static_cast<archetype_test::Velocity*>(storage.column_data(*chunk, 1));
            if (!pos || !vel)
            {
                return false;
            }

            if ((reinterpret_cast<std::uintptr_t>(pos) & 63u) != 0u ||
                (reinterpret_cast<std::uintptr_t>(vel) & 63u) != 0u)
            {
                return false;
            }

            for (std::uint32_t i = 0; i < chunk->entity_count; i += 4)
            {
                const std::uint32_t limit = ((i + 4u) <= chunk->entity_count) ? 4u : (chunk->entity_count - i);
                for (std::uint32_t lane = 0; lane < limit; ++lane)
                {
                    pos[i + lane].x += vel[i + lane].x;
                    pos[i + lane].y += vel[i + lane].y;
                }
            }
        }

        const auto* chunk0 = storage.chunk(0);
        const auto* pos0 = static_cast<const archetype_test::Position*>(storage.column_data(*chunk0, 0));
        return pos0 && pos0[10].x == 11.0f && pos0[10].y == 2.0f;
    }

    static bool cache_traversal_benchmark() noexcept
    {
        alignas(64) acheron::ecs::ArchetypeChunk chunks[8]{};
        alignas(64) std::byte memory[8 * acheron::ecs::ECS_CHUNK_BYTES]{};
        acheron::ecs::ComponentRegistry registry;
        acheron::ecs::ArchetypeStorage storage;
        if (!build_test_storage(storage, registry, chunks, memory, 8))
        {
            return false;
        }

        const std::uint32_t target = storage.entity_capacity_per_chunk() * 4u;
        for (std::uint32_t i = 0; i < target; ++i)
        {
            const auto slot = storage.allocate(acheron::ecs::EntityID{ i, 1 });
            if (!is_valid(slot))
            {
                return false;
            }
            const archetype_test::Position pos{ 1.0f, 2.0f, 3.0f, 4.0f };
            const archetype_test::Velocity vel{ 2.0f, 2.0f, 2.0f, 2.0f };
            (void)storage.set_component(*slot.chunk, slot.row, 0, &pos, sizeof(pos));
            (void)storage.set_component(*slot.chunk, slot.row, 1, &vel, sizeof(vel));
        }

        volatile float sink = 0.0f;
        const auto begin = std::chrono::steady_clock::now();
        for (std::uint32_t pass = 0; pass < 256; ++pass)
        {
            for (std::uint32_t c = 0; c < storage.chunk_count(); ++c)
            {
                auto* chunk = storage.chunk(c);
                const auto* pos = static_cast<const archetype_test::Position*>(storage.column_data(*chunk, 0));
                for (std::uint32_t i = 0; i < chunk->entity_count; ++i)
                {
                    sink += pos[i].x;
                }
            }
        }
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        std::printf("[archetype] cache traversal %u entities x256: %lld us\n",
                    target,
                    static_cast<long long>(elapsed_us));
        return sink > 0.0f;
    }
}

int main()
{
    if (!simd_iteration_validation())
    {
        std::printf("[archetype] SIMD iteration: FAIL\n");
        return 1;
    }
    std::printf("[archetype] SIMD iteration: OK\n");

    if (!cache_traversal_benchmark())
    {
        std::printf("[archetype] cache traversal: FAIL\n");
        return 1;
    }
    std::printf("[archetype] cache traversal: OK\n");
    return 0;
}

#endif
