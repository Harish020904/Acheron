#include "phase_barrier.h"

#include "job_system.h"

#include <chrono>
#include <cstdio>
#include <thread>

namespace acheron::simulation::scheduler
{
    bool PhaseBarrier::init(std::uint32_t phase_count_value) noexcept
    {
        if (phase_count_value == 0u || phase_count_value > MAX_PHASE_BARRIER_PHASES)
        {
            return false;
        }

        m_phase_count.store(phase_count_value, std::memory_order_release);
        reset();
        return true;
    }

    void PhaseBarrier::reset() noexcept
    {
        const std::uint32_t count = m_phase_count.load(std::memory_order_acquire);
        for (std::uint32_t i = 0; i < MAX_PHASE_BARRIER_PHASES; ++i)
        {
            m_phases[i].expected.store(0, std::memory_order_relaxed);
            m_phases[i].arrived.store(0, std::memory_order_relaxed);
            m_phases[i].opened.store(0, std::memory_order_relaxed);
        }

        (void)count;
        m_current_phase.store(0, std::memory_order_release);
        m_waits.store(0, std::memory_order_release);
        m_arrivals.store(0, std::memory_order_release);
    }

    bool PhaseBarrier::open_phase(std::uint32_t phase_index, std::uint32_t expected_arrivals) noexcept
    {
        if (!phase_valid(phase_index))
        {
            return false;
        }

        if (m_current_phase.load(std::memory_order_acquire) != phase_index)
        {
            return false;
        }

        PhaseState& phase = m_phases[phase_index];
        std::uint32_t closed = 0;
        if (!phase.opened.compare_exchange_strong(closed, 1u, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            return false;
        }

        phase.arrived.store(0, std::memory_order_release);
        phase.expected.store(expected_arrivals, std::memory_order_release);

        if (expected_arrivals == 0u)
        {
            complete_phase(phase_index);
        }

        return true;
    }

    bool PhaseBarrier::arrive(std::uint32_t phase_index) noexcept
    {
        if (!phase_valid(phase_index))
        {
            return false;
        }

        PhaseState& phase = m_phases[phase_index];
        if (phase.opened.load(std::memory_order_acquire) == 0u)
        {
            return false;
        }

        const std::uint32_t expected = phase.expected.load(std::memory_order_acquire);
        if (expected == 0u)
        {
            return false;
        }

        std::uint32_t previous = phase.arrived.load(std::memory_order_acquire);
        for (;;)
        {
            if (previous >= expected)
            {
                return false;
            }

            if (phase.arrived.compare_exchange_weak(previous,
                                                    previous + 1u,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire))
            {
                break;
            }
        }

        m_arrivals.fetch_add(1, std::memory_order_relaxed);

        if ((previous + 1u) == expected)
        {
            complete_phase(phase_index);
        }
        return true;
    }

    bool PhaseBarrier::arrive_and_wait(std::uint32_t phase_index, JobSystem* job_system) noexcept
    {
        if (!arrive(phase_index))
        {
            return false;
        }
        return wait_for_phase(phase_index, job_system);
    }

    bool PhaseBarrier::wait_for_turn(std::uint32_t phase_index, JobSystem* job_system) noexcept
    {
        if (!phase_valid(phase_index))
        {
            return false;
        }

        while (m_current_phase.load(std::memory_order_acquire) < phase_index)
        {
            wait_spin(job_system);
        }

        return m_current_phase.load(std::memory_order_acquire) == phase_index;
    }

    bool PhaseBarrier::wait_for_phase(std::uint32_t phase_index, JobSystem* job_system) noexcept
    {
        if (!phase_valid(phase_index))
        {
            return false;
        }

        while (m_current_phase.load(std::memory_order_acquire) <= phase_index)
        {
            wait_spin(job_system);
        }

        return true;
    }

    bool PhaseBarrier::wait_for_dependencies(const std::uint32_t* phases,
                                             std::uint32_t phase_count_value,
                                             JobSystem* job_system) noexcept
    {
        if (!phases && phase_count_value != 0u)
        {
            return false;
        }

        for (std::uint32_t i = 0; i < phase_count_value; ++i)
        {
            if (!wait_for_phase(phases[i], job_system))
            {
                return false;
            }
        }
        return true;
    }

    bool PhaseBarrier::is_phase_open(std::uint32_t phase_index) const noexcept
    {
        if (!phase_valid(phase_index))
        {
            return false;
        }
        return m_phases[phase_index].opened.load(std::memory_order_acquire) != 0u;
    }

    bool PhaseBarrier::is_phase_complete(std::uint32_t phase_index) const noexcept
    {
        if (!phase_valid(phase_index))
        {
            return false;
        }
        return m_current_phase.load(std::memory_order_acquire) > phase_index;
    }

    std::uint32_t PhaseBarrier::phase_count() const noexcept
    {
        return m_phase_count.load(std::memory_order_acquire);
    }

    std::uint32_t PhaseBarrier::current_phase() const noexcept
    {
        return m_current_phase.load(std::memory_order_acquire);
    }

    PhaseBarrierStats PhaseBarrier::stats() const noexcept
    {
        PhaseBarrierStats out{};
        out.phase_count = phase_count();
        out.current_phase = current_phase();
        out.waits = m_waits.load(std::memory_order_acquire);
        out.arrivals = m_arrivals.load(std::memory_order_acquire);
        return out;
    }

    bool PhaseBarrier::phase_valid(std::uint32_t phase_index) const noexcept
    {
        return phase_index < m_phase_count.load(std::memory_order_acquire);
    }

    void PhaseBarrier::complete_phase(std::uint32_t phase_index) noexcept
    {
        std::uint32_t expected = phase_index;
        (void)m_current_phase.compare_exchange_strong(expected,
                                                      phase_index + 1u,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire);
    }

    void PhaseBarrier::wait_spin(JobSystem* job_system) noexcept
    {
        m_waits.fetch_add(1, std::memory_order_relaxed);
        if (job_system && job_system->help_one())
        {
            return;
        }
        std::this_thread::yield();
    }
}

#if defined(ACHERON_PHASE_BARRIER_UNIT_TEST)

#include <atomic>

namespace
{
    struct PhaseJob
    {
        acheron::simulation::scheduler::PhaseBarrier* barrier = nullptr;
        std::atomic<std::uint32_t>* counts = nullptr;
        std::atomic<bool>* valid = nullptr;
        std::uint32_t phase = 0;
        std::uint32_t jobs_per_phase = 0;
    };

