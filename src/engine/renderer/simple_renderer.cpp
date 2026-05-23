#include "simple_renderer.h"
#include "assets/data/melbourne_map_data.h"
#include <cmath>

namespace acheron::renderer
{
    SimpleRenderer::~SimpleRenderer()
    {
        shutdown();
    }

    bool SimpleRenderer::init(const Config& config)
    {
        if (config.width <= 0 || config.height <= 0 || !config.title)
        {
            return false;
        }

        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(config.width, config.height, config.title);
        SetTargetFPS(config.target_fps);

        m_camera.target = Vector2{ 0.0f, 0.0f };
        m_camera.offset = Vector2{ static_cast<float>(config.width) / 2.0f, static_cast<float>(config.height) / 2.0f };
        m_camera.rotation = 0.0f;
        m_camera.zoom = 0.12f; // Zoomed out to see full Melbourne CBD network

        // Center on Melbourne CBD
        m_fly_target_x    = acheron::world::melbourne_data::MAP_WORLD_WIDTH  * 0.5f;
        m_fly_target_y    = acheron::world::melbourne_data::MAP_WORLD_HEIGHT * 0.5f;
        m_fly_target_zoom = 0.12f;
        m_camera.target = Vector2{ m_fly_target_x, m_fly_target_y };

        m_initialized = true;
        return true;
    }

    void SimpleRenderer::shutdown()
    {
        if (m_initialized)
        {
            CloseWindow();
            m_initialized = false;
        }
    }

    bool SimpleRenderer::begin_frame()
    {
        if (!m_initialized || WindowShouldClose())
            return false;

        BeginDrawing();
        return true;
    }

    void SimpleRenderer::end_frame()
    {
        if (m_initialized)
            EndDrawing();
    }

    void SimpleRenderer::begin_world()
    {
        BeginMode2D(m_camera);
    }

    void SimpleRenderer::end_world()
    {
        EndMode2D();
    }

    void SimpleRenderer::update_camera()
    {
        // WASD Panning
        float speed = 500.0f / m_camera.zoom * GetFrameTime();
        if (IsKeyDown(KEY_W)) m_camera.target.y -= speed;
        if (IsKeyDown(KEY_S)) m_camera.target.y += speed;
        if (IsKeyDown(KEY_A)) m_camera.target.x -= speed;
        if (IsKeyDown(KEY_D)) m_camera.target.x += speed;

        // Zooming
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
        {
            Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), m_camera);
            m_camera.offset = GetMousePosition();
            m_camera.target = mouseWorldPos;

            m_camera.zoom += wheel * 0.15f * m_camera.zoom;
            if (m_camera.zoom < 0.15f) m_camera.zoom = 0.15f;
            if (m_camera.zoom > 6.0f)  m_camera.zoom = 6.0f;
        }

        float panSpeed = 500.0f / m_camera.zoom * GetFrameTime();
        if (IsKeyDown(KEY_W)) m_camera.target.y -= panSpeed;
        if (IsKeyDown(KEY_S)) m_camera.target.y += panSpeed;
        if (IsKeyDown(KEY_A)) m_camera.target.x -= panSpeed;
        if (IsKeyDown(KEY_D)) m_camera.target.x += panSpeed;

        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
        {
            Vector2 delta = GetMouseDelta();
            delta.x = delta.x * -1.0f / m_camera.zoom;
            delta.y = delta.y * -1.0f / m_camera.zoom;
            m_camera.target.x += delta.x;
            m_camera.target.y += delta.y;
        }

        if (IsKeyPressed(KEY_F11))
        {
            ToggleFullscreen();
        }
    }

    Vector2 SimpleRenderer::get_world_mouse_position() const
    {
        return GetScreenToWorld2D(GetMousePosition(), m_camera);
    }
    
    void SimpleRenderer::pan_camera(float dx, float dy)
    {
        m_camera.target.x += dx;
        m_camera.target.y += dy;
    }

    void SimpleRenderer::fly_to(float world_x, float world_y, float zoom)
    {
        m_fly_target_x    = world_x;
        m_fly_target_y    = world_y;
        m_fly_target_zoom = zoom;
        m_flying          = true;
    }

    void SimpleRenderer::update_fly_to(float dt)
    {
        if (!m_flying) return;

        float t = 1.0f - std::pow(0.001f, dt * 5.0f); // Smooth exponential ease
        m_camera.target.x += (m_fly_target_x - m_camera.target.x) * t;
        m_camera.target.y += (m_fly_target_y - m_camera.target.y) * t;
        m_camera.zoom     += (m_fly_target_zoom - m_camera.zoom)   * t;

        float dx = m_fly_target_x - m_camera.target.x;
        float dy = m_fly_target_y - m_camera.target.y;
        if ((dx*dx + dy*dy) < 4.0f && std::abs(m_fly_target_zoom - m_camera.zoom) < 0.01f)
            m_flying = false;
    }

    bool SimpleRenderer::is_world_visible(float wx, float wy, float radius) const
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        float sx = (wx - m_camera.target.x) * m_camera.zoom + m_camera.offset.x;
        float sy = (wy - m_camera.target.y) * m_camera.zoom + m_camera.offset.y;
        float margin = radius * m_camera.zoom + 64.0f;
        return sx > -margin && sx < sw + margin && sy > -margin && sy < sh + margin;
    }

    bool SimpleRenderer::should_close() const
    {
        return !m_initialized || WindowShouldClose();
    }
    
    int SimpleRenderer::width() const
    {
        return GetScreenWidth();
    }

    int SimpleRenderer::height() const
    {
        return GetScreenHeight();
    }
}
