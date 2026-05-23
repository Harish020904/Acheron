#include "overlay_renderer.h"
#include "visual_palette.h"
#include <cmath>
#include <algorithm>

namespace acheron::renderer
{
    void OverlayRenderer::render(const ecs::World& world, float cell_px, OverlayMode mode)
    {
        float time = GetTime();

        // Background scrolling haze
        for (int i = 0; i < 50; ++i)
        {
            float y = std::fmod(i * 40.0f + time * 20.0f, 2000.0f);
            DrawLineEx(Vector2{0, y}, Vector2{2000, y}, 2.0f, Fade(palette::grid_lines, 0.1f));
        }

        if (mode == OverlayMode::None || mode == OverlayMode::Traffic) 
        {
            // Draw Industrial Smoke
            for (std::uint32_t i = 0; i < world.district_count(); ++i)
            {
                const auto* district = world.district_slot(i);
                if (district && district->type == ecs::DistrictType::Industrial && district->powered)
                {
                    float sx = district->grid_x * cell_px + cell_px / 2.0f;
                    float sy = district->grid_y * cell_px + cell_px / 2.0f;
                    
                    for (int s = 0; s < 3; ++s)
                    {
                        float offset_y = std::fmod(time * 15.0f + s * 20.0f, 60.0f);
                        float alpha = 1.0f - (offset_y / 60.0f);
                        DrawRectangle(sx - 10, sy - offset_y, 20, 15, Fade(DARKGRAY, alpha * 0.5f));
                    }
                }
            }
            return;
        }
        
        for (std::uint32_t i = 0; i < world.district_count(); ++i)
        {
            const auto* district = world.district_slot(i);
            if (!district) continue;

            Rectangle destRec = {
                district->grid_x * cell_px,
                district->grid_y * cell_px,
                cell_px,
                cell_px
            };

            if (mode == OverlayMode::Zoning)
            {
                Color zoneColor = Fade(GRAY, 0.3f);
                if (district->type == ecs::DistrictType::Residential) zoneColor = Fade(GREEN, 0.4f);
                else if (district->type == ecs::DistrictType::Commercial) zoneColor = Fade(BLUE, 0.4f);
                else if (district->type == ecs::DistrictType::Industrial) zoneColor = Fade(YELLOW, 0.4f);
                DrawRectangleRec(destRec, zoneColor);
            }
            else if (mode == OverlayMode::Economy)
            {
                float intensity = std::clamp(district->wealth / 1000.0f, 0.0f, 1.0f);
                // Pulse slightly based on intensity
                float pulse = 0.8f + 0.2f * std::sin(time * 3.0f + district->grid_x);
                DrawRectangleRec(destRec, Fade(palette::economic_growth, intensity * 0.8f * pulse));
                
                // Draw up/down trend if wealth is very high or very low
                if (district->wealth > 800.0f)
                {
                    DrawTriangle(
                        Vector2{destRec.x + cell_px/2, destRec.y + cell_px*0.2f},
                        Vector2{destRec.x + cell_px*0.2f, destRec.y + cell_px*0.8f},
                        Vector2{destRec.x + cell_px*0.8f, destRec.y + cell_px*0.8f},
                        Fade(GREEN, 0.5f)
                    );
                }
            }
            else if (mode == OverlayMode::Instability)
            {
                float intensity = district->instability;
                float flash = 0.5f + 0.5f * std::sin(time * 10.0f * intensity);
                DrawRectangleRec(destRec, Fade(palette::system_failure, intensity * 0.8f * flash));
            }
            else if (mode == OverlayMode::Progression)
            {
                // Height based on level
                for (int b = 0; b < (int)district->level; ++b) {
                    float inset = 8.0f + b * 6.0f;
                    if (inset >= cell_px / 2.0f) break;
                    Rectangle bldgRec = { destRec.x + inset, destRec.y + inset, destRec.width - inset * 2, destRec.height - inset * 2 };
                    DrawRectangleLinesEx(bldgRec, 2.0f, Fade(palette::upgrade_pressure, 0.5f + 0.15f * b));
                }
            }
        }

        // Global Particle Effects: Rain
        // A simple procedural rain overlay that moves diagonally
        for (int i = 0; i < 200; ++i)
        {
            float seedX = (float)((i * 137) % 2000) - 100.0f;
            float seedY = (float)((i * 149) % 2000) - 100.0f;
            
            float dropX = std::fmod(seedX + time * 200.0f, 2000.0f);
            float dropY = std::fmod(seedY + time * 600.0f, 2000.0f);
            
            DrawLineV(Vector2{dropX, dropY}, Vector2{dropX - 10.0f, dropY + 30.0f}, Fade(DARKBLUE, 0.3f));
        }
    }
}
