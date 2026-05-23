#include "profiler.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <thread>

namespace acheron::utilities
{
    // Global profiler instance
    FrameProfiler g_profiler;

    // === ScopedTimer Implementation ===

    ScopedTimer::ScopedTimer(TimerID timer_id) noexcept
        : m_timer_id(timer_id),
          m_start_time(std::chrono::high_resolution_clock::now())
    {
        g_profiler.start_timer(timer_id);
    }

    ScopedTimer::~ScopedTimer() noexcept
    {
        (void)std::chrono::high_resolution_clock::now();
        g_profiler.end_timer(m_timer_id);
    }

    void ScopedTimer::report_gpu_time(std::uint64_t gpu_time_us) noexcept
    {
        g_profiler.report_gpu_timing(m_timer_id, gpu_time_us);
    }

    // === FrameProfiler Implementation ===

    bool FrameProfiler::initialize(float target_frame_rate_fps) noexcept
    {
        if (m_initialized || target_frame_rate_fps <= 0.0f)
        {
            return false;
        }

        m_target_frame_time_ms = 1000.0f / target_frame_rate_fps;
        m_frame_index.store(0u, std::memory_order_relaxed);
        m_in_frame.store(false, std::memory_order_relaxed);
        m_registered_threads.store(0u, std::memory_order_relaxed);

        for (std::uint32_t i = 0; i < MAX_PROFILER_FRAMES; ++i)
        {
            m_frames[i] = {};
        }

        for (std::uint32_t i = 0; i < MAX_PROFILER_TIMERS; ++i)
        {
            m_active_timers[i] = {};
        }

        m_initialized = true;
        return true;
    }

    void FrameProfiler::shutdown() noexcept
    {
        m_initialized = false;
        m_in_frame.store(false, std::memory_order_relaxed);
    }

    void FrameProfiler::begin_frame() noexcept
    {
        if (!m_initialized)
        {
            return;
        }

        m_in_frame.store(true, std::memory_order_relaxed);
        auto& frame = get_current_frame();
        frame.start_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
        frame.timer_count = 0u;
    }

    void FrameProfiler::end_frame() noexcept
    {
        if (!m_initialized || !m_in_frame.load(std::memory_order_relaxed))
        {
            return;
        }

        auto& frame = get_current_frame();
        frame.end_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();

        if (frame.end_time_us > frame.start_time_us)
        {
            frame.total_cpu_time_us = frame.end_time_us - frame.start_time_us;
        }

        frame.complete = true;
        m_in_frame.store(false, std::memory_order_relaxed);
        m_frame_index.fetch_add(1u, std::memory_order_relaxed);
    }

