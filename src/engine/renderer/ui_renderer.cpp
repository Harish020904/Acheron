#include "ui_renderer.h"
#include "visual_palette.h"
#include <algorithm>
#include <cmath>

namespace acheron::renderer
{
    void UIRenderer::init()
    {
        for (int i=0; i<100; ++i) m_ft_history[i] = 16.0f;
    }

    void UIRenderer::render(
        const TextureCache& cache, int sw, int sh,
        const ecs::World& world,
        const simulation::traffic::TrafficSystem& traffic,
        const simulation::economy::EconomySystem& economy,
        const gameplay::progression::ProgressionSystem& progression,
        int overlay_mode, int sim_speed, ecs::EntityID selected_district,
        bool is_paused, bool show_profiler)
    {
        (void)traffic;
        (void)economy;
        (void)progression;
        Font font = cache.get_font("orbitron_regular");
        
        // 1. Top-left HUD (Metrics)
        DrawRectangleRec(Rectangle{0, 0, 250, 160}, Fade(palette::ui_panel_bg, 0.8f));
        DrawRectangleLinesEx(Rectangle{0, 0, 250, 160}, 1.0f, palette::ui_border);
        
        DrawTextEx(font, "ACHERON OS v0.9", Vector2{10, 10}, 16, 1, palette::ui_accent);
        DrawTextEx(font, TextFormat("FPS: %d", GetFPS()), Vector2{10, 40}, 14, 1, palette::ui_text_main);
        DrawTextEx(font, TextFormat("Districts: %u", world.district_count()), Vector2{10, 60}, 14, 1, palette::ui_text_main);
        DrawTextEx(font, TextFormat("Roads: %u", world.road_count()), Vector2{10, 80}, 14, 1, palette::ui_text_main);
        DrawTextEx(font, TextFormat("Sim Speed: %dx", sim_speed), Vector2{10, 100}, 14, 1, palette::ui_text_main);
        
        if (is_paused)
        {
            DrawTextEx(font, "PAUSED", Vector2{10, 120}, 16, 1, RED);
        }
        
        const char* overlay_names[] = {"NONE", "TRAFFIC", "ECONOMY", "INSTABILITY", "PROGRESSION", "ZONING"};
        DrawTextEx(font, TextFormat("OVERLAY: %s", overlay_names[overlay_mode]), Vector2{10, 140}, 14, 1, palette::ui_accent);

        // 2. Profiler Overlay (Bottom Left)
        if (show_profiler)
        {
            float current_ft = GetFrameTime() * 1000.0f;
            m_ft_history[m_ft_index] = current_ft;
            m_ft_index = (m_ft_index + 1) % 100;

            Rectangle prof_rec = {0, (float)sh - 120, 300, 120};
            DrawRectangleRec(prof_rec, Fade(palette::ui_panel_bg, 0.8f));
            DrawRectangleLinesEx(prof_rec, 1.0f, palette::ui_border);
            
            DrawTextEx(font, "PROFILER", Vector2{10, prof_rec.y + 10}, 14, 1, palette::ui_accent);
            DrawTextEx(font, TextFormat("FT: %.2fms", current_ft), Vector2{10, prof_rec.y + 30}, 14, 1, palette::ui_text_main);

            float graph_w = 120, graph_h = 60;
            float graph_x = 160, graph_y = prof_rec.y + 30;
            DrawRectangleLines(graph_x, graph_y, graph_w, graph_h, DARKGRAY);
            for (int i = 0; i < 99; ++i)
            {
                int idx1 = (m_ft_index + i) % 100;
                int idx2 = (m_ft_index + i + 1) % 100;
                float h1 = std::clamp(m_ft_history[idx1] / 33.0f, 0.0f, 1.0f) * graph_h;
                float h2 = std::clamp(m_ft_history[idx2] / 33.0f, 0.0f, 1.0f) * graph_h;
                DrawLineEx(Vector2{graph_x + (i/99.0f)*graph_w, graph_y + graph_h - h1}, 
                           Vector2{graph_x + ((i+1)/99.0f)*graph_w, graph_y + graph_h - h2}, 
                           1.0f, palette::ui_accent);
            }
        }

        // 3. District Info (Bottom Right)
        if (selected_district.index != 0)
        {
            auto* d = world.district(selected_district);
            if (d)
            {
                Rectangle info_rec = {(float)sw - 260, (float)sh - 160, 260, 160};
                DrawRectangleRec(info_rec, Fade(palette::ui_panel_bg, 0.8f));
                DrawRectangleLinesEx(info_rec, 1.0f, palette::ui_border);
                
                const char* type_str = "Unknown";
                if (d->type == ecs::DistrictType::Residential) type_str = "Residential";
                if (d->type == ecs::DistrictType::Commercial) type_str = "Commercial";
                if (d->type == ecs::DistrictType::Industrial) type_str = "Industrial";
                if (d->type == ecs::DistrictType::Empty) type_str = "Empty";

                DrawTextEx(font, TextFormat("DISTRICT %u", d->entity.index), Vector2{info_rec.x + 10, info_rec.y + 10}, 16, 1, palette::ui_accent);
                DrawTextEx(font, TextFormat("Type: %s", type_str), Vector2{info_rec.x + 10, info_rec.y + 35}, 14, 1, WHITE);
                DrawTextEx(font, TextFormat("Level: %d", d->level), Vector2{info_rec.x + 10, info_rec.y + 55}, 14, 1, WHITE);
                DrawTextEx(font, TextFormat("Wealth: %.0f", d->wealth), Vector2{info_rec.x + 10, info_rec.y + 75}, 14, 1, GREEN);
                DrawTextEx(font, TextFormat("Instability: %.2f", d->instability), Vector2{info_rec.x + 10, info_rec.y + 95}, 14, 1, d->instability > 0.6f ? RED : WHITE);
                DrawTextEx(font, TextFormat("Power: %s", d->powered ? "ONLINE" : "OFFLINE"), Vector2{info_rec.x + 10, info_rec.y + 115}, 14, 1, d->powered ? GREEN : RED);
            }
        }
    }
}
