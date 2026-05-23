#include "melbourne_renderer.h"
#include "visual_palette.h"
#include "assets/data/melbourne_map_data.h"
#include <cmath>
#include <algorithm>

namespace acheron::renderer
{
    using namespace acheron::world;
    using namespace acheron::simulation::citizens;

    // ============================================================
    // Visibility culling — is a world-space point visible on screen?
    // ============================================================
    bool MelbourneRenderer::_is_visible(float wx, float wy, float radius,
                                         const Camera2D& camera,
                                         int sw, int sh) const
    {
        // Transform world → screen
        float sx = (wx - camera.target.x) * camera.zoom + sw * 0.5f + camera.offset.x;
        float sy = (wy - camera.target.y) * camera.zoom + sh * 0.5f + camera.offset.y;
        float margin = radius * camera.zoom + 64.0f;
        return sx > -margin && sx < sw + margin && sy > -margin && sy < sh + margin;
    }

    // ============================================================
    // Road visual style based on class and congestion
    // ============================================================
    Color MelbourneRenderer::_road_class_color(RoadClass rc, float congestion, int overlay_mode) const
    {
        if (overlay_mode == 1) // Traffic overlay
        {
            // Congestion-driven: green → yellow → red
            if (congestion < 0.35f) return Color{ 0, 220, 80,  255 };
            if (congestion < 0.70f) return Color{ 255, 190, 0, 255 };
            return Color{ 255, 30, 30, 255 };
        }

        // Base road colors by class (cyberpunk palette)
        switch (rc)
        {
            case RoadClass::MotorwayTrunk: return Color{ 255, 200, 40,  255 }; // Gold
            case RoadClass::Primary:       return Color{ 200, 220, 255, 200 }; // Pale blue-white
            case RoadClass::Secondary:     return Color{ 120, 160, 200, 160 }; // Medium blue-gray
            case RoadClass::Tertiary:      return Color{ 70,  100, 140, 130 }; // Dark blue-gray
            case RoadClass::Residential:   return Color{ 45,  65,  90,  100 }; // Very dark
            default:                       return Color{ 30,  50,  70,  70  };
        }
    }

    float MelbourneRenderer::_road_class_thickness(RoadClass rc, float zoom) const
    {
        float base;
        switch (rc)
        {
            case RoadClass::MotorwayTrunk: base = 6.0f; break;
            case RoadClass::Primary:       base = 4.0f; break;
            case RoadClass::Secondary:     base = 3.0f; break;
            case RoadClass::Tertiary:      base = 2.0f; break;
            case RoadClass::Residential:   base = 1.5f; break;
            default:                       base = 1.0f; break;
        }
        // Scale thickness with zoom so roads stay readable
        return base * std::clamp(zoom * 0.8f, 0.5f, 3.0f);
    }

    // ============================================================
    // Render all road edges
    // ============================================================
    void MelbourneRenderer::render_roads(const MelbourneMap& map,
                                          const Camera2D& camera,
                                          int screen_width, int screen_height,
                                          int overlay_mode) const
    {
        const float zoom = camera.zoom;
        const float time = GetTime();

        // At low zoom: show primary+ only; at medium: tertiary+; close up: all
        RoadClass min_class = (zoom < 0.25f) ? RoadClass::Primary
                            : (zoom < 0.50f) ? RoadClass::Secondary
                            : (zoom < 1.00f) ? RoadClass::Tertiary
                                             : RoadClass::Residential;

        for (uint32_t i = 0; i < map.edge_count; ++i)
        {
            const RoadEdge& edge = map.edges[i];

            // Cull low-importance roads at low zoom
            if (edge.road_class > min_class) continue;

            // Skip footways always unless very zoomed in
            if (edge.road_class == RoadClass::Footway && zoom < 3.0f) continue;

            const auto& from_node = melbourne_data::NODES[edge.from_node];
            const auto& to_node   = melbourne_data::NODES[edge.to_node];

            // Rough visibility cull
            float mid_x = (from_node.x + to_node.x) * 0.5f;
            float mid_y = (from_node.y + to_node.y) * 0.5f;
            if (!_is_visible(mid_x, mid_y, edge.length * 0.5f + 32.0f, camera, screen_width, screen_height))
                continue;

            Vector2 start { from_node.x, from_node.y };
            Vector2 end   { to_node.x,   to_node.y   };

            float thickness = _road_class_thickness(edge.road_class, zoom);
            Color color = _road_class_color(edge.road_class, edge.congestion, overlay_mode);

            // Draw dark asphalt base
            DrawLineEx(start, end, thickness + 2.0f, Color{12, 18, 28, 200});
            // Draw road color
            DrawLineEx(start, end, thickness, color);

            // Moving vehicle dots on roads with congestion (traffic overlay or normal)
            if (edge.congestion > 0.05f && zoom > 0.6f)
            {
                int vehicle_count = static_cast<int>(edge.congestion * 6.0f) + 1;
                vehicle_count = std::min(vehicle_count, 8);

                float speed_mult = 1.0f - edge.congestion * 0.6f;
                float dx = end.x - start.x;
                float dy = end.y - start.y;

                for (int v = 0; v < vehicle_count; ++v)
                {
                    float t = std::fmod(time * speed_mult * 0.4f + v * (1.0f / vehicle_count), 1.0f);
                    Vector2 pos { start.x + dx * t, start.y + dy * t };
                    float car_size = std::clamp(zoom * 2.0f, 1.5f, 4.0f);
                    DrawCircleV(pos, car_size, Color{240, 240, 200, 220});
                }
            }

            // Severe congestion flash
            if (overlay_mode == 1 && edge.congestion > 0.85f)
            {
                float pulse = 0.6f + 0.4f * std::sin(time * 18.0f);
                DrawLineEx(start, end, thickness + 1.5f, Fade(Color{255, 30, 30, 255}, pulse * 0.6f));
            }
        }
    }

