#include "renderer.h"

namespace acheron::renderer
{
    RendererBackend::~RendererBackend() noexcept
    {
        shutdown();
    }

    bool RendererBackend::init(const Config& config) noexcept
    {
        if (config.width <= 0 || config.height <= 0 || !config.title)
        {
            return false;
        }

        unsigned int flags = 0;
        if (config.resizable)
        {
            flags |= FLAG_WINDOW_RESIZABLE;
        }
        if (config.vsync)
        {
            flags |= FLAG_VSYNC_HINT;
        }

        SetConfigFlags(flags);
        InitWindow(config.width, config.height, config.title);
        SetTargetFPS(config.target_fps > 0 ? config.target_fps : 60);

        m_initialized = true;
        return true;
    }

    void RendererBackend::shutdown() noexcept
    {
        if (!m_initialized)
        {
            return;
        }

        CloseWindow();
        m_initialized = false;
    }

    bool RendererBackend::begin_frame() noexcept
    {
        if (!m_initialized || WindowShouldClose())
        {
            return false;
        }

        BeginDrawing();
        return true;
    }

    void RendererBackend::end_frame() noexcept
    {
        if (m_initialized)
        {
            EndDrawing();
        }
    }

    bool RendererBackend::should_close() const noexcept
    {
        return !m_initialized || WindowShouldClose();
    }

    bool RendererBackend::key_pressed(int key) const noexcept
    {
        return m_initialized && IsKeyPressed(key);
    }

    bool RendererBackend::key_down(int key) const noexcept
    {
        return m_initialized && IsKeyDown(key);
    }

    float RendererBackend::frame_time() const noexcept
    {
        return m_initialized ? GetFrameTime() : 0.0f;
    }

    int RendererBackend::width() const noexcept
    {
        return m_initialized ? GetScreenWidth() : 0;
    }

    int RendererBackend::height() const noexcept
    {
        return m_initialized ? GetScreenHeight() : 0;
    }
}
