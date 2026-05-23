#include "road_renderer.h"
#include <cmath>
#include <algorithm>

namespace acheron::renderer
{
    Color RoadRenderer::mix_color(Color a, Color b, float t) const
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return Color{
            static_cast<unsigned char>(a.r + (b.r - a.r) * t),
            static_cast<unsigned char>(a.g + (b.g - a.g) * t),
            static_cast<unsigned char>(a.b + (b.b - a.b) * t),
            255
        };
    }

    void RoadRenderer::render_roads(const ecs::World& world, float cell_px, int overlay_mode)
    {
        for (std::uint32_t i = 0; i < world.road_count(); ++i)
        {
            const auto* road = world.road_slot(i);
            if (!road) continue;

            const auto* from = world.district(road->from);
            const auto* to = world.district(road->to);
            if (!from || !to) continue;

            Vector2 start = {
                from->grid_x * cell_px + cell_px * 0.5f,
                from->grid_y * cell_px + cell_px * 0.5f
            };
            Vector2 end = {
                to->grid_x * cell_px + cell_px * 0.5f,
                to->grid_y * cell_px + cell_px * 0.5f
            };
            
            // Road base
            DrawLineEx(start, end, 8.0f, Color{20, 30, 40, 255}); // Dark asphalt base
            
            if (overlay_mode != 1) // 1 == Traffic
            {
                // In non-traffic modes, draw basic neon line
                DrawLineEx(start, end, 4.0f, palette::stable_blue);
                continue;
            }

            // Traffic mode: Congestion color
            Color roadColor = palette::traffic_flow;
            if (road->congestion > 0.4f) roadColor = palette::traffic_warn;
            if (road->congestion > 0.8f) roadColor = palette::traffic_heavy;

            // Neon line
            float thickness = 2.0f + road->congestion * 3.0f;
            DrawLineEx(start, end, thickness, roadColor);
            
            // Flow pulse
            float time = GetTime();
            float pulse = std::sin(time * 5.0f - road->congestion * 10.0f) * 0.5f + 0.5f;
            
            // Severe congestion flash
            if (road->congestion > 0.9f)
            {
                pulse = 0.7f + 0.3f * std::sin(time * 20.0f);
            }

            DrawLineEx(start, end, thickness + 2.0f, Fade(roadColor, 0.3f * pulse));
            
            // Draw moving vehicles
            if (road->congestion > 0.05f)
            {
                int vehicle_count = static_cast<int>(road->congestion * 8.0f);
                for (int v = 0; v < vehicle_count; ++v)
                {
                    float offset = std::fmod(time * (1.0f - road->congestion * 0.5f) * 0.5f + (v * 0.15f), 1.0f);
                    Vector2 pos = {
                        start.x + (end.x - start.x) * offset,
                        start.y + (end.y - start.y) * offset
                    };
                    DrawCircleV(pos, 2.0f, WHITE);
                }
            }
        }
    }
    
    void RoadRenderer::render_traffic(const ecs::World& world, const TextureCache& cache, float cell_px)
    {
        Texture2D carTex = cache.get_texture("vehicle_50");
        float time = GetTime();
        
        for (std::uint32_t i = 0; i < world.road_count(); ++i)
        {
            const auto* road = world.road_slot(i);
            if (!road || road->congestion < 0.1f) continue;
            
            const auto* from = world.district(road->from);
            const auto* to = world.district(road->to);
            if (!from || !to) continue;
            
            Vector2 start = { from->grid_x * cell_px + cell_px * 0.5f, from->grid_y * cell_px + cell_px * 0.5f };
            Vector2 end = { to->grid_x * cell_px + cell_px * 0.5f, to->grid_y * cell_px + cell_px * 0.5f };
            
            // Number of cars depends on congestion
            int car_count = static_cast<int>(road->congestion * 5.0f);
            for (int c = 0; c < car_count; ++c)
            {
                // Stagger cars along the road
                float t = std::fmod(time * (0.5f - road->congestion * 0.2f) + (c * 0.2f), 1.0f);
                Vector2 pos = {
                    start.x + (end.x - start.x) * t,
                    start.y + (end.y - start.y) * t
                };
                
                if (carTex.id != 0) {
                    Rectangle src = {0, 0, (float)carTex.width, (float)carTex.height};
                    Rectangle dest = {pos.x, pos.y, 16.0f, 16.0f};
                    Vector2 origin = {8.0f, 8.0f};
                    float rot = std::atan2(end.y - start.y, end.x - start.x) * (180.0f / PI);
                    DrawTexturePro(carTex, src, dest, origin, rot, WHITE);
                } else {
                    DrawCircleV(pos, 3.0f, RAYWHITE);
                }
            }
        }
    }
}
