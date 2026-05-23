#pragma once

#include "raylib.h"
#include <unordered_map>
#include <string>

namespace acheron::engine::audio
{
    class AudioManager final
    {
    public:
        AudioManager() = default;
        ~AudioManager();

        bool init();
        void shutdown();

        void update(); // Handle ambient audio generation
        void play_blip();
        void play_alert();

    private:
        bool m_initialized = false;
        AudioStream m_ambient_stream;
        float m_time = 0.0f;
    };
}
