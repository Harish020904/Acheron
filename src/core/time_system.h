#pragma once

#include <cstdint>

namespace acheron::core
{
    struct TimeConfig
    {
        // Fixed-step simulation timestep (nanoseconds). Default: 60 Hz.
        std::int64_t fixed_dt_ns = 16'666'666;

        // Clamp per-frame delta (nanoseconds) to avoid catch-up spirals.
        std::int64_t max_frame_dt_ns = 250'000'000; // 250 ms

        // Maximum fixed steps executed per rendered frame.
        std::uint32_t max_catchup_steps = 8;

        // Frame pacing (sleep/spin to target cadence). Affects presentation pacing, not determinism.
        bool enable_frame_pacing = false;
        std::int64_t target_frame_ns = 16'666'666; // 60 Hz
    };

    struct TimeFrame
    {
        std::int64_t frame_begin_ns = 0;
        std::int64_t delta_ns = 0;
        double delta_seconds = 0.0;

        std::uint32_t fixed_steps = 0;
        double alpha = 0.0; // accumulator / fixed_dt in [0, 1)
    };

    // Deterministic fixed-timestep accumulator + per-frame delta timing.
    // All internal state uses integer nanoseconds to avoid long-run drift.
    class TimeSystem final
    {
    public:
        void init(const TimeConfig& cfg) noexcept;
        void reset(std::int64_t start_ns = 0) noexcept;

        // Start real-time tracking. If now_ns == 0, uses the high precision clock.
        void start(std::int64_t now_ns = 0) noexcept;

        // Real-time begin frame (uses pacing if enabled).
        TimeFrame begin_frame() noexcept;

        // Deterministic begin frame at an explicit timestamp (no pacing). Intended for tests/replay.
        TimeFrame begin_frame_at(std::int64_t now_ns) noexcept;

        // Consume one fixed simulation step for the current frame.
        bool consume_fixed_step() noexcept;

        std::int64_t fixed_dt_ns() const noexcept { return m_cfg.fixed_dt_ns; }
        double fixed_dt_seconds() const noexcept { return static_cast<double>(m_cfg.fixed_dt_ns) * 1e-9; }

        std::int64_t sim_time_ns() const noexcept { return m_sim_time_ns; }
        double sim_time_seconds() const noexcept { return static_cast<double>(m_sim_time_ns) * 1e-9; }

        std::int64_t last_delta_ns() const noexcept { return m_last_frame.delta_ns; }
        double last_delta_seconds() const noexcept { return m_last_frame.delta_seconds; }

    private:
        TimeConfig m_cfg{};

        bool m_started = false;
        std::int64_t m_last_frame_ns = 0;

        std::int64_t m_accumulator_ns = 0;
        std::uint32_t m_pending_steps = 0;

        std::int64_t m_sim_time_ns = 0;
        TimeFrame m_last_frame{};
    };

    // High precision monotonic clock (nanoseconds).
    std::int64_t time_now_ns() noexcept;

    // Sleep/spin until the monotonic time reaches target_ns.
    void sleep_until_ns(std::int64_t target_ns) noexcept;
}