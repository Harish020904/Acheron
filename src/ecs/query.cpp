#include "query.h"

#include <chrono>
#include <cstdio>

namespace acheron::ecs
{
    bool ArchetypeQuery::init(ArchetypeStorage** matches, std::uint32_t match_capacity) noexcept
    {
        if (!matches || match_capacity == 0u)
        {
            reset();
            return false;
        }

        m_matches = matches;
        m_match_capacity = match_capacity;
        m_match_count = 0;
        m_total_chunks = 0;
        m_iter_archetype = 0;
        m_iter_chunk = 0;
        m_required = {};

        for (std::uint32_t i = 0; i < match_capacity; ++i)
        {
            m_matches[i] = nullptr;
        }

        return true;
    }

    void ArchetypeQuery::reset() noexcept
    {
        m_matches = nullptr;
        m_match_capacity = 0;
        m_match_count = 0;
        m_total_chunks = 0;
        m_iter_archetype = 0;
        m_iter_chunk = 0;
        m_required = {};
    }

    bool ArchetypeQuery::compile(ArchetypeStorage* const* archetypes,
                                 std::uint32_t archetype_count,
                                 ComponentSignature required) noexcept
    {
        if (!m_matches || (!archetypes && archetype_count != 0u))
        {
            return false;
        }

        m_match_count = 0;
        m_total_chunks = 0;
        m_iter_archetype = 0;
        m_iter_chunk = 0;
        m_required = required;

        for (std::uint32_t i = 0; i < m_match_capacity; ++i)
        {
            m_matches[i] = nullptr;
        }

        for (std::uint32_t i = 0; i < archetype_count; ++i)
        {
            ArchetypeStorage* archetype = archetypes[i];
            if (!archetype)
            {
                continue;
            }

            if (!signature_includes(archetype->signature(), required))
            {
                continue;
            }

            if (m_match_count >= m_match_capacity)
            {
                return false;
            }

            m_matches[m_match_count] = archetype;
            ++m_match_count;
            m_total_chunks += archetype->chunk_count();
        }

        return true;
    }

    bool ArchetypeQuery::next(QueryChunkView& out) noexcept
    {
        out = {};

        while (m_iter_archetype < m_match_count)
        {
            ArchetypeStorage* archetype = m_matches[m_iter_archetype];
            if (!archetype)
            {
                ++m_iter_archetype;
                m_iter_chunk = 0;
                continue;
            }

            if (m_iter_chunk < archetype->chunk_count())
            {
                ArchetypeChunk* chunk = archetype->chunk(m_iter_chunk);
                ++m_iter_chunk;
                if (chunk && chunk->entity_count != 0u)
                {
                    out.archetype = archetype;
                    out.chunk = chunk;
                    out.entity_count = chunk->entity_count;
                    return true;
                }
                continue;
            }

            ++m_iter_archetype;
            m_iter_chunk = 0;
        }

        return false;
    }

    void ArchetypeQuery::rewind() noexcept
    {
        m_iter_archetype = 0;
        m_iter_chunk = 0;
    }

    void ArchetypeQuery::for_each_chunk(QueryChunkFn fn, void* user) noexcept
    {
        if (!fn)
        {
            return;
        }

        rewind();
        QueryChunkView view{};
        while (next(view))
        {
            fn(view, user);
        }
    }
}

#if defined(ACHERON_QUERY_UNIT_TEST)

namespace query_test
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

    struct Health
    {
        std::uint32_t value;
    };
}

template <>
struct acheron::ecs::ComponentTraits<query_test::Position>
{
    static constexpr acheron::ecs::ComponentTypeID id = 0;
    static constexpr const char* name = "query_test::Position";
};

template <>
struct acheron::ecs::ComponentTraits<query_test::Velocity>
{
    static constexpr acheron::ecs::ComponentTypeID id = 1;
    static constexpr const char* name = "query_test::Velocity";
};

template <>
struct acheron::ecs::ComponentTraits<query_test::Health>
{
    static constexpr acheron::ecs::ComponentTypeID id = 2;
    static constexpr const char* name = "query_test::Health";
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

