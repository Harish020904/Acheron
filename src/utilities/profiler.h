#pragma once

#include <cstdint>
#include <chrono>
#include <atomic>

namespace acheron::utilities
{
    // Frame profiler timing constants
    static constexpr std::uint32_t MAX_PROFILER_FRAMES = 256u;
    static constexpr std::uint32_t MAX_PROFILER_TIMERS = 64u;
    static constexpr std::uint32_t MAX_PROFILER_THREADS = 16u;
    static constexpr std::uint32_t TIMER_NAME_BYTES = 32u;

    // Timer ID enumeration for well-known timers
    enum class TimerID : std::uint16_t
    {
        INVALID = 0xFFFFu,

        // Frame lifecycle
        FRAME_START = 0,
        INPUT_PROCESSING = 1,
        SIMULATION = 2,
        ECS_UPDATE = 3,
        TRAFFIC = 4,
        ECONOMY = 5,
        PATHFINDING = 6,
        RENDERING = 7,
        GPU_SUBMIT = 8,
        UI_UPDATE = 9,
        FRAME_END = 10,

        // GPU operations
        GPU_CLEAR = 11,
        GPU_CULLING = 12,
        GPU_LIGHTING = 13,
        GPU_POSTPROCESS = 14,

        // Rendering stages
        VISIBILITY_CULLING = 15,
        DRAW_SUBMISSION = 16,
        RENDER_BARRIERS = 17,

        // ECS operations
        ECS_QUERY = 18,
        ECS_MIGRATION = 19,
        ECS_COMPONENT_ADD = 20,

        // End of predefined timers
        USER_TIMER_START = 64
    };

    // Timing data for a single timer instance
    struct TimerSnapshot
    {
        std::uint16_t timer_id = static_cast<std::uint16_t>(TimerID::INVALID);
        std::uint32_t frame_index = 0u;
        std::uint64_t cpu_time_us = 0ull;
        std::uint64_t gpu_time_us = 0ull;
        std::uint32_t thread_id = 0u;
    };

    // Frame statistics snapshot
    struct FrameSnapshot
    {
        std::uint32_t frame_index = 0u;
        std::uint64_t total_cpu_time_us = 0ull;
        std::uint64_t total_gpu_time_us = 0ull;
        std::uint32_t timer_count = 0u;
        std::uint32_t thread_count = 0u;
        float target_frame_time_ms = 16.67f;
        float actual_frame_time_ms = 0.0f;
    };

    // Scoped timer for RAII-style profiling
    class ScopedTimer
    {
    public:
        explicit ScopedTimer(TimerID timer_id) noexcept;
        ~ScopedTimer() noexcept;

        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;

        // Report GPU time for this timer scope
        void report_gpu_time(std::uint64_t gpu_time_us) noexcept;

    private:
        TimerID m_timer_id;
        std::chrono::high_resolution_clock::time_point m_start_time;
    };

    class FrameProfiler final
    {
    public:
        FrameProfiler() noexcept = default;

        FrameProfiler(const FrameProfiler&) = delete;
        FrameProfiler& operator=(const FrameProfiler&) = delete;

        // Initialize profiler with target frame rate
        bool initialize(float target_frame_rate_fps) noexcept;

        // Cleanup resources
        void shutdown() noexcept;

        // Mark frame boundaries
        void begin_frame() noexcept;
        void end_frame() noexcept;

        // === Low-Overhead Timer Management ===

        // Start a timer (returns timer ID, can pre-allocate well-known IDs)
        TimerID start_timer(TimerID timer_id) noexcept;

        // End a timer (non-blocking)
        bool end_timer(TimerID timer_id) noexcept;

        // Scoped timer helper (use with ScopedTimer RAII)
        bool is_timer_running(TimerID timer_id) const noexcept;

        // === Statistics ===

        // Get current frame index
        std::uint32_t get_frame_index() const noexcept;

        // Get current frame snapshot
        bool get_frame_snapshot(FrameSnapshot& out) const noexcept;

        // Get timer snapshot for specific timer
        bool get_timer_snapshot(TimerID timer_id, TimerSnapshot& out) const noexcept;

        // Get all timer snapshots for current frame
        bool get_all_timers(TimerSnapshot* out, std::uint32_t capacity, std::uint32_t& out_count) const noexcept;

        // Get frame time for specific frame
        float get_frame_time_ms(std::uint32_t frame_index) const noexcept;

        // Get average frame time over last N frames
        float get_average_frame_time_ms(std::uint32_t frame_count = 60u) const noexcept;

        // Get frame time variance (for stutter detection)
        float get_frame_time_variance_ms(std::uint32_t frame_count = 60u) const noexcept;

        // Get frame time spike detection (max deviation from average)
        float get_frame_time_max_deviation_ms(std::uint32_t frame_count = 60u) const noexcept;

        // === GPU Timing ===

        // Report GPU event timing (must call before frame boundaries)
        bool report_gpu_timing(TimerID timer_id, std::uint64_t gpu_time_us) noexcept;

        // Get GPU timing for specific timer
        std::uint64_t get_gpu_time_us(TimerID timer_id) const noexcept;

        // === Thread Visualization ===

        // Register current thread for tracking
        bool register_thread() noexcept;

        // Get thread count
        std::uint32_t get_thread_count() const noexcept;

        // Get profiler overhead as percentage
        float get_profiler_overhead_percent() const noexcept;

        // === Memory ===

        // Get profiler memory usage
        std::uint64_t get_memory_usage_bytes() const noexcept;

        // === State ===

        // Check if profiler is active
        bool is_initialized() const noexcept { return m_initialized; }

        // Check if currently in frame
        bool in_frame() const noexcept { return m_in_frame; }

        // === Validation ===

        // Reset all profiling data
        void reset() noexcept;

    private:
        // Timer state
        struct Timer
        {
            std::uint64_t cpu_time_us = 0ull;
            std::uint64_t gpu_time_us = 0ull;
            std::chrono::high_resolution_clock::time_point start_time{};
            bool active = false;
            std::uint32_t thread_id = 0u;
        };

        // Frame data
        struct Frame
        {
            std::uint64_t start_time_us = 0ull;
            std::uint64_t end_time_us = 0ull;
            std::uint64_t total_cpu_time_us = 0ull;
            std::uint64_t total_gpu_time_us = 0ull;
            Timer timers[MAX_PROFILER_TIMERS]{};
            std::uint32_t timer_count = 0u;
            bool complete = false;
        };

        Timer m_active_timers[MAX_PROFILER_TIMERS]{};
        Frame m_frames[MAX_PROFILER_FRAMES]{};

        std::atomic<std::uint32_t> m_frame_index{0u};
        std::atomic<bool> m_in_frame{false};
        std::atomic<std::uint32_t> m_registered_threads{0u};

        float m_target_frame_time_ms = 16.67f;
        bool m_initialized = false;

        // Helper methods
        Timer* find_active_timer(TimerID timer_id) noexcept;
        const Timer* find_active_timer(TimerID timer_id) const noexcept;
        Frame& get_current_frame() noexcept;
        const Frame& get_current_frame() const noexcept;
    };

    // Global profiler instance
    extern FrameProfiler g_profiler;

    // Convenience functions
    inline void profiler_begin_frame() noexcept
    {
        g_profiler.begin_frame();
    }

    inline void profiler_end_frame() noexcept
    {
        g_profiler.end_frame();
    }

    inline ScopedTimer profile_scope(TimerID timer_id) noexcept
    {
        return ScopedTimer(timer_id);
    }
}