    static void phase_job_entry(void* user) noexcept
    {
        auto* job = static_cast<PhaseJob*>(user);
        if (job->phase > 0u)
        {
            const std::uint32_t previous = job->counts[job->phase - 1u].load(std::memory_order_acquire);
            if (previous != job->jobs_per_phase)
            {
                job->valid->store(false, std::memory_order_release);
            }
        }

        job->counts[job->phase].fetch_add(1, std::memory_order_acq_rel);
        if (!job->barrier->arrive(job->phase))
        {
            job->valid->store(false, std::memory_order_release);
        }
    }

    static std::uint32_t run_replay_once() noexcept
    {
        constexpr std::uint32_t phase_count = 4;
        constexpr std::uint32_t jobs_per_phase = 64;
        constexpr std::uint32_t total_jobs = phase_count * jobs_per_phase;

        acheron::simulation::scheduler::JobSystem system;
        acheron::simulation::scheduler::JobSystemConfig cfg{};
        cfg.worker_count = 4;
        cfg.pin_workers = false;
        cfg.job_capacity = total_jobs + 16u;
        cfg.dependency_capacity = 16u;
        cfg.worker_queue_capacity = 128u;

        if (!system.init(cfg))
        {
            return 0u;
        }

        acheron::simulation::scheduler::PhaseBarrier barrier;
        if (!barrier.init(phase_count))
        {
            system.shutdown();
            return 0u;
        }

        static PhaseJob jobs[total_jobs];
        static acheron::simulation::scheduler::JobHandle handles[total_jobs];
        std::atomic<std::uint32_t> counts[phase_count];
        std::atomic<bool> valid{ true };
        for (std::uint32_t i = 0; i < phase_count; ++i)
        {
            counts[i].store(0, std::memory_order_relaxed);
        }

        std::uint32_t hash = 2166136261u;
        for (std::uint32_t phase = 0; phase < phase_count; ++phase)
        {
            if (!barrier.open_phase(phase, jobs_per_phase))
            {
                valid.store(false, std::memory_order_release);
                break;
            }

            for (std::uint32_t i = 0; i < jobs_per_phase; ++i)
            {
                const std::uint32_t index = phase * jobs_per_phase + i;
                jobs[index].barrier = &barrier;
                jobs[index].counts = counts;
                jobs[index].valid = &valid;
                jobs[index].phase = phase;
                jobs[index].jobs_per_phase = jobs_per_phase;
                handles[index] = system.create_job(&phase_job_entry, &jobs[index]);
                if (!system.dispatch(handles[index]))
                {
                    valid.store(false, std::memory_order_release);
                }
            }

            if (!barrier.wait_for_phase(phase, &system))
            {
                valid.store(false, std::memory_order_release);
            }

            const std::uint32_t count = counts[phase].load(std::memory_order_acquire);
            hash ^= count + phase * 16777619u;
            hash *= 16777619u;
        }

        system.wait_idle();
        const bool ok = valid.load(std::memory_order_acquire);
        system.shutdown();

        return ok ? hash : 0u;
    }

    static bool deterministic_replay_test() noexcept
    {
        const std::uint32_t a = run_replay_once();
        const std::uint32_t b = run_replay_once();
        return a != 0u && a == b;
    }

    static bool synchronization_correctness_test() noexcept
    {
        acheron::simulation::scheduler::PhaseBarrier barrier;
        if (!barrier.init(2))
        {
            return false;
        }

        if (!barrier.open_phase(0, 2))
        {
            return false;
        }

        std::atomic<std::uint32_t> passed{ 0 };
        std::thread a([&]() noexcept {
            if (barrier.arrive_and_wait(0))
            {
                passed.fetch_add(1, std::memory_order_acq_rel);
            }
        });
        std::thread b([&]() noexcept {
            if (barrier.arrive_and_wait(0))
            {
                passed.fetch_add(1, std::memory_order_acq_rel);
            }
        });

        a.join();
        b.join();

        if (passed.load(std::memory_order_acquire) != 2u || !barrier.is_phase_complete(0))
        {
            return false;
        }

        if (!barrier.open_phase(1, 0))
        {
            return false;
        }
        return barrier.is_phase_complete(1);
    }
}

int main()
{
    if (!synchronization_correctness_test())
    {
        std::printf("[phase_barrier] synchronization correctness: FAIL\n");
        return 1;
    }
    std::printf("[phase_barrier] synchronization correctness: OK\n");

    if (!deterministic_replay_test())
    {
        std::printf("[phase_barrier] deterministic replay: FAIL\n");
        return 1;
    }
    std::printf("[phase_barrier] deterministic replay: OK\n");
    return 0;
}

#endif