    // ============================================================
    // Render points of interest (hospitals, schools, parks, etc.)
    // ============================================================
    void MelbourneRenderer::render_pois(const MelbourneMap& map,
                                         const Camera2D& camera,
                                         int screen_width, int screen_height) const
    {
        if (camera.zoom < 0.5f) return; // Don't draw POIs at very low zoom

        static const Color poi_colors[] = {
            Color{255, 60,  60,  220},  // Hospital — red
            Color{60,  220, 100, 220},  // School — green
            Color{60,  140, 255, 220},  // University — blue
            Color{40,  200, 80,  200},  // Park — dark green
            Color{255, 200, 40,  220},  // Station — gold
            Color{255, 120, 40,  200},  // Restaurant — orange
            Color{140, 80,  255, 200},  // Gym — purple
            Color{180, 200, 255, 180},  // Office — pale blue
        };

        float time = GetTime();
        float icon_size = std::clamp(camera.zoom * 5.0f, 3.0f, 10.0f);

        for (uint32_t i = 0; i < map.poi_count; ++i)
        {
            const PointOfInterest& poi = map.pois[i];
            if (!_is_visible(poi.x, poi.y, 24.0f, camera, screen_width, screen_height)) continue;

            uint8_t cat = poi.category < 8 ? poi.category : 7;
            Color col = poi_colors[cat];

            // Pulsing outer ring for hospitals
            if (cat == 0) // Hospital
            {
                float pulse = 0.5f + 0.5f * std::sin(time * 2.5f);
                DrawCircleV(Vector2{poi.x, poi.y}, icon_size * 1.8f, Fade(col, pulse * 0.4f));
            }

            DrawCircleV(Vector2{poi.x, poi.y}, icon_size, col);
            DrawCircleLinesV(Vector2{poi.x, poi.y}, icon_size + 1.5f, Fade(WHITE, 0.4f));
        }
    }

    // ============================================================
    // Render POI labels (only at sufficient zoom)
    // ============================================================
    void MelbourneRenderer::render_labels(const MelbourneMap& map,
                                           Font font,
                                           const Camera2D& camera) const
    {
        if (camera.zoom < 1.2f) return; // Only show labels when zoomed in enough

        for (uint32_t i = 0; i < map.poi_count; ++i)
        {
            const PointOfInterest& poi = map.pois[i];
            float sx = poi.x + 12.0f;
            float sy = poi.y - 6.0f;
            float font_size = std::clamp(camera.zoom * 8.0f, 6.0f, 12.0f);
            DrawTextEx(font, poi.name, Vector2{sx, sy}, font_size, 0.5f, Fade(WHITE, 0.9f));
        }
    }

    // ============================================================
    // CitizenRenderer
    // ============================================================
    void CitizenRenderer::render(const CitizenSystem& system,
                                  const Camera2D& camera,
                                  uint32_t selected_uid,
                                  float sim_time) const
    {
        (void)sim_time;
        // Only render individual citizens at zoom > 0.5
        if (camera.zoom < 0.5f) return;

        float citizen_size = std::clamp(camera.zoom * 2.5f, 1.5f, 5.0f);
        float time = GetTime();

        static const Color occupation_colors[] = {
            Color{40,  120, 255, 200}, // Worker — blue
            Color{40,  220, 80,  200}, // Student — green
            Color{255, 40,  40,  220}, // Medic — red
            Color{255, 200, 0,   200}, // Service — yellow
            Color{160, 160, 160, 160}, // Retired — gray
        };

        for (uint32_t i = 0; i < system.citizen_count(); ++i)
        {
            const auto* c = system.citizen_slot(i);
            if (!c) continue;

            uint8_t occ = static_cast<uint8_t>(c->occupation);
            if (occ >= 5) occ = 0;
            Color col = occupation_colors[occ];

            // Mood modifier
            if (c->mood == CitizenMood::Unhappy)   col = Color{255, 0, 0, 200};
            if (c->mood == CitizenMood::Stressed)  col.r = static_cast<unsigned char>(std::min(255, (int)col.r + 60));

            Vector2 pos { c->x, c->y };
            DrawCircleV(pos, citizen_size, col);

            // Selected citizen: pulsing ring + destination line
            if (c->uid == selected_uid)
            {
                float pulse = 0.6f + 0.4f * std::sin(time * 6.0f);
                DrawCircleLinesV(pos, citizen_size * 2.5f, Fade(WHITE, pulse));
                DrawLineEx(pos, Vector2{c->dest_x, c->dest_y}, 1.5f, Fade(YELLOW, 0.6f));
            }
        }
    }

} // namespace acheron::renderer
