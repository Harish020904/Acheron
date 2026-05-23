#pragma once

#include "raylib.h"
#include "../../world/melbourne_map.h"
#include "../../simulation/citizens/citizen_system.h"

namespace acheron::renderer
{
    // Camera state for smooth inertia and fly-to support
    struct SmoothCamera
    {
        float target_x    = 0.0f;
        float target_y    = 0.0f;
        float target_zoom = 0.8f;
        float current_x   = 0.0f;
        float current_y   = 0.0f;
        float current_zoom = 0.8f;
        float inertia_speed = 8.0f;   // Higher = snappier
        bool  flying_to     = false;

        void fly_to(float wx, float wy, float zoom)
        {
            target_x    = wx;
            target_y    = wy;
            target_zoom = zoom;
            flying_to   = true;
        }

        void update(float dt)
        {
            float t = 1.0f - std::pow(0.001f, dt * inertia_speed);
            current_x    += (target_x    - current_x)    * t;
            current_y    += (target_y    - current_y)    * t;
            current_zoom += (target_zoom - current_zoom) * t;
            // Stop flying when close enough
            if (flying_to)
            {
                float dx = target_x - current_x;
                float dy = target_y - current_y;
                if ((dx*dx + dy*dy) < 4.0f) flying_to = false;
            }
        }
    };

    class MelbourneRenderer final
    {
    public:
        // Render the real Melbourne road network
        void render_roads(const world::MelbourneMap& map,
                          const Camera2D& camera,
                          int screen_width, int screen_height,
                          int overlay_mode) const;

        // Render buildings / points of interest
        void render_pois(const world::MelbourneMap& map,
                         const Camera2D& camera,
                         int screen_width, int screen_height) const;

        // Render district name labels (suburb labels at zoom-out)
        void render_labels(const world::MelbourneMap& map,
                           Font font,
                           const Camera2D& camera) const;

    private:
        bool _is_visible(float wx, float wy, float radius,
                         const Camera2D& camera,
                         int sw, int sh) const;
        Color _road_class_color(world::RoadClass rc, float congestion, int overlay_mode) const;
        float _road_class_thickness(world::RoadClass rc, float zoom) const;
    };

    class CitizenRenderer final
    {
    public:
        // Render citizen dots
        void render(const simulation::citizens::CitizenSystem& system,
                    const Camera2D& camera,
                    uint32_t selected_uid,
                    float sim_time) const;
    };

} // namespace acheron::renderer
