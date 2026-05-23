#include "interaction_system.h"
#include "../core/logger.h"
#include <cmath>

namespace acheron::gameplay
{
    void InteractionSystem::update(ecs::World& world, Vector2 world_mouse, float cell_px)
    {
        // 1. Resolve hovered district
        std::uint32_t grid_x = static_cast<std::uint32_t>(world_mouse.x / cell_px);
        std::uint32_t grid_y = static_cast<std::uint32_t>(world_mouse.y / cell_px);
        
        ecs::EntityID hovered_id{};
        if (world_mouse.x >= 0 && world_mouse.y >= 0 && grid_x < world.grid_width() && grid_y < world.grid_height())
        {
            auto* d = world.district_at(grid_x, grid_y);
            if (d) hovered_id = d->entity;
        }

        // 2. Selection and dragging
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            m_selected_district = hovered_id;
            m_drag_start_district = hovered_id;
            if (hovered_id.index != 0) {
                m_is_dragging = true;
            }
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
        {
            if (m_is_dragging && hovered_id.index != 0 && hovered_id.index != m_drag_start_district.index)
            {
                // Connect roads if adjacent
                auto* d1 = world.district(m_drag_start_district);
                auto* d2 = world.district(hovered_id);
                if (d1 && d2)
                {
                    int dx = std::abs((int)d1->grid_x - (int)d2->grid_x);
                    int dy = std::abs((int)d1->grid_y - (int)d2->grid_y);
                    if (dx + dy == 1) // Must be exactly orthogonal adjacent
                    {
                        if (world.connect_districts(m_drag_start_district, hovered_id, 1.0f))
                        {
                            ACH_LOG_INFO("Road built between %u and %u", m_drag_start_district.index, hovered_id.index);
                        }
                    }
                }
            }
            m_is_dragging = false;
            m_drag_start_district = {};
        }

        // 3. Spawning
        if (IsKeyPressed(KEY_D) && hovered_id.index == 0 && grid_x < world.grid_width() && grid_y < world.grid_height())
        {
            auto new_d = world.spawn_district(grid_x, grid_y);
            m_selected_district = new_d;
            ACH_LOG_INFO("Spawned district at %u, %u", grid_x, grid_y);
        }

        // 4. District operations (Zoning and Upgrading)
        auto* selected = world.district(m_selected_district);
        if (selected)
        {
            if (IsKeyPressed(KEY_Z))
            {
                // Cycle zone
                int current = static_cast<int>(selected->type);
                current = (current + 1) % 3; // Cycle Res->Com->Ind (Empty is ignored during cycle)
                selected->type = static_cast<ecs::DistrictType>(current);
                ACH_LOG_INFO("Zoned district %u to type %d", selected->entity.index, current);
            }
            if (IsKeyPressed(KEY_U))
            {
                // Upgrade
                if (selected->level < 4 && selected->wealth >= 500.0f)
                {
                    selected->level++;
                    selected->wealth -= 500.0f; // Consume wealth
                    ACH_LOG_INFO("Upgraded district %u to level %d", selected->entity.index, selected->level);
                }
            }
        }
    }
}