    static bool fill_storage(acheron::ecs::ArchetypeStorage& storage, std::uint32_t count) noexcept
    {
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto slot = storage.allocate(acheron::ecs::EntityID{ i + 1u, 1u });
            if (!is_valid(slot))
            {
                return false;
            }

            if (storage.contains(0))
            {
                const query_test::Position pos{ static_cast<float>(i), 0.0f, 0.0f, 1.0f };
                (void)storage.set_component(*slot.chunk, slot.row, 0, &pos, sizeof(pos));
            }
            if (storage.contains(1))
            {
                const query_test::Velocity vel{ 1.0f, 0.0f, 0.0f, 0.0f };
                (void)storage.set_component(*slot.chunk, slot.row, 1, &vel, sizeof(vel));
            }
            if (storage.contains(2))
            {
                const query_test::Health hp{ 100u };
                (void)storage.set_component(*slot.chunk, slot.row, 2, &hp, sizeof(hp));
            }
        }
        return true;
    }

    static bool query_benchmark() noexcept
    {
        acheron::ecs::ComponentRegistry registry;
        if (!registry.register_component<query_test::Position>() ||
            !registry.register_component<query_test::Velocity>() ||
            !registry.register_component<query_test::Health>())
        {
            return false;
        }

        alignas(64) acheron::ecs::ArchetypeChunk chunks_a[4]{};
        alignas(64) acheron::ecs::ArchetypeChunk chunks_b[4]{};
        alignas(64) acheron::ecs::ArchetypeChunk chunks_c[4]{};
        alignas(64) std::byte memory_a[4 * acheron::ecs::ECS_CHUNK_BYTES]{};
        alignas(64) std::byte memory_b[4 * acheron::ecs::ECS_CHUNK_BYTES]{};
        alignas(64) std::byte memory_c[4 * acheron::ecs::ECS_CHUNK_BYTES]{};

        acheron::ecs::ArchetypeStorage a;
        acheron::ecs::ArchetypeStorage b;
        acheron::ecs::ArchetypeStorage c;

        acheron::ecs::ComponentSignature pos_vel = acheron::ecs::signature_add(acheron::ecs::component_bit(0), 1);
        acheron::ecs::ComponentSignature pos_health = acheron::ecs::signature_add(acheron::ecs::component_bit(0), 2);
        acheron::ecs::ComponentSignature all = acheron::ecs::signature_add(pos_vel, 2);

        if (!make_storage(a, registry, pos_vel, 0, chunks_a, memory_a, 4) ||
            !make_storage(b, registry, pos_health, 1, chunks_b, memory_b, 4) ||
            !make_storage(c, registry, all, 2, chunks_c, memory_c, 4))
        {
            return false;
        }

        if (!fill_storage(a, a.entity_capacity_per_chunk() * 2u) ||
            !fill_storage(b, b.entity_capacity_per_chunk()) ||
            !fill_storage(c, c.entity_capacity_per_chunk() * 2u))
        {
            return false;
        }

        acheron::ecs::ArchetypeStorage* archetypes[3] = { &a, &b, &c };
        acheron::ecs::ArchetypeStorage* matches[3]{};
        acheron::ecs::ArchetypeQuery query;
        if (!query.init(matches, 3))
        {
            return false;
        }

        const acheron::ecs::ComponentSignature required = pos_vel;
        if (!query.compile(archetypes, 3, required))
        {
            return false;
        }

        if (query.match_count() != 2u)
        {
            return false;
        }

        volatile float sink = 0.0f;
        const auto begin = std::chrono::steady_clock::now();
        for (std::uint32_t pass = 0; pass < 256; ++pass)
        {
            query.rewind();
            acheron::ecs::QueryChunkView view{};
            while (query.next(view))
            {
                const auto* pos = static_cast<const query_test::Position*>(view.column(0));
                const auto* vel = static_cast<const query_test::Velocity*>(view.column(1));
                if (!pos || !vel)
                {
                    return false;
                }

                for (std::uint32_t i = 0; i < view.entity_count; i += 4)
                {
                    const std::uint32_t lane_count = ((i + 4u) <= view.entity_count) ? 4u : (view.entity_count - i);
                    for (std::uint32_t lane = 0; lane < lane_count; ++lane)
                    {
                        sink += pos[i + lane].x + vel[i + lane].x;
                    }
                }
            }
        }
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        std::printf("[query] traversal %u chunks x256: %lld us\n",
                    query.chunk_count(),
                    static_cast<long long>(elapsed_us));
        return sink > 0.0f;
    }

    static bool traversal_profiling_validation() noexcept
    {
        return query_benchmark();
    }
}

int main()
{
    if (!query_benchmark())
    {
        std::printf("[query] benchmark: FAIL\n");
        return 1;
    }
    std::printf("[query] benchmark: OK\n");

    if (!traversal_profiling_validation())
    {
        std::printf("[query] traversal profiling: FAIL\n");
        return 1;
    }
    std::printf("[query] traversal profiling: OK\n");
    return 0;
}

#endif
