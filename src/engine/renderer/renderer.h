#pragma once

#include "raylib.h"

#include <cstdint>

namespace acheron::renderer
{
    class RendererBackend final
    {
    public:
        struct Config
        {
            int width = 1280;
            int height = 720;
            const char* title = "acheron";
            bool resizable = true;
            bool vsync = true;
            int target_fps = 60;
        };

        RendererBackend() noexcept = default;
        ~RendererBackend() noexcept;

        RendererBackend(const RendererBackend&) = delete;
        RendererBackend& operator=(const RendererBackend&) = delete;

        bool init(const Config& config) noexcept;
        void shutdown() noexcept;

        bool begin_frame() noexcept;
        void end_frame() noexcept;

        bool should_close() const noexcept;
        bool key_pressed(int key) const noexcept;
        bool key_down(int key) const noexcept;

        float frame_time() const noexcept;
        int width() const noexcept;
        int height() const noexcept;
        bool is_initialized() const noexcept { return m_initialized; }

    private:
        bool m_initialized = false;
    };
}
