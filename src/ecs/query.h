#pragma once

#include "archetype.h"

namespace acheron::ecs
{
    static constexpr std::uint32_t MAX_QUERY_MATCHES = 128u;

    struct QueryChunkView
    {
        ArchetypeStorage* archetype = nullptr;
        ArchetypeChunk* chunk = nullptr;
        std::uint32_t entity_count = 0;

        void* column(ComponentTypeID id) noexcept
        {
            return (archetype && chunk) ? archetype->column_data(*chunk, id) : nullptr;
        }

        const void* column(ComponentTypeID id) const noexcept
        {
            return (archetype && chunk) ? archetype->column_data(*chunk, id) : nullptr;
        }
    };

    using QueryChunkFn = void (*)(QueryChunkView view, void* user) noexcept;

    class ArchetypeQuery final
    {
    public:
        ArchetypeQuery() noexcept = default;

        ArchetypeQuery(const ArchetypeQuery&) = delete;
        ArchetypeQuery& operator=(const ArchetypeQuery&) = delete;

        bool init(ArchetypeStorage** matches, std::uint32_t match_capacity) noexcept;
        void reset() noexcept;

        bool compile(ArchetypeStorage* const* archetypes,
                     std::uint32_t archetype_count,
                     ComponentSignature required) noexcept;

        bool next(QueryChunkView& out) noexcept;
        void rewind() noexcept;

        void for_each_chunk(QueryChunkFn fn, void* user) noexcept;

        ComponentSignature required_signature() const noexcept { return m_required; }
        std::uint32_t match_count() const noexcept { return m_match_count; }
        std::uint32_t chunk_count() const noexcept { return m_total_chunks; }

    private:
        ArchetypeStorage** m_matches = nullptr;
        std::uint32_t m_match_capacity = 0;
        std::uint32_t m_match_count = 0;
        std::uint32_t m_total_chunks = 0;
        std::uint32_t m_iter_archetype = 0;
        std::uint32_t m_iter_chunk = 0;
        ComponentSignature m_required{};
    };
}
