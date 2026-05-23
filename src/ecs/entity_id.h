#pragma once

#include <cstdint>

namespace acheron::ecs
{
    static constexpr std::uint32_t INVALID_ENTITY_INDEX = 0xFFFF'FFFFu;
    static constexpr std::uint32_t INVALID_ENTITY_GENERATION = 0u;
    static constexpr std::uint32_t FIRST_ENTITY_GENERATION = 1u;
    static constexpr std::uint32_t INVALID_ARCHETYPE_INDEX = 0xFFFF'FFFFu;
    static constexpr std::uint32_t INVALID_CHUNK_INDEX = 0xFFFF'FFFFu;
    static constexpr std::uint32_t INVALID_CHUNK_ROW = 0xFFFF'FFFFu;

    struct EntityID
    {
        std::uint32_t index = INVALID_ENTITY_INDEX;
        std::uint32_t generation = INVALID_ENTITY_GENERATION;
    };

    constexpr bool operator==(EntityID a, EntityID b) noexcept
    {
        return a.index == b.index && a.generation == b.generation;
    }

    constexpr bool operator!=(EntityID a, EntityID b) noexcept
    {
        return !(a == b);
    }

    constexpr bool is_valid(EntityID id) noexcept
    {
        return id.index != INVALID_ENTITY_INDEX && id.generation != INVALID_ENTITY_GENERATION;
    }

    struct EntityLocation
    {
        std::uint32_t archetype_index = INVALID_ARCHETYPE_INDEX;
        std::uint32_t chunk_index = INVALID_CHUNK_INDEX;
        std::uint32_t row = INVALID_CHUNK_ROW;
    };

    constexpr bool is_valid(EntityLocation location) noexcept
    {
        return location.archetype_index != INVALID_ARCHETYPE_INDEX &&
               location.chunk_index != INVALID_CHUNK_INDEX &&
               location.row != INVALID_CHUNK_ROW;
    }

    struct EntityRecord
    {
        std::uint32_t generation = FIRST_ENTITY_GENERATION;
        bool alive = false;
        EntityLocation location{};
    };

    class EntityTable final
    {
    public:
        EntityTable() noexcept = default;

        EntityTable(const EntityTable&) = delete;
        EntityTable& operator=(const EntityTable&) = delete;

        bool init(EntityRecord* records, std::uint32_t* free_indices, std::uint32_t capacity) noexcept;
        void reset() noexcept;
        void clear() noexcept;

        EntityID create() noexcept;
        bool destroy(EntityID id) noexcept;

        bool is_alive(EntityID id) const noexcept;
        EntityRecord* record(EntityID id) noexcept;
        const EntityRecord* record(EntityID id) const noexcept;

        bool set_location(EntityID id, EntityLocation location) noexcept;
        EntityLocation location(EntityID id) const noexcept;

        std::uint32_t capacity() const noexcept { return m_capacity; }
        std::uint32_t alive_count() const noexcept { return m_alive_count; }
        std::uint32_t free_count() const noexcept { return m_free_count; }

    private:
        EntityRecord* m_records = nullptr;
        std::uint32_t* m_free_indices = nullptr;
        std::uint32_t m_capacity = 0;
        std::uint32_t m_alive_count = 0;
        std::uint32_t m_next_unused = 0;
        std::uint32_t m_free_count = 0;
    };
}
