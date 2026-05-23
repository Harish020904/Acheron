#pragma once

#include "component_registry.h"
#include "entity_id.h"

#include <cstddef>
#include <cstdint>

namespace acheron::ecs
{
    static constexpr std::uint32_t ECS_CACHE_LINE_BYTES = 64u;
    static constexpr std::uint32_t ECS_CHUNK_BYTES = 16u * 1024u;
    static constexpr std::uint32_t MAX_COMPONENTS_PER_ARCHETYPE = 32u;
    static constexpr std::uint32_t MAX_ENTITIES_PER_CHUNK = 1024u;

    struct ComponentColumnLayout
    {
        ComponentTypeID id = INVALID_COMPONENT_TYPE;
        std::uint32_t size = 0;
        std::uint32_t alignment = 0;
        std::uint32_t offset = 0;
    };

    struct ArchetypeLayout
    {
        ComponentSignature signature{};
        ComponentColumnLayout columns[MAX_COMPONENTS_PER_ARCHETYPE]{};
        std::uint32_t column_count = 0;
        std::uint32_t entity_capacity = 0;
        std::uint32_t bytes_used = 0;
    };

    struct alignas(ECS_CACHE_LINE_BYTES) ArchetypeChunk
    {
        std::byte* memory = nullptr;
        std::uint32_t chunk_index = INVALID_CHUNK_INDEX;
        std::uint32_t entity_count = 0;
        std::uint32_t entity_capacity = 0;
        EntityID entities[MAX_ENTITIES_PER_CHUNK]{};
    };

    struct ChunkSlot
    {
        ArchetypeChunk* chunk = nullptr;
        std::uint32_t chunk_index = INVALID_CHUNK_INDEX;
        std::uint32_t row = INVALID_CHUNK_ROW;
    };

    constexpr bool is_valid(ChunkSlot slot) noexcept
    {
        return slot.chunk != nullptr && slot.chunk_index != INVALID_CHUNK_INDEX && slot.row != INVALID_CHUNK_ROW;
    }

    struct ArchetypeConfig
    {
        std::uint32_t archetype_index = 0;
        ComponentSignature signature{};
        ArchetypeChunk* chunks = nullptr;
        std::uint32_t chunk_capacity = 0;
        std::byte* chunk_memory = nullptr;
        std::uint32_t chunk_memory_bytes = 0;
    };

    class ArchetypeStorage final
    {
    public:
        ArchetypeStorage() noexcept = default;

        ArchetypeStorage(const ArchetypeStorage&) = delete;
        ArchetypeStorage& operator=(const ArchetypeStorage&) = delete;

        bool init(const ComponentRegistry& registry, const ArchetypeConfig& config) noexcept;
        void reset() noexcept;

        ChunkSlot allocate(EntityID entity) noexcept;
        bool remove(ArchetypeChunk& chunk, std::uint32_t row, EntityID* moved_entity) noexcept;

        bool set_component(ArchetypeChunk& chunk,
                           std::uint32_t row,
                           ComponentTypeID component,
                           const void* data,
                           std::uint32_t size) noexcept;

        bool zero_component(ArchetypeChunk& chunk, std::uint32_t row, ComponentTypeID component) noexcept;

        void* component_data(ArchetypeChunk& chunk, ComponentTypeID component, std::uint32_t row) noexcept;
        const void* component_data(const ArchetypeChunk& chunk, ComponentTypeID component, std::uint32_t row) const noexcept;

        void* column_data(ArchetypeChunk& chunk, ComponentTypeID component) noexcept;
        const void* column_data(const ArchetypeChunk& chunk, ComponentTypeID component) const noexcept;

        bool copy_component(const ArchetypeChunk& src,
                            std::uint32_t src_row,
                            ArchetypeChunk& dst,
                            std::uint32_t dst_row,
                            ComponentTypeID component) noexcept;

        bool contains(ComponentTypeID component) const noexcept;
        const ComponentColumnLayout* column(ComponentTypeID component) const noexcept;
        std::uint32_t column_index(ComponentTypeID component) const noexcept;

        const ArchetypeLayout& layout() const noexcept { return m_layout; }
        ComponentSignature signature() const noexcept { return m_layout.signature; }
        std::uint32_t archetype_index() const noexcept { return m_archetype_index; }

        ArchetypeChunk* chunk(std::uint32_t chunk_index) noexcept;
        const ArchetypeChunk* chunk(std::uint32_t chunk_index) const noexcept;

        std::uint32_t chunk_count() const noexcept { return m_chunk_count; }
        std::uint32_t chunk_capacity() const noexcept { return m_chunk_capacity; }
        std::uint32_t entity_count() const noexcept { return m_entity_count; }
        std::uint32_t entity_capacity_per_chunk() const noexcept { return m_layout.entity_capacity; }

    private:
        bool build_layout(const ComponentRegistry& registry, ComponentSignature signature) noexcept;
        bool create_chunk(ArchetypeChunk& chunk, std::uint32_t chunk_index) noexcept;

        std::uint32_t m_archetype_index = 0;
        ArchetypeLayout m_layout{};
        ArchetypeChunk* m_chunks = nullptr;
        std::uint32_t m_chunk_capacity = 0;
        std::uint32_t m_chunk_count = 0;
        std::uint32_t m_entity_count = 0;
        std::byte* m_chunk_memory = nullptr;
        std::uint32_t m_chunk_memory_bytes = 0;
    };
}
