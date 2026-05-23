#include "minimap_panel.h"
#include <algorithm>

namespace acheron::ui
{
    void MinimapPanel::render(int screen_width, const ecs::World& world)
    {
        float minimap_size = 150.0f;
        Rectangle panel = { static_cast<float>(screen_width) - minimap_size - 20.0f, 280.0f, minimap_size, minimap_size };
        
        // Background
        DrawRectangleRec(panel, Fade(renderer::palette::ui_panel_bg, 0.8f));
        DrawRectangleLinesEx(panel, 1.0f, renderer::palette::ui_border);
        
        float cell_w = minimap_size / static_cast<float>(world.grid_width());
        float cell_h = minimap_size / static_cast<float>(world.grid_height());

        // Draw roads first
        for (std::uint32_t i = 0; i < world.road_count(); ++i)
        {
            const auto* r = world.road_slot(i);
            if (!r) continue;
            auto* d1 = world.district(r->from);
            auto* d2 = world.district(r->to);
            if (d1 && d2)
            {
                Vector2 p1 = { panel.x + d1->grid_x * cell_w + cell_w*0.5f, panel.y + d1->grid_y * cell_h + cell_h*0.5f };
                Vector2 p2 = { panel.x + d2->grid_x * cell_w + cell_w*0.5f, panel.y + d2->grid_y * cell_h + cell_h*0.5f };
                DrawLineEx(p1, p2, 1.5f, renderer::palette::traffic_flow);
            }
        }
        
        // Draw districts
        for (std::uint32_t i = 0; i < world.district_count(); ++i)
        {
            const auto* d = world.district_slot(i);
            if (d)
            {
                Rectangle c = { panel.x + d->grid_x * cell_w + 1, panel.y + d->grid_y * cell_h + 1, cell_w - 2, cell_h - 2 };
                Color f = renderer::palette::stable_blue;
                if (!d->powered) f = DARKGRAY;
                else if (d->instability > 0.7f) f = RED;
                else if (d->type == ecs::DistrictType::Commercial) f = BLUE;
                else if (d->type == ecs::DistrictType::Industrial) f = YELLOW;
                
                DrawRectangleRec(c, f);
            }
        }
    }
}
