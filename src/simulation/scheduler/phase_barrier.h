#pragma once

#include <atomic>
#include <cstdint>

namespace acheron::simulation::scheduler
{
    class JobSystem;

    static constexpr std::uint32_t MAX_PHASE_BARRIER_PHASES = 64;
    static constexpr std::uint32_t INVALID_PHASE_INDEX = 0xFFFF'FFFFu;

    struct PhaseBarrierStats
    {
        std::uint32_t phase_count = 0;
        std::uint32_t current_phase = 0;
        std::uint64_t waits = 0;
        std::uint64_t arrivals = 0;
    };

    class PhaseBarrier final
    {
    public:
        PhaseBarrier() noexcept = default;

        bool init(std::uint32_t phase_count) noexcept;
        void reset() noexcept;

        bool open_phase(std::uint32_t phase_index, std::uint32_t expected_arrivals) noexcept;
        bool arrive(std::uint32_t phase_index) noexcept;
        bool arrive_and_wait(std::uint32_t phase_index, JobSystem* job_system = nullptr) noexcept;

        bool wait_for_turn(std::uint32_t phase_index, JobSystem* job_system = nullptr) noexcept;
        bool wait_for_phase(std::uint32_t phase_index, JobSystem* job_system = nullptr) noexcept;
        bool wait_for_dependencies(const std::uint32_t* phases,
                                   std::uint32_t phase_count,
                                   JobSystem* job_system = nullptr) noexcept;

        bool is_phase_open(std::uint32_t phase_index) const noexcept;
        bool is_phase_complete(std::uint32_t phase_index) const noexcept;

        std::uint32_t phase_count() const noexcept;
        std::uint32_t current_phase() const noexcept;
        PhaseBarrierStats stats() const noexcept;

    private:
        struct alignas(64) PhaseState
        {
            std::atomic<std::uint32_t> expected{ 0 };
            std::atomic<std::uint32_t> arrived{ 0 };
            std::atomic<std::uint32_t> opened{ 0 };
        };

        bool phase_valid(std::uint32_t phase_index) const noexcept;
        void complete_phase(std::uint32_t phase_index) noexcept;
        void wait_spin(JobSystem* job_system) noexcept;

        PhaseState m_phases[MAX_PHASE_BARRIER_PHASES]{};
        std::atomic<std::uint32_t> m_phase_count{ 0 };
        std::atomic<std::uint32_t> m_current_phase{ 0 };
        std::atomic<std::uint64_t> m_waits{ 0 };
        std::atomic<std::uint64_t> m_arrivals{ 0 };
    };
}