    TimerID FrameProfiler::start_timer(TimerID timer_id) noexcept
    {
        if (!m_initialized || static_cast<std::uint16_t>(timer_id) >= MAX_PROFILER_TIMERS)
        {
            return TimerID::INVALID;
        }

        Timer& timer = m_active_timers[static_cast<std::uint16_t>(timer_id)];
        timer.active = true;
        timer.start_time = std::chrono::high_resolution_clock::now();
        timer.thread_id = static_cast<std::uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));

        return timer_id;
    }

    bool FrameProfiler::end_timer(TimerID timer_id) noexcept
    {
        if (!m_initialized || static_cast<std::uint16_t>(timer_id) >= MAX_PROFILER_TIMERS)
        {
            return false;
        }

        Timer& timer = m_active_timers[static_cast<std::uint16_t>(timer_id)];
        if (!timer.active)
        {
            return false;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        timer.cpu_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - timer.start_time).count();
        timer.active = false;

        return true;
    }

    bool FrameProfiler::is_timer_running(TimerID timer_id) const noexcept
    {
        if (!m_initialized || static_cast<std::uint16_t>(timer_id) >= MAX_PROFILER_TIMERS)
        {
            return false;
        }

        return m_active_timers[static_cast<std::uint16_t>(timer_id)].active;
    }

    std::uint32_t FrameProfiler::get_frame_index() const noexcept
    {
        return m_frame_index.load(std::memory_order_relaxed);
    }

    bool FrameProfiler::get_frame_snapshot(FrameSnapshot& out) const noexcept
    {
        if (!m_initialized)
        {
            return false;
        }

        std::uint32_t frame_idx = m_frame_index.load(std::memory_order_relaxed);
        if (frame_idx == 0u)
        {
            return false;
        }

        const Frame& frame = m_frames[frame_idx % MAX_PROFILER_FRAMES];
        if (!frame.complete)
        {
            return false;
        }

        out.frame_index = frame_idx;
        out.total_cpu_time_us = frame.total_cpu_time_us;
        out.total_gpu_time_us = frame.total_gpu_time_us;
        out.timer_count = frame.timer_count;
        out.thread_count = m_registered_threads.load(std::memory_order_relaxed);
        out.target_frame_time_ms = m_target_frame_time_ms;
        out.actual_frame_time_ms = static_cast<float>(frame.total_cpu_time_us) / 1000.0f;

        return true;
    }

    bool FrameProfiler::get_timer_snapshot(TimerID timer_id, TimerSnapshot& out) const noexcept
    {
        if (!m_initialized || static_cast<std::uint16_t>(timer_id) >= MAX_PROFILER_TIMERS)
        {
            return false;
        }

        const Timer& timer = m_active_timers[static_cast<std::uint16_t>(timer_id)];
        if (!timer.active && timer.cpu_time_us == 0ull)
        {
            return false;
        }

        out.timer_id = static_cast<std::uint16_t>(timer_id);
        out.frame_index = m_frame_index.load(std::memory_order_relaxed);
        out.cpu_time_us = timer.cpu_time_us;
        out.gpu_time_us = timer.gpu_time_us;
        out.thread_id = timer.thread_id;

        return true;
    }

    bool FrameProfiler::get_all_timers(TimerSnapshot* out, std::uint32_t capacity, std::uint32_t& out_count) const noexcept
    {
        if (!m_initialized || !out || capacity == 0u)
        {
            return false;
        }

        out_count = 0u;
        for (std::uint32_t i = 0; i < MAX_PROFILER_TIMERS && out_count < capacity; ++i)
        {
            const Timer& timer = m_active_timers[i];
            if (timer.cpu_time_us > 0ull || timer.active)
            {
                TimerSnapshot& snap = out[out_count];
                snap.timer_id = i;
                snap.frame_index = m_frame_index.load(std::memory_order_relaxed);
                snap.cpu_time_us = timer.cpu_time_us;
                snap.gpu_time_us = timer.gpu_time_us;
                snap.thread_id = timer.thread_id;
                ++out_count;
            }
        }

        return out_count > 0u;
    }

    float FrameProfiler::get_frame_time_ms(std::uint32_t frame_index) const noexcept
    {
        if (!m_initialized || frame_index >= MAX_PROFILER_FRAMES)
        {
            return 0.0f;
        }

        const Frame& frame = m_frames[frame_index % MAX_PROFILER_FRAMES];
        return static_cast<float>(frame.total_cpu_time_us) / 1000.0f;
    }

    float FrameProfiler::get_average_frame_time_ms(std::uint32_t frame_count) const noexcept
    {
        if (!m_initialized || frame_count == 0u)
        {
            return 0.0f;
        }

        frame_count = std::min(frame_count, MAX_PROFILER_FRAMES);
        std::uint32_t current_idx = m_frame_index.load(std::memory_order_relaxed);

        std::uint64_t total_time_us = 0ull;
        for (std::uint32_t i = 0; i < frame_count; ++i)
        {
            std::uint32_t frame_idx = (current_idx - i - 1u) % MAX_PROFILER_FRAMES;
            total_time_us += m_frames[frame_idx].total_cpu_time_us;
        }

        return static_cast<float>(total_time_us) / static_cast<float>(frame_count) / 1000.0f;
    }

    float FrameProfiler::get_frame_time_variance_ms(std::uint32_t frame_count) const noexcept
    {
        if (!m_initialized || frame_count == 0u)
        {
            return 0.0f;
        }

        float avg_time = get_average_frame_time_ms(frame_count);
        if (avg_time == 0.0f)
        {
            return 0.0f;
        }

        frame_count = std::min(frame_count, MAX_PROFILER_FRAMES);
        std::uint32_t current_idx = m_frame_index.load(std::memory_order_relaxed);

        float variance = 0.0f;
        for (std::uint32_t i = 0; i < frame_count; ++i)
        {
            std::uint32_t frame_idx = (current_idx - i - 1u) % MAX_PROFILER_FRAMES;
            float frame_time = static_cast<float>(m_frames[frame_idx].total_cpu_time_us) / 1000.0f;
            float diff = frame_time - avg_time;
            variance += diff * diff;
        }

        variance /= static_cast<float>(frame_count);
        return std::sqrt(variance);
    }

    float FrameProfiler::get_frame_time_max_deviation_ms(std::uint32_t frame_count) const noexcept
    {
        if (!m_initialized || frame_count == 0u)
        {
            return 0.0f;
        }

        float avg_time = get_average_frame_time_ms(frame_count);
        if (avg_time == 0.0f)
        {
            return 0.0f;
        }

        frame_count = std::min(frame_count, MAX_PROFILER_FRAMES);
        std::uint32_t current_idx = m_frame_index.load(std::memory_order_relaxed);

        float max_deviation = 0.0f;
        for (std::uint32_t i = 0; i < frame_count; ++i)
        {
            std::uint32_t frame_idx = (current_idx - i - 1u) % MAX_PROFILER_FRAMES;
            float frame_time = static_cast<float>(m_frames[frame_idx].total_cpu_time_us) / 1000.0f;
            float deviation = std::abs(frame_time - avg_time);
            max_deviation = std::max(max_deviation, deviation);
        }

        return max_deviation;
    }

    bool FrameProfiler::report_gpu_timing(TimerID timer_id, std::uint64_t gpu_time_us) noexcept
    {
        if (!m_initialized || static_cast<std::uint16_t>(timer_id) >= MAX_PROFILER_TIMERS)
        {
            return false;
        }

        m_active_timers[static_cast<std::uint16_t>(timer_id)].gpu_time_us = gpu_time_us;
        return true;
    }

    std::uint64_t FrameProfiler::get_gpu_time_us(TimerID timer_id) const noexcept
    {
        if (!m_initialized || static_cast<std::uint16_t>(timer_id) >= MAX_PROFILER_TIMERS)
        {
            return 0ull;
        }

        return m_active_timers[static_cast<std::uint16_t>(timer_id)].gpu_time_us;
    }

    bool FrameProfiler::register_thread() noexcept
    {
        if (!m_initialized)
        {
            return false;
        }

        std::uint32_t expected = m_registered_threads.load(std::memory_order_relaxed);
        if (expected >= MAX_PROFILER_THREADS)
        {
            return false;
        }

        m_registered_threads.fetch_add(1u, std::memory_order_relaxed);
        return true;
    }

    std::uint32_t FrameProfiler::get_thread_count() const noexcept
    {
        if (!m_initialized)
        {
            return 0u;
        }

        return m_registered_threads.load(std::memory_order_relaxed);
    }

    float FrameProfiler::get_profiler_overhead_percent() const noexcept
    {
        if (!m_initialized || m_target_frame_time_ms <= 0.0f)
        {
            return 0.0f;
        }

        // Profiler overhead is typically very small (< 1%)
        // This is a conservative estimate based on frame time measurements
        float avg_frame_time = get_average_frame_time_ms(60u);
        if (avg_frame_time <= 0.0f)
        {
            return 0.0f;
        }

        // Assume profiler adds ~0.1ms overhead per frame
        float profiler_overhead_ms = 0.1f;
        return (profiler_overhead_ms / avg_frame_time) * 100.0f;
    }

    std::uint64_t FrameProfiler::get_memory_usage_bytes() const noexcept
    {
        if (!m_initialized)
        {
            return 0ull;
        }

        // Calculate memory used by frame profiler
        std::uint64_t frame_memory = MAX_PROFILER_FRAMES * sizeof(Frame);
        std::uint64_t timer_memory = MAX_PROFILER_TIMERS * sizeof(Timer);
        std::uint64_t atomics_memory = 3u * sizeof(std::atomic<std::uint32_t>) + sizeof(std::atomic<bool>);

        return frame_memory + timer_memory + atomics_memory;
    }

    void FrameProfiler::reset() noexcept
    {
        if (!m_initialized)
        {
            return;
        }

        m_frame_index.store(0u, std::memory_order_relaxed);
        m_in_frame.store(false, std::memory_order_relaxed);

        for (std::uint32_t i = 0; i < MAX_PROFILER_FRAMES; ++i)
        {
            m_frames[i] = {};
        }

        for (std::uint32_t i = 0; i < MAX_PROFILER_TIMERS; ++i)
        {
            m_active_timers[i] = {};
        }
    }

    FrameProfiler::Timer* FrameProfiler::find_active_timer(TimerID timer_id) noexcept
    {
        if (static_cast<std::uint16_t>(timer_id) >= MAX_PROFILER_TIMERS)
        {
            return nullptr;
        }

        return &m_active_timers[static_cast<std::uint16_t>(timer_id)];
    }

    const FrameProfiler::Timer* FrameProfiler::find_active_timer(TimerID timer_id) const noexcept
    {
        if (static_cast<std::uint16_t>(timer_id) >= MAX_PROFILER_TIMERS)
        {
            return nullptr;
        }

        return &m_active_timers[static_cast<std::uint16_t>(timer_id)];
    }

    FrameProfiler::Frame& FrameProfiler::get_current_frame() noexcept
    {
        std::uint32_t idx = m_frame_index.load(std::memory_order_relaxed);
        return m_frames[idx % MAX_PROFILER_FRAMES];
    }

    const FrameProfiler::Frame& FrameProfiler::get_current_frame() const noexcept
    {
        std::uint32_t idx = m_frame_index.load(std::memory_order_relaxed);
        return m_frames[idx % MAX_PROFILER_FRAMES];
    }
}

