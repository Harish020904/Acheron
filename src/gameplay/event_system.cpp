#include "event_system.h"
#include "../core/logger.h"
#include <cstdlib>

namespace acheron::gameplay
{
    void EventSystem::update(ecs::World& world, float dt)
    {
        m_timer += dt;
        // Randomly power things back on slowly
        if (m_timer > 2.0f)
        {
            m_timer = 0.0f;
            for (std::uint32_t i = 0; i < world.district_count(); ++i)
            {
                auto* d = world.district_slot(i);
                if (d && !d->powered)
                {
                    // 10% chance to restore power every 2 sec
                    if (rand() % 100 < 10)
                    {
                        d->powered = true;
                    }
                }
            }
        }
    }

    void EventSystem::trigger_blackout(ecs::World& world)
    {
        int count = 0;
        for (std::uint32_t i = 0; i < world.district_count(); ++i)
        {
            auto* d = world.district_slot(i);
            if (d && d->instability > 0.6f)
            {
                d->powered = false;
                count++;
            }
        }
        ACH_LOG_INFO("EventSystem: Triggered blackout on %d highly unstable districts", count);
    }

    void EventSystem::trigger_boom(ecs::World& world)
    {
        for (std::uint32_t i = 0; i < world.district_count(); ++i)
        {
            auto* d = world.district_slot(i);
            if (d && d->type == ecs::DistrictType::Commercial)
            {
                d->wealth += 300.0f;
            }
        }
        ACH_LOG_INFO("EventSystem: Commercial economic boom");
    }
}
