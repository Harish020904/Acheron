#pragma once

#include <cstdint>
#include <cstring>

namespace acheron::simulation::citizens
{
    // Citizen daily routine state
    enum class CitizenState : uint8_t
    {
        Idle        = 0,
        MovingToWork,
        AtWork,
        MovingHome,
        Visiting,
        Emergency,
    };

    // Citizen mood/satisfaction
    enum class CitizenMood : uint8_t
    {
        Happy    = 0,
        Content,
        Stressed,
        Unhappy,
    };

    // Citizen occupation (affects color coding and routine)
    enum class CitizenOccupation : uint8_t
    {
        Worker    = 0,   // Blue — commutes to office
        Student   = 1,   // Green — commutes to school/uni
        Medic     = 2,   // Red — hospital worker
        Service   = 3,   // Yellow — restaurant/gym worker
        Retired   = 4,   // Gray — random walker
    };

    // A single citizen agent (48 bytes)
    struct Citizen
    {
        float           x;           // Current world X position
        float           y;           // Current world Y position
        float           dest_x;      // Destination world X
        float           dest_y;      // Destination world Y
        float           home_x;      // Home location X
        float           home_y;      // Home location Y
        float           work_x;      // Work/school location X
        float           work_y;      // Work/school location Y
        float           speed;       // World units per second
        float           routine_timer; // Time accumulator for daily routine
        CitizenState    state;
        CitizenMood     mood;
        CitizenOccupation occupation;
        uint8_t         id_low;      // Lower 8 bits of unique ID (for color variety)
        uint8_t         _pad[2];
        uint32_t        uid;         // Unique citizen ID
    };

    struct CitizenSystemConfig
    {
        uint32_t max_citizens    = 1500;
        float    sim_day_seconds = 120.0f;  // A sim day = 2 real minutes
        float    base_speed      = 80.0f;   // World pixels per second
        float    congestion_slowdown = 0.5f; // Factor reduction at max congestion
    };

    class CitizenSystem final
    {
    public:
        bool initialize(const CitizenSystemConfig& config) noexcept;
        void shutdown() noexcept;

        // Spawn a citizen at a given world position
        uint32_t spawn_citizen(float home_x, float home_y, float work_x, float work_y,
                               CitizenOccupation occ) noexcept;

        // Fixed-step update — advances all citizen positions and state machines
        void update(float dt, float global_congestion) noexcept;

        // Read-only access for rendering
        const Citizen* citizen_slot(uint32_t index) const noexcept;
        uint32_t citizen_count() const noexcept { return m_citizen_count; }

        // Locate a citizen for camera focus
        const Citizen* find_citizen(uint32_t uid) const noexcept;

        bool is_initialized() const noexcept { return m_citizens != nullptr; }

        ~CitizenSystem() noexcept;

    private:
        Citizen*              m_citizens     = nullptr;
        CitizenSystemConfig   m_config       = {};
        uint32_t              m_citizen_count = 0;
        uint32_t              m_next_uid      = 1;

        void _advance_routine(Citizen& c, float dt) noexcept;
        void _move_toward_dest(Citizen& c, float dt, float congestion_factor) noexcept;
        CitizenMood _compute_mood(const Citizen& c, float congestion) const noexcept;
    };

} // namespace acheron::simulation::citizens
