#pragma once

#include "raylib.h"
#include "../ecs/world.h"

namespace acheron::gameplay
{
    class InteractionSystem final
    {
    public:
        void update(ecs::World& world, Vector2 world_mouse, float cell_px);
        
        ecs::EntityID get_selected_district() const { return m_selected_district; }
        
    private:
        ecs::EntityID m_selected_district{};
        ecs::EntityID m_drag_start_district{};
        bool m_is_dragging = false;
    };
}
