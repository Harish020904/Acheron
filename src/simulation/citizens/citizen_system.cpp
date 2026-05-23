#include "citizen_system.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace acheron::simulation::citizens
{
    bool CitizenSystem::initialize(const CitizenSystemConfig& config) noexcept
    {
        shutdown();
        m_config = config;
        m_citizens = new Citizen[config.max_citizens]{};
        if (!m_citizens) return false;
        m_citizen_count = 0;
        m_next_uid = 1;
        return true;
    }

    void CitizenSystem::shutdown() noexcept
    {
        delete[] m_citizens;
        m_citizens = nullptr;
        m_citizen_count = 0;
        m_next_uid = 1;
    }

    uint32_t CitizenSystem::spawn_citizen(float home_x, float home_y, float work_x, float work_y,
                                          CitizenOccupation occ) noexcept
    {
        if (!m_citizens || m_citizen_count >= m_config.max_citizens) return 0;

        Citizen& c = m_citizens[m_citizen_count++];
        c = {};
        c.uid         = m_next_uid++;
        c.id_low      = static_cast<uint8_t>(c.uid & 0xFF);
        c.home_x      = home_x;
        c.home_y      = home_y;
        c.work_x      = work_x;
        c.work_y      = work_y;
        c.x           = home_x;
        c.y           = home_y;
        c.dest_x      = work_x;
        c.dest_y      = work_y;
        c.occupation  = occ;
        c.state       = CitizenState::MovingToWork;
        c.mood        = CitizenMood::Content;
        c.speed       = m_config.base_speed * (0.8f + (c.id_low % 40) * 0.01f); // Slight speed variety
        // Stagger start time so they don't all depart at once
        c.routine_timer = (static_cast<float>(c.uid % 100) / 100.0f) * m_config.sim_day_seconds;
        return c.uid;
    }

    void CitizenSystem::update(float dt, float global_congestion) noexcept
    {
        if (!m_citizens) return;

        float congestion_factor = 1.0f - (global_congestion * m_config.congestion_slowdown);
        if (congestion_factor < 0.2f) congestion_factor = 0.2f;

        for (uint32_t i = 0; i < m_citizen_count; ++i)
        {
            Citizen& c = m_citizens[i];
            _advance_routine(c, dt);
            _move_toward_dest(c, dt, congestion_factor);
            c.mood = _compute_mood(c, global_congestion);
        }
    }

    void CitizenSystem::_advance_routine(Citizen& c, float dt) noexcept
    {
        c.routine_timer += dt;
        const float half_day  = m_config.sim_day_seconds * 0.5f;
        const float work_hold = m_config.sim_day_seconds * 0.35f; // Stay at work 35% of day

        switch (c.state)
        {
            case CitizenState::MovingToWork:
            {
                // Check if arrived at work
                float dx = c.dest_x - c.x;
                float dy = c.dest_y - c.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist < 8.0f)
                {
                    c.x = c.dest_x;
                    c.y = c.dest_y;
                    c.state = CitizenState::AtWork;
                    c.routine_timer = 0.0f;
                }
                break;
            }
            case CitizenState::AtWork:
            {
                if (c.routine_timer >= work_hold)
                {
                    c.state = CitizenState::MovingHome;
                    c.dest_x = c.home_x;
                    c.dest_y = c.home_y;
                    c.routine_timer = 0.0f;
                }
                break;
            }
            case CitizenState::MovingHome:
            {
                float dx = c.dest_x - c.x;
                float dy = c.dest_y - c.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist < 8.0f)
                {
                    c.x = c.dest_x;
                    c.y = c.dest_y;
                    c.state = CitizenState::Idle;
                    c.routine_timer = 0.0f;
                }
                break;
            }
            case CitizenState::Idle:
            {
                if (c.routine_timer >= half_day)
                {
                    // Start next cycle
                    c.state = CitizenState::MovingToWork;
                    c.dest_x = c.work_x;
                    c.dest_y = c.work_y;
                    c.routine_timer = 0.0f;
                }
                break;
            }
            default:
                break;
        }
    }

    void CitizenSystem::_move_toward_dest(Citizen& c, float dt, float congestion_factor) noexcept
    {
        if (c.state == CitizenState::AtWork || c.state == CitizenState::Idle) return;

        float dx = c.dest_x - c.x;
        float dy = c.dest_y - c.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < 1.0f) return;

        float move = c.speed * congestion_factor * dt;
        if (move >= dist) move = dist;

        float nx = dx / dist;
        float ny = dy / dist;
        c.x += nx * move;
        c.y += ny * move;
    }

    CitizenMood CitizenSystem::_compute_mood(const Citizen& c, float congestion) const noexcept
    {
        if (congestion > 0.85f) return CitizenMood::Unhappy;
        if (congestion > 0.60f) return CitizenMood::Stressed;
        if (c.state == CitizenState::AtWork) return CitizenMood::Content;
        return CitizenMood::Happy;
    }

    const Citizen* CitizenSystem::citizen_slot(uint32_t index) const noexcept
    {
        if (!m_citizens || index >= m_citizen_count) return nullptr;
        return &m_citizens[index];
    }

    const Citizen* CitizenSystem::find_citizen(uint32_t uid) const noexcept
    {
        if (!m_citizens) return nullptr;
        for (uint32_t i = 0; i < m_citizen_count; ++i)
        {
            if (m_citizens[i].uid == uid) return &m_citizens[i];
        }
        return nullptr;
    }

    CitizenSystem::~CitizenSystem() noexcept
    {
        shutdown();
    }

} // namespace acheron::simulation::citizens
