#pragma once

#include "raylib.h"

namespace acheron::renderer
{
    class SimpleRenderer final
    {
    public:
        struct Config
        {
            int width = 1920;
            int height = 1080;
            const char* title = "Acheron — Melbourne Urban Simulation";
            int target_fps = 60;
        };

        SimpleRenderer() = default;
        ~SimpleRenderer();

        bool init(const Config& config);
        void shutdown();

        bool begin_frame();
        void end_frame();

        void begin_world();
        void end_world();
        void update_camera();
        void pan_camera(float dx, float dy);
        Vector2 get_world_mouse_position() const;

        // Smooth camera fly-to (Locate Citizen)
        void fly_to(float world_x, float world_y, float zoom = 3.0f);
        void update_fly_to(float dt);
        bool is_flying() const { return m_flying; }

        // Frustum visibility test (world space)
        bool is_world_visible(float wx, float wy, float radius) const;

        bool should_close() const;
        bool is_initialized() const { return m_initialized; }

        Camera2D& get_camera() { return m_camera; }
        const Camera2D& get_camera() const { return m_camera; }
        
        int width() const;
        int height() const;

    private:
        bool m_initialized = false;
        Camera2D m_camera{};
        // Smooth fly-to state
        float m_fly_target_x    = 0.0f;
        float m_fly_target_y    = 0.0f;
        float m_fly_target_zoom = 1.0f;
        bool  m_flying          = false;
    };
}