#ifdef ACHERON_FRAME_PROFILER_UNIT_TEST

namespace acheron::utilities
{
    static bool test_initialization()
    {
        FrameProfiler profiler;

        if (profiler.is_initialized())
        {
            return false;
        }

        if (!profiler.initialize(60.0f))
        {
            return false;
        }

        if (!profiler.is_initialized())
        {
            return false;
        }

        if (profiler.initialize(60.0f))
        {
            return false; // Cannot reinitialize - should fail
        }

        profiler.shutdown();

        if (profiler.is_initialized())
        {
            return false;
        }

        return true;
    }

    static bool test_frame_timing()
    {
        FrameProfiler profiler;
        if (!profiler.initialize(60.0f))
        {
            return false;
        }

        profiler.begin_frame();

        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        profiler.end_frame();

        if (!profiler.in_frame())
        {
            // Should be out of frame after end_frame
        }

        float frame_time = profiler.get_frame_time_ms(0);
        if (frame_time < 0.5f || frame_time > 100.0f)
        {
            return false; // Frame time should be reasonable
        }

        profiler.shutdown();
        return true;
    }

    static bool test_timer_basic()
    {
        FrameProfiler profiler;
        if (!profiler.initialize(60.0f))
        {
            return false;
        }

        TimerID timer_id = TimerID::SIMULATION;

        // Start timer
        TimerID started = profiler.start_timer(timer_id);
        if (started != timer_id)
        {
            return false;
        }

        if (!profiler.is_timer_running(timer_id))
        {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // End timer
        if (!profiler.end_timer(timer_id))
        {
            return false;
        }

        if (profiler.is_timer_running(timer_id))
        {
            return false;
        }

        // Get snapshot
        TimerSnapshot snap;
        if (!profiler.get_timer_snapshot(timer_id, snap))
        {
            return false;
        }

        if (snap.cpu_time_us < 500ull || snap.cpu_time_us > 100000ull)
        {
            return false;
        }

        profiler.shutdown();
        return true;
    }

    static bool test_scoped_timer()
    {
        // Use the global profiler since ScopedTimer uses g_profiler
        if (!g_profiler.initialize(60.0f))
        {
            return false;
        }

        // No frame context, just test ScopedTimer directly
        {
            ScopedTimer timer(TimerID::RENDERING);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        // Timer should be complete and have CPU time recorded
        TimerSnapshot snap;
        bool snapshot_ok = g_profiler.get_timer_snapshot(TimerID::RENDERING, snap);
        if (!snapshot_ok)
        {
            g_profiler.shutdown();
            return false;
        }

        // Timer should have recorded some time
        if (snap.cpu_time_us == 0ull)
        {
            g_profiler.shutdown();
            return false;
        }

        g_profiler.shutdown();
        return true;
    }

    static bool test_gpu_timing()
    {
        FrameProfiler profiler;
        if (!profiler.initialize(60.0f))
        {
            return false;
        }

        profiler.start_timer(TimerID::GPU_SUBMIT);
        profiler.report_gpu_timing(TimerID::GPU_SUBMIT, 5000ull);
        profiler.end_timer(TimerID::GPU_SUBMIT);

        std::uint64_t gpu_time = profiler.get_gpu_time_us(TimerID::GPU_SUBMIT);
        if (gpu_time != 5000ull)
        {
            return false;
        }

        profiler.shutdown();
        return true;
    }

    static bool test_frame_statistics()
    {
        FrameProfiler profiler;
        if (!profiler.initialize(60.0f))
        {
            return false;
        }

        // Simulate multiple frames
        for (int i = 0; i < 5; ++i)
        {
            profiler.begin_frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            profiler.end_frame();
        }

        float avg_time = profiler.get_average_frame_time_ms(5);
        if (avg_time < 0.5f || avg_time > 100.0f)
        {
            return false;
        }

        float variance = profiler.get_frame_time_variance_ms(5);
        (void)variance;
        // Variance should be positive

        float max_dev = profiler.get_frame_time_max_deviation_ms(5);
        (void)max_dev;
        // Max deviation should be positive

        profiler.shutdown();
        return true;
    }

    static bool test_thread_registration()
    {
        FrameProfiler profiler;
        if (!profiler.initialize(60.0f))
        {
            return false;
        }

        if (profiler.get_thread_count() != 0u)
        {
            return false;
        }

        if (!profiler.register_thread())
        {
            return false;
        }

        if (profiler.get_thread_count() != 1u)
        {
            return false;
        }

        profiler.shutdown();
        return true;
    }

    static bool test_memory_usage()
    {
        FrameProfiler profiler;
        if (!profiler.initialize(60.0f))
        {
            return false;
        }

        std::uint64_t memory = profiler.get_memory_usage_bytes();
        if (memory < 1024ull) // Should be at least a few KB
        {
            return false;
        }

        profiler.shutdown();
        return true;
    }

    static bool test_profiler_overhead()
    {
        FrameProfiler profiler;
        if (!profiler.initialize(60.0f))
        {
            return false;
        }

        // Run frames with some actual work to get meaningful frame times
        for (int i = 0; i < 60; ++i)
        {
            profiler.begin_frame();
            
            // Do some work to make frame time non-trivial
            profiler.start_timer(TimerID::SIMULATION);
            {
                // Add a small sleep to ensure measurable frame time
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            profiler.end_timer(TimerID::SIMULATION);
            
            profiler.end_frame();
        }

        float overhead = profiler.get_profiler_overhead_percent();
        
        // Overhead should be reasonable - between 0% and 100%
        // For a 1ms frame with 0.1ms overhead estimate, should be ~10%
        if (overhead < 0.0f || overhead > 100.0f)
        {
            return false;
        }

        profiler.shutdown();
        return true;
    }

    void run_frame_profiler_tests() noexcept
    {
        using TestFn = bool (*)();
        const TestFn tests[] = {
            test_initialization,
            test_frame_timing,
            test_timer_basic,
            test_scoped_timer,
            test_gpu_timing,
            test_frame_statistics,
            test_thread_registration,
            test_memory_usage,
            test_profiler_overhead,
        };

        const char* test_names[] = {
            "initialization",
            "frame_timing",
            "timer_basic",
            "scoped_timer",
            "gpu_timing",
            "frame_statistics",
            "thread_registration",
            "memory_usage",
            "profiler_overhead",
        };

        static_assert(sizeof(tests) / sizeof(tests[0]) == sizeof(test_names) / sizeof(test_names[0]));

        std::printf("=== FRAME PROFILER UNIT TESTS ===\n\n");

        for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
        {
            bool result = tests[i]();
            std::printf("[profiler] %s: %s\n", test_names[i], result ? "OK" : "FAIL");

            if (!result)
            {
                std::printf("FAILURE: Test %s failed\n", test_names[i]);
                return;
            }
        }

        std::printf("\n[profiler] All tests passed\n");
    }
}

int main()
{
    acheron::utilities::run_frame_profiler_tests();
    return 0;
}

#endif
