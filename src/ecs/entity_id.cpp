#include "entity_id.h"

#include <cstdio>

namespace acheron::ecs
{
    bool EntityTable::init(EntityRecord* records, std::uint32_t* free_indices, std::uint32_t capacity) noexcept
    {
        if (!records || !free_indices || capacity == 0u)
        {
            reset();
            return false;
        }

        m_records = records;
        m_free_indices = free_indices;
        m_capacity = capacity;
        m_alive_count = 0;
        m_next_unused = 0;
        m_free_count = 0;

        for (std::uint32_t i = 0; i < capacity; ++i)
        {
            m_records[i] = {};
            m_records[i].generation = FIRST_ENTITY_GENERATION;
            m_free_indices[i] = INVALID_ENTITY_INDEX;
        }

        return true;
    }

    void EntityTable::reset() noexcept
    {
        m_records = nullptr;
        m_free_indices = nullptr;
        m_capacity = 0;
        m_alive_count = 0;
        m_next_unused = 0;
        m_free_count = 0;
    }

    void EntityTable::clear() noexcept
    {
        m_alive_count = 0;
        m_next_unused = 0;
        m_free_count = 0;
        for (std::uint32_t i = 0; i < m_capacity; ++i)
        {
            m_records[i].alive = false;
            m_records[i].location = {};
            // DO NOT reset generation, to prevent stale handles from matching
            m_free_indices[i] = INVALID_ENTITY_INDEX;
        }
    }

    EntityID EntityTable::create() noexcept
    {
        if (!m_records)
        {
            return {};
        }

        std::uint32_t index = INVALID_ENTITY_INDEX;
        if (m_free_count != 0u)
        {
            --m_free_count;
            index = m_free_indices[m_free_count];
            m_free_indices[m_free_count] = INVALID_ENTITY_INDEX;
        }
        else
        {
            if (m_next_unused >= m_capacity)
            {
                return {};
            }
            index = m_next_unused;
            ++m_next_unused;
        }

        EntityRecord& record = m_records[index];
        record.alive = true;
        record.location = {};
        ++m_alive_count;

        return EntityID{ index, record.generation };
    }

    bool EntityTable::destroy(EntityID id) noexcept
    {
        if (!is_alive(id))
        {
            return false;
        }

        EntityRecord& record = m_records[id.index];
        record.alive = false;
        record.location = {};

        std::uint32_t next_generation = record.generation + 1u;
        if (next_generation == INVALID_ENTITY_GENERATION || next_generation == 0u)
        {
            next_generation = FIRST_ENTITY_GENERATION;
        }
        record.generation = next_generation;

        if (m_free_count < m_capacity)
        {
            m_free_indices[m_free_count] = id.index;
            ++m_free_count;
        }

        if (m_alive_count != 0u)
        {
            --m_alive_count;
        }

        return true;
    }

    bool EntityTable::is_alive(EntityID id) const noexcept
    {
        if (!m_records || id.index >= m_capacity || id.generation == INVALID_ENTITY_GENERATION)
        {
            return false;
        }

        const EntityRecord& record = m_records[id.index];
        return record.alive && record.generation == id.generation;
    }

    EntityRecord* EntityTable::record(EntityID id) noexcept
    {
        if (!is_alive(id))
        {
            return nullptr;
        }
        return &m_records[id.index];
    }

    const EntityRecord* EntityTable::record(EntityID id) const noexcept
    {
        if (!is_alive(id))
        {
            return nullptr;
        }
        return &m_records[id.index];
    }

    bool EntityTable::set_location(EntityID id, EntityLocation location) noexcept
    {
        EntityRecord* rec = record(id);
        if (!rec)
        {
            return false;
        }

        rec->location = location;
        return true;
    }

    EntityLocation EntityTable::location(EntityID id) const noexcept
    {
        const EntityRecord* rec = record(id);
        return rec ? rec->location : EntityLocation{};
    }
}

#if defined(ACHERON_ENTITY_ID_UNIT_TEST)

namespace
{
    static bool stale_handle_test() noexcept
    {
        acheron::ecs::EntityRecord records[8]{};
        std::uint32_t free_indices[8]{};
        acheron::ecs::EntityTable table;

        if (!table.init(records, free_indices, 8))
        {
            return false;
        }

        const acheron::ecs::EntityID a = table.create();
        if (!is_valid(a) || !table.is_alive(a))
        {
            return false;
        }

        if (!table.destroy(a) || table.is_alive(a))
        {
            return false;
        }

        const acheron::ecs::EntityID b = table.create();
        if (b.index != a.index)
        {
            return false;
        }
        if (b.generation == a.generation)
        {
            return false;
        }
        if (table.is_alive(a) || !table.is_alive(b))
        {
            return false;
        }

        return true;
    }

    static bool reuse_correctness_test() noexcept
    {
        acheron::ecs::EntityRecord records[4]{};
        std::uint32_t free_indices[4]{};
        acheron::ecs::EntityTable table;
        if (!table.init(records, free_indices, 4))
        {
            return false;
        }

        const acheron::ecs::EntityID ids[4] = {
            table.create(),
            table.create(),
            table.create(),
            table.create(),
        };

        if (is_valid(table.create()))
        {
            return false;
        }

        if (!table.destroy(ids[1]) || !table.destroy(ids[3]))
        {
            return false;
        }

        const acheron::ecs::EntityID r0 = table.create();
        const acheron::ecs::EntityID r1 = table.create();

        if (r0.index != ids[3].index || r1.index != ids[1].index)
        {
            return false;
        }
        if (r0.generation == ids[3].generation || r1.generation == ids[1].generation)
        {
            return false;
        }

        acheron::ecs::EntityLocation loc{};
        loc.archetype_index = 7;
        loc.chunk_index = 2;
        loc.row = 11;
        if (!table.set_location(r0, loc))
        {
            return false;
        }

        const acheron::ecs::EntityLocation read = table.location(r0);
        return read.archetype_index == 7u && read.chunk_index == 2u && read.row == 11u;
    }
}

int main()
{
    if (!stale_handle_test())
    {
        std::printf("[entity_id] stale handle: FAIL\n");
        return 1;
    }
    std::printf("[entity_id] stale handle: OK\n");

    if (!reuse_correctness_test())
    {
        std::printf("[entity_id] reuse correctness: FAIL\n");
        return 1;
    }
    std::printf("[entity_id] reuse correctness: OK\n");
    return 0;
}

#endif
