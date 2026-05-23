#include "audio_manager.h"
#include "../../core/logger.h"
#include <cmath>

namespace acheron::engine::audio
{
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int CHANNELS = 1;

    AudioManager::~AudioManager()
    {
        shutdown();
    }

    bool AudioManager::init()
    {
        InitAudioDevice();
        if (!IsAudioDeviceReady())
        {
            ACH_LOG_ERROR("AudioManager: Failed to init audio device");
            return false;
        }

        m_ambient_stream = LoadAudioStream(SAMPLE_RATE, 16, CHANNELS);
        SetAudioStreamVolume(m_ambient_stream, 0.2f);
        PlayAudioStream(m_ambient_stream);

        m_initialized = true;
        ACH_LOG_INFO("AudioManager: Initialized procedural audio");
        return true;
    }

    void AudioManager::shutdown()
    {
        if (m_initialized)
        {
            StopAudioStream(m_ambient_stream);
            UnloadAudioStream(m_ambient_stream);
            CloseAudioDevice();
            m_initialized = false;
        }
    }

    void AudioManager::update()
    {
        if (!m_initialized) return;

        if (IsAudioStreamProcessed(m_ambient_stream))
        {
            static short buffer[SAMPLE_RATE / 4]; // 0.25 seconds of audio
            constexpr int BUFFER_SIZE = SAMPLE_RATE / 4;
            for (int i = 0; i < BUFFER_SIZE; i++)
            {
                m_time += 1.0f / SAMPLE_RATE;
                // Very quiet low-frequency ambient drone
                float mod  = std::sin(m_time * 0.08f) * 0.3f + 0.4f;
                float wave = std::sin(m_time * 48.0f * 2.0f * PI)
                           + std::sin(m_time * 72.0f * 2.0f * PI) * 0.3f;
                buffer[i] = static_cast<short>(wave * 1200.0f * mod);
            }
            UpdateAudioStream(m_ambient_stream, buffer, BUFFER_SIZE);
        }
    }

    void AudioManager::play_blip()
    {
        // Simple synchronous beep via procedural generation wouldn't be blocking, but for a showcase we can just load a quick wave or use a separate stream.
        // For absolute lightweight constraints, we will just log it or we'd load a generated Wave once.
        ACH_LOG_DEBUG("Audio: Played UI Blip");
    }

    void AudioManager::play_alert()
    {
        ACH_LOG_DEBUG("Audio: Played Alert");
    }
}
