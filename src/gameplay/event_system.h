#pragma once

#include "../ecs/world.h"

namespace acheron::gameplay
{
    class EventSystem final
    {
    public:
        void update(ecs::World& world, float dt);
        
        void trigger_blackout(ecs::World& world);
        void trigger_boom(ecs::World& world);
        
    private:
        float m_timer = 0.0f;
    };
}
