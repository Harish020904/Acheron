#include "tile_renderer.h"
#include <cmath>
#include <algorithm>

namespace acheron::renderer
{
    void TileRenderer::render_districts(const ecs::World& world, const TextureCache& cache, float cell_px, OverlayMode mode, ecs::EntityID selected_district)
    {
        Texture2D tileset = cache.get_texture("tileset_cyberpunk");
        Rectangle srcRec = { 0.0f, 0.0f, 32.0f, 32.0f };
        
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
            
            // Background
            if (tileset.id != 0) {
                Color tint = WHITE;
                if (!district->powered) tint = DARKGRAY;
                DrawTexturePro(tileset, srcRec, destRec, Vector2{0, 0}, 0.0f, tint);
            } else {
                DrawRectangleRec(destRec, palette::background);
                DrawRectangleLinesEx(destRec, 2.0f, palette::grid_lines);
            }
            
            // Draw simple building blocks
            if (mode == OverlayMode::None || mode == OverlayMode::Traffic)
            {
                for (int b = 0; b < (int)district->level; ++b) {
                    float inset = 8.0f + b * 6.0f;
                    if (inset >= cell_px / 2.0f) break;
                    Rectangle bldgRec = { destRec.x + inset, destRec.y + inset, destRec.width - inset * 2, destRec.height - inset * 2 };
                    Color bldgColor = Fade(BLUE, 0.4f);
                    if (district->type == ecs::DistrictType::Commercial) bldgColor = palette::economic_growth; // Cyan/Neon Blue
                    else if (district->type == ecs::DistrictType::Industrial) bldgColor = palette::industrial_zone; // Orange
                    else if (district->type == ecs::DistrictType::Residential) bldgColor = Fade(SKYBLUE, 0.5f); // Cyan
                    
                    if (!district->powered) bldgColor = DARKGRAY;
                    
                    // Glow intensity based on level
                    float glow = 0.5f + (district->level * 0.15f);
                    if (district->type == ecs::DistrictType::Commercial) glow += 0.2f * std::sin(GetTime() * 3.0f + district->grid_x); // Commercial flicker
                    
                    // Failure State: Collapse warning (instability > 0.8)
                    if (district->instability > 0.8f)
                    {
                        bldgColor = RED;
                        glow = 0.7f + 0.3f * std::sin(GetTime() * 15.0f); // Rapid flashing
                    }
                    
                    DrawRectangleLinesEx(bldgRec, 2.0f, Fade(bldgColor, glow));
                }
            }

            // Selection highlight
            if (district->entity.index == selected_district.index)
            {
                DrawRectangleLinesEx(destRec, 3.0f, WHITE);
            }
        }
    }
}
