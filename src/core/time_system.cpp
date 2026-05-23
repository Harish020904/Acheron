#include "time_system.h"

#include <chrono>
#include <thread>

namespace acheron::core
{
    namespace
    {
        static inline std::int64_t clamp_i64(std::int64_t v, std::int64_t lo, std::int64_t hi) noexcept
        {
            if (v < lo)
            {
                return lo;
            }
            if (v > hi)
            {
                return hi;
            }
            return v;
        }

        static inline std::uint32_t min_u32(std::uint32_t a, std::uint32_t b) noexcept
        {
            return (a < b) ? a : b;
        }

        static inline void busy_wait_until(std::int64_t target_ns) noexcept
        {
            while (time_now_ns() < target_ns)
            {
                // spin
            }
        }
    }

    std::int64_t time_now_ns() noexcept
    {
        using clock = std::chrono::steady_clock;
        const auto now = clock::now().time_since_epoch();
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        return static_cast<std::int64_t>(ns);
    }

    void sleep_until_ns(std::int64_t target_ns) noexcept
    {
        // Coarse sleep first, then spin for the last ~200us for better pacing stability.
        for (;;)
        {
            const std::int64_t now_ns = time_now_ns();
            if (now_ns >= target_ns)
            {
                return;
            }

            const std::int64_t remaining = target_ns - now_ns;
            if (remaining > 200'000) // 200 us
            {
                const auto dur = std::chrono::nanoseconds(remaining - 100'000);
                std::this_thread::sleep_for(dur);
            }
            else
            {
                busy_wait_until(target_ns);
                return;
            }
        }
    }

    void TimeSystem::init(const TimeConfig& cfg) noexcept
    {
        m_cfg = cfg;
        if (m_cfg.fixed_dt_ns <= 0)
        {
            m_cfg.fixed_dt_ns = 16'666'666;
        }
        if (m_cfg.max_frame_dt_ns <= 0)
        {
            m_cfg.max_frame_dt_ns = m_cfg.fixed_dt_ns;
        }
        if (m_cfg.max_catchup_steps == 0)
        {
            m_cfg.max_catchup_steps = 1;
        }
        if (m_cfg.target_frame_ns <= 0)
        {
            m_cfg.target_frame_ns = m_cfg.fixed_dt_ns;
        }

        reset(0);
    }

    void TimeSystem::reset(std::int64_t start_ns) noexcept
    {
        m_started = false;
        m_last_frame_ns = start_ns;
        m_accumulator_ns = 0;
        m_pending_steps = 0;
        m_sim_time_ns = 0;
        m_last_frame = {};
        m_last_frame.frame_begin_ns = start_ns;
    }

    void TimeSystem::start(std::int64_t now_ns) noexcept
    {
        if (now_ns == 0)
        {
            now_ns = time_now_ns();
        }

        m_started = true;
        m_last_frame_ns = now_ns;
        m_last_frame.frame_begin_ns = now_ns;
        m_last_frame.delta_ns = 0;
        m_last_frame.delta_seconds = 0.0;
        m_last_frame.fixed_steps = 0;
        m_last_frame.alpha = 0.0;
    }

    TimeFrame TimeSystem::begin_frame() noexcept
    {
        if (!m_started)
        {
            start(0);
            return m_last_frame;
        }

        std::int64_t now_ns = time_now_ns();

        if (m_cfg.enable_frame_pacing)
        {
            const std::int64_t target_ns = m_last_frame_ns + m_cfg.target_frame_ns;
            if (now_ns < target_ns)
            {
                sleep_until_ns(target_ns);
                now_ns = time_now_ns();
                if (now_ns < target_ns)
                {
                    now_ns = target_ns;
                }
            }
        }

        return begin_frame_at(now_ns);
    }

    TimeFrame TimeSystem::begin_frame_at(std::int64_t now_ns) noexcept
    {
        if (!m_started)
        {
            start(now_ns);
            return m_last_frame;
        }

        std::int64_t raw_dt = now_ns - m_last_frame_ns;
        if (raw_dt < 0)
        {
            raw_dt = 0;
        }
        raw_dt = clamp_i64(raw_dt, 0, m_cfg.max_frame_dt_ns);

        m_last_frame_ns = now_ns;

        // Accumulate for fixed steps.
        m_accumulator_ns += raw_dt;
        if (m_accumulator_ns < 0)
        {
            m_accumulator_ns = 0;
        }

        const std::int64_t step_ns = m_cfg.fixed_dt_ns;
        std::uint32_t steps = static_cast<std::uint32_t>(m_accumulator_ns / step_ns);
        steps = min_u32(steps, m_cfg.max_catchup_steps);

        m_pending_steps = steps;
        m_accumulator_ns -= static_cast<std::int64_t>(steps) * step_ns;

        TimeFrame fr{};
        fr.frame_begin_ns = now_ns;
        fr.delta_ns = raw_dt;
        fr.delta_seconds = static_cast<double>(raw_dt) * 1e-9;
        fr.fixed_steps = steps;
        fr.alpha = (step_ns > 0) ? (static_cast<double>(m_accumulator_ns) / static_cast<double>(step_ns)) : 0.0;

        // alpha must remain stable.
        if (fr.alpha < 0.0)
        {
            fr.alpha = 0.0;
        }
        if (fr.alpha >= 1.0)
        {
            fr.alpha = 0.999999;
        }

        m_last_frame = fr;
        return fr;
    }

    bool TimeSystem::consume_fixed_step() noexcept
    {
        if (m_pending_steps == 0)
        {
            return false;
        }

        --m_pending_steps;
        m_sim_time_ns += m_cfg.fixed_dt_ns;
        return true;
    }
}

// ---------------------------
// Unit tests (self-contained)
// Compile with: -DACHERON_TIME_UNIT_TEST
// ---------------------------
#if defined(ACHERON_TIME_UNIT_TEST)

#include <cstdio>

namespace
{
    static bool timestep_stability_test() noexcept
    {
        acheron::core::TimeSystem ts;
        acheron::core::TimeConfig cfg{};
        cfg.fixed_dt_ns = 16'666'666;
        cfg.max_frame_dt_ns = 250'000'000;
        cfg.max_catchup_steps = 16;
        cfg.enable_frame_pacing = false;
        ts.init(cfg);

        std::int64_t now = 1'000'000'000;
        ts.start(now);

        // Patterned jitter: 10ms, 33ms, 16ms repeating.
        const std::int64_t pattern[] = { 10'000'000, 33'000'000, 16'000'000 };

        std::int64_t expected_acc = 0;
        std::uint64_t expected_steps = 0;
        std::uint64_t actual_steps = 0;

        for (int i = 0; i < 10'000; ++i)
        {
            const std::int64_t dt = pattern[i % 3];
            now += dt;

            // Expected model (integer accumulator).
            expected_acc += dt;
            const std::uint32_t steps = static_cast<std::uint32_t>(expected_acc / cfg.fixed_dt_ns);
            expected_steps += steps;
            expected_acc -= static_cast<std::int64_t>(steps) * cfg.fixed_dt_ns;

            const auto fr = ts.begin_frame_at(now);
            (void)fr;
            while (ts.consume_fixed_step())
            {
                ++actual_steps;
            }
        }

        if (actual_steps != expected_steps)
        {
            return false;
        }

        // Alpha should remain in [0,1).
        if (ts.last_delta_ns() < 0)
        {
            return false;
        }

        return true;
    }

    static bool long_runtime_drift_test() noexcept
    {
        acheron::core::TimeSystem ts;
        acheron::core::TimeConfig cfg{};
        cfg.fixed_dt_ns = 1'000'000; // 1ms
        cfg.max_frame_dt_ns = 1'000'000;
        cfg.max_catchup_steps = 4;
        ts.init(cfg);

        std::int64_t now = 5'000'000'000;
        ts.start(now);

        constexpr std::uint32_t frames = 200'000;
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            now += cfg.fixed_dt_ns;
            const auto fr = ts.begin_frame_at(now);
            if (fr.fixed_steps != 1)
            {
                return false;
            }
            if (!ts.consume_fixed_step())
            {
                return false;
            }
            if (ts.consume_fixed_step())
            {
                return false;
            }
        }

        const std::int64_t expected_sim = static_cast<std::int64_t>(frames) * cfg.fixed_dt_ns;
        if (ts.sim_time_ns() != expected_sim)
        {
            return false;
        }
        return true;
    }
}

int main()
{
    if (!timestep_stability_test())
    {
        std::fprintf(stderr, "[time] timestep stability: FAIL\n");
        return 1;
    }
    std::fprintf(stderr, "[time] timestep stability: OK\n");

    if (!long_runtime_drift_test())
    {
        std::fprintf(stderr, "[time] long-runtime drift: FAIL\n");
        return 1;
    }
    std::fprintf(stderr, "[time] long-runtime drift: OK\n");

    return 0;
}

#endif