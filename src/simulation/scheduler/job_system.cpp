#include "job_system.h"

#include "../../threading/lockfree_queue.h"

#include <climits>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <new>
#include <semaphore>
#include <thread>

namespace acheron::simulation::scheduler
{
    namespace
    {
        static constexpr std::uint32_t INVALID_LINK_INDEX = 0xFFFF'FFFFu;
        static constexpr std::uint32_t MIN_JOB_CAPACITY = 64u;
        static constexpr std::uint32_t MIN_DEPENDENCY_CAPACITY = 64u;
        static constexpr std::uint32_t MIN_QUEUE_CAPACITY = 64u;
        static constexpr std::uint32_t MAX_QUEUE_CAPACITY = 65536u;

        enum class JobState : std::uint32_t
        {
            Created = 0,
            Waiting = 1,
            Queued = 2,
            Running = 3,
            Completed = 4,
        };

        thread_local JobSystem* tls_job_system = nullptr;
        thread_local std::uint32_t tls_worker_index = INVALID_WORKER_INDEX;

        static std::uint32_t clamp_u32(std::uint32_t value, std::uint32_t lo, std::uint32_t hi) noexcept
        {
            if (value < lo)
            {
                return lo;
            }
            if (value > hi)
            {
                return hi;
            }
            return value;
        }

        static std::uint32_t next_power_of_two(std::uint32_t value) noexcept
        {
            if (value <= 2u)
            {
                return 2u;
            }

            --value;
            value |= value >> 1u;
            value |= value >> 2u;
            value |= value >> 4u;
            value |= value >> 8u;
            value |= value >> 16u;
            return value + 1u;
        }

        static void make_worker_name(char* out,
                                     std::uint32_t out_bytes,
                                     const char* prefix,
                                     std::uint32_t index) noexcept
        {
            if (!out || out_bytes == 0u)
            {
                return;
            }

            if (!prefix || prefix[0] == '\0')
            {
                prefix = "ach_worker";
            }

            std::snprintf(out, out_bytes, "%s_%u", prefix, index);
            out[out_bytes - 1u] = '\0';
        }
    }

    struct alignas(threading::CACHE_LINE_BYTES) WorkStealingDeque
    {
        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint64_t> top{ 0 };
        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint64_t> bottom{ 0 };
        JobHandle* jobs = nullptr;
        std::uint32_t capacity = 0;
        std::uint32_t mask = 0;

        bool init(JobHandle* storage, std::uint32_t queue_capacity) noexcept
        {
            if (!storage || queue_capacity < 2u || !threading::is_power_of_two(queue_capacity))
            {
                return false;
            }

            jobs = storage;
            capacity = queue_capacity;
            mask = queue_capacity - 1u;
            top.store(0, std::memory_order_relaxed);
            bottom.store(0, std::memory_order_relaxed);
            for (std::uint32_t i = 0; i < capacity; ++i)
            {
                jobs[i] = {};
            }
            return true;
        }

        bool push_bottom(JobHandle job) noexcept
        {
            const std::uint64_t b = bottom.load(std::memory_order_relaxed);
            const std::uint64_t t = top.load(std::memory_order_acquire);
            if ((b - t) >= capacity)
            {
                return false;
            }

            jobs[b & mask] = job;
            std::atomic_thread_fence(std::memory_order_release);
            bottom.store(b + 1u, std::memory_order_release);
            return true;
        }

        bool pop_bottom(JobHandle& out) noexcept
        {
            std::uint64_t b = bottom.load(std::memory_order_relaxed);
            if (b == 0u)
            {
                return false;
            }

            b -= 1u;
            bottom.store(b, std::memory_order_seq_cst);
            const std::uint64_t t = top.load(std::memory_order_seq_cst);

            if (t <= b)
            {
                out = jobs[b & mask];
                if (t == b)
                {
                    std::uint64_t expected_top = t;
                    if (!top.compare_exchange_strong(expected_top,
                                                     t + 1u,
                                                     std::memory_order_seq_cst,
                                                     std::memory_order_relaxed))
                    {
                        bottom.store(b + 1u, std::memory_order_relaxed);
                        return false;
                    }
                    bottom.store(b + 1u, std::memory_order_relaxed);
                }
                return true;
            }

            bottom.store(b + 1u, std::memory_order_relaxed);
            return false;
        }

        bool steal(JobHandle& out) noexcept
        {
            std::uint64_t t = top.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            const std::uint64_t b = bottom.load(std::memory_order_acquire);
            if (t >= b)
            {
                return false;
            }

            out = jobs[t & mask];
            return top.compare_exchange_strong(t,
                                               t + 1u,
                                               std::memory_order_seq_cst,
                                               std::memory_order_relaxed);
        }
    };

    struct JobRecord
    {
        JobFunction function = nullptr;
        void* user = nullptr;
        std::atomic<std::uint32_t> unfinished_dependencies{ 0 };
        std::atomic<std::uint32_t> first_dependent{ INVALID_LINK_INDEX };
        std::atomic<std::uint32_t> state{ static_cast<std::uint32_t>(JobState::Completed) };
        std::uint32_t generation = 0;
    };

    struct DependentLink
    {
        JobHandle dependent{};
        std::uint32_t next = INVALID_LINK_INDEX;
    };

    struct alignas(threading::CACHE_LINE_BYTES) JobSystem::WorkerSlot
    {
        JobSystem* owner = nullptr;
        std::uint32_t index = INVALID_WORKER_INDEX;
        threading::WorkerThread thread{};

        WorkStealingDeque deque{};
        JobHandle* deque_storage = nullptr;

        threading::MpscBoundedQueue<JobHandle> inbound{};
        threading::MpscQueueSlot<JobHandle>* inbound_storage = nullptr;

        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint64_t> executed_jobs{ 0 };
        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint64_t> steal_success{ 0 };
        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint64_t> steal_miss{ 0 };
        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint64_t> idle_loops{ 0 };

        char name[threading::MAX_THREAD_NAME_BYTES]{};
    };

    struct JobSystem::Storage
    {
        std::atomic<bool> initialized{ false };
        std::atomic<bool> running{ false };

        std::uint32_t worker_count = 0;
        std::uint32_t job_capacity = 0;
        std::uint32_t dependency_capacity = 0;
        std::uint32_t worker_queue_capacity = 0;
        bool pin_workers = true;

        WorkerSlot* workers = nullptr;
        JobRecord* jobs = nullptr;
        DependentLink* links = nullptr;

        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint32_t> next_job{ 0 };
        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint32_t> next_link{ 0 };
        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint32_t> outstanding_jobs{ 0 };
        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint32_t> generation{ 1 };
        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint32_t> external_ticket{ 0 };

        alignas(threading::CACHE_LINE_BYTES) std::atomic<std::uint64_t> total_executed{ 0 };

        std::counting_semaphore<INT_MAX> wake{ 0 };
    };

    JobSystem::~JobSystem() noexcept
    {
        shutdown();
    }

    bool JobSystem::init(const JobSystemConfig& config) noexcept
    {
        if (m_storage)
        {
            return true;
        }

        Storage* storage = new (std::nothrow) Storage();
        if (!storage)
        {
            return false;
        }

        const std::uint32_t hw = threading::hardware_thread_count();
        std::uint32_t workers = config.worker_count;
        if (workers == 0u)
        {
            workers = (hw > 2u) ? (hw - 1u) : 1u;
        }
        workers = clamp_u32(workers, 1u, MAX_JOB_WORKERS);

        storage->worker_count = workers;
        storage->job_capacity = (config.job_capacity < MIN_JOB_CAPACITY) ? MIN_JOB_CAPACITY : config.job_capacity;
        storage->dependency_capacity =
            (config.dependency_capacity < MIN_DEPENDENCY_CAPACITY) ? MIN_DEPENDENCY_CAPACITY : config.dependency_capacity;

        const std::uint32_t requested_queue = clamp_u32(config.worker_queue_capacity,
                                                        MIN_QUEUE_CAPACITY,
                                                        MAX_QUEUE_CAPACITY);
        storage->worker_queue_capacity = next_power_of_two(requested_queue);
        storage->pin_workers = config.pin_workers;

        storage->jobs = new (std::nothrow) JobRecord[storage->job_capacity];
        storage->links = new (std::nothrow) DependentLink[storage->dependency_capacity];
        storage->workers = new (std::nothrow) WorkerSlot[storage->worker_count];

        if (!storage->jobs || !storage->links || !storage->workers)
        {
            delete[] storage->workers;
            delete[] storage->links;
            delete[] storage->jobs;
            delete storage;
            return false;
        }

        for (std::uint32_t i = 0; i < storage->worker_count; ++i)
        {
            WorkerSlot& worker = storage->workers[i];
            worker.owner = this;
            worker.index = i;
            make_worker_name(worker.name, threading::MAX_THREAD_NAME_BYTES, config.worker_name_prefix, i);

            worker.deque_storage = new (std::nothrow) JobHandle[storage->worker_queue_capacity];
            worker.inbound_storage =
                new (std::nothrow) threading::MpscQueueSlot<JobHandle>[storage->worker_queue_capacity];

            if (!worker.deque_storage || !worker.inbound_storage)
            {
                m_storage = storage;
                shutdown();
                return false;
            }

            if (!worker.deque.init(worker.deque_storage, storage->worker_queue_capacity))
            {
                m_storage = storage;
                shutdown();
                return false;
            }

            if (!worker.inbound.init(worker.inbound_storage, storage->worker_queue_capacity))
            {
                m_storage = storage;
                shutdown();
                return false;
            }
        }

        storage->running.store(true, std::memory_order_release);
        m_storage = storage;

        for (std::uint32_t i = 0; i < storage->worker_count; ++i)
        {
            WorkerSlot& worker = storage->workers[i];
            threading::ThreadConfig thread_cfg{};
            thread_cfg.name = worker.name;
            if (storage->pin_workers)
            {
                thread_cfg.affinity.cpu_index = i % hw;
            }

            const threading::ThreadStartResult started = worker.thread.start(thread_cfg, &JobSystem::worker_entry, &worker);
            if (!started.started)
            {
                shutdown();
                return false;
            }
        }

        storage->initialized.store(true, std::memory_order_release);
        return true;
    }

    void JobSystem::shutdown() noexcept
    {
        Storage* storage = m_storage;
        if (!storage)
        {
            return;
        }

        if (storage->running.load(std::memory_order_acquire))
        {
            wait_idle();
        }

        storage->running.store(false, std::memory_order_release);
        for (std::uint32_t i = 0; i < storage->worker_count; ++i)
        {
            storage->wake.release();
        }

        if (storage->workers)
        {
            for (std::uint32_t i = 0; i < storage->worker_count; ++i)
            {
                storage->workers[i].thread.request_stop();
            }
            for (std::uint32_t i = 0; i < storage->worker_count; ++i)
            {
                storage->workers[i].thread.join();
                delete[] storage->workers[i].inbound_storage;
                delete[] storage->workers[i].deque_storage;
                storage->workers[i].inbound_storage = nullptr;
                storage->workers[i].deque_storage = nullptr;
            }
        }

        delete[] storage->workers;
        delete[] storage->links;
        delete[] storage->jobs;
        delete storage;
        m_storage = nullptr;
    }

    bool JobSystem::reset() noexcept
    {
        Storage* storage = m_storage;
        if (!storage || storage->outstanding_jobs.load(std::memory_order_acquire) != 0u)
        {
            return false;
        }

        storage->next_job.store(0, std::memory_order_release);
        storage->next_link.store(0, std::memory_order_release);
        std::uint32_t next_generation = storage->generation.load(std::memory_order_relaxed) + 1u;
        if (next_generation == 0u)
        {
            next_generation = 1u;
        }
        storage->generation.store(next_generation, std::memory_order_release);
        return true;
    }

    JobHandle JobSystem::create_job(JobFunction function, void* user) noexcept
    {
        Storage* storage = m_storage;
        if (!storage || !function)
        {
            return {};
        }

        const std::uint32_t index = storage->next_job.fetch_add(1, std::memory_order_acq_rel);
        if (index >= storage->job_capacity)
        {
            return {};
        }

        JobRecord& job = storage->jobs[index];
        job.function = function;
        job.user = user;
        job.unfinished_dependencies.store(0, std::memory_order_relaxed);
        job.first_dependent.store(INVALID_LINK_INDEX, std::memory_order_relaxed);
        job.generation = storage->generation.load(std::memory_order_acquire);
        job.state.store(static_cast<std::uint32_t>(JobState::Created), std::memory_order_release);

        return JobHandle{ index, job.generation };
    }

    bool JobSystem::add_dependency(JobHandle job_handle, JobHandle dependency_handle) noexcept
    {
        Storage* storage = m_storage;
        if (!storage || !handle_valid(job_handle) || !handle_valid(dependency_handle))
        {
            return false;
        }

        JobRecord& job = storage->jobs[job_handle.index];
        JobRecord& dependency = storage->jobs[dependency_handle.index];

        if (job.state.load(std::memory_order_acquire) != static_cast<std::uint32_t>(JobState::Created) ||
            dependency.state.load(std::memory_order_acquire) != static_cast<std::uint32_t>(JobState::Created))
        {
            return false;
        }

        const std::uint32_t link_index = storage->next_link.fetch_add(1, std::memory_order_acq_rel);
        if (link_index >= storage->dependency_capacity)
        {
            return false;
        }

        DependentLink& link = storage->links[link_index];
        link.dependent = job_handle;

        std::uint32_t old_head = dependency.first_dependent.load(std::memory_order_acquire);
        do
        {
            link.next = old_head;
        } while (!dependency.first_dependent.compare_exchange_weak(old_head,
                                                                   link_index,
                                                                   std::memory_order_release,
                                                                   std::memory_order_acquire));

        job.unfinished_dependencies.fetch_add(1, std::memory_order_acq_rel);
        return true;
    }

    bool JobSystem::dispatch(JobHandle job_handle) noexcept
    {
        Storage* storage = m_storage;
        if (!storage || !handle_valid(job_handle))
        {
            return false;
        }

        JobRecord& job = storage->jobs[job_handle.index];
        std::uint32_t expected = static_cast<std::uint32_t>(JobState::Created);
        if (job.state.compare_exchange_strong(expected,
                                              static_cast<std::uint32_t>(JobState::Waiting),
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire))
        {
            storage->outstanding_jobs.fetch_add(1, std::memory_order_acq_rel);
            return try_schedule_ready(job_handle);
        }

        return expected == static_cast<std::uint32_t>(JobState::Waiting) ||
               expected == static_cast<std::uint32_t>(JobState::Queued) ||
               expected == static_cast<std::uint32_t>(JobState::Running);
    }

    bool JobSystem::is_complete(JobHandle job_handle) const noexcept
    {
        if (!handle_valid(job_handle))
        {
            return false;
        }

        const JobRecord& job = m_storage->jobs[job_handle.index];
        return job.state.load(std::memory_order_acquire) == static_cast<std::uint32_t>(JobState::Completed);
    }

    void JobSystem::wait(JobHandle job_handle) noexcept
    {
        while (handle_valid(job_handle) && !is_complete(job_handle))
        {
            if (!help_one())
            {
                if (m_storage)
                {
                    m_storage->wake.release();
                }
                std::this_thread::yield();
            }
        }
    }

    void JobSystem::wait_idle() noexcept
    {
        Storage* storage = m_storage;
        if (!storage)
        {
            return;
        }

        while (storage->outstanding_jobs.load(std::memory_order_acquire) != 0u)
        {
            if (!help_one())
            {
                storage->wake.release();
                std::this_thread::yield();
            }
        }
    }

    bool JobSystem::help_one() noexcept
    {
        Storage* storage = m_storage;
        if (!storage)
        {
            return false;
        }

        JobHandle job{};
        WorkerSlot* worker = nullptr;

        if (tls_job_system == this && tls_worker_index < storage->worker_count)
        {
            worker = &storage->workers[tls_worker_index];
            if (!acquire_job(*worker, job))
            {
                return false;
            }
        }
        else
        {
            if (!steal_job(INVALID_WORKER_INDEX, job))
            {
                return false;
            }
        }

        execute_job(job, worker);
        return true;
    }

    std::uint32_t JobSystem::worker_count() const noexcept
    {
        return m_storage ? m_storage->worker_count : 0u;
    }

    JobSystemStats JobSystem::stats() const noexcept
    {
        JobSystemStats out{};
        const Storage* storage = m_storage;
        if (!storage)
        {
            return out;
        }

        out.worker_count = storage->worker_count;
        out.outstanding_jobs = storage->outstanding_jobs.load(std::memory_order_acquire);
        out.total_executed_jobs = storage->total_executed.load(std::memory_order_acquire);

        for (std::uint32_t i = 0; i < storage->worker_count; ++i)
        {
            const WorkerSlot& worker = storage->workers[i];
            const std::uint64_t executed = worker.executed_jobs.load(std::memory_order_acquire);
            out.worker_executed_jobs[i] = executed;
            out.total_steal_success += worker.steal_success.load(std::memory_order_acquire);
            out.total_steal_miss += worker.steal_miss.load(std::memory_order_acquire);
            out.total_idle_loops += worker.idle_loops.load(std::memory_order_acquire);
        }

        return out;
    }

    bool JobSystem::is_current_worker_thread() const noexcept
    {
        return tls_job_system == this && tls_worker_index != INVALID_WORKER_INDEX;
    }

    std::uint32_t JobSystem::current_worker_index() noexcept
    {
        return tls_worker_index;
    }

    bool JobSystem::handle_valid(JobHandle job_handle) const noexcept
    {
        const Storage* storage = m_storage;
        if (!storage || !is_valid(job_handle) || job_handle.index >= storage->job_capacity)
        {
            return false;
        }

        const JobRecord& job = storage->jobs[job_handle.index];
        return job.generation == job_handle.generation;
    }

    bool JobSystem::try_schedule_ready(JobHandle job_handle) noexcept
    {
        if (!handle_valid(job_handle))
        {
            return false;
        }

        JobRecord& job = m_storage->jobs[job_handle.index];
        if (job.unfinished_dependencies.load(std::memory_order_acquire) != 0u)
        {
            return true;
        }

        std::uint32_t expected = static_cast<std::uint32_t>(JobState::Waiting);
        if (job.state.compare_exchange_strong(expected,
                                              static_cast<std::uint32_t>(JobState::Queued),
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire))
        {
            return enqueue_ready(job_handle);
        }

        return expected == static_cast<std::uint32_t>(JobState::Queued) ||
               expected == static_cast<std::uint32_t>(JobState::Running) ||
               expected == static_cast<std::uint32_t>(JobState::Completed) ||
               expected == static_cast<std::uint32_t>(JobState::Created);
    }

    bool JobSystem::enqueue_ready(JobHandle job_handle) noexcept
    {
        Storage* storage = m_storage;
        if (!storage || storage->worker_count == 0u)
        {
            return false;
        }

        if (tls_job_system == this && tls_worker_index < storage->worker_count)
        {
            WorkerSlot& local = storage->workers[tls_worker_index];
            if (local.deque.push_bottom(job_handle))
            {
                storage->wake.release();
                return true;
            }
        }

        for (;;)
        {
            const std::uint32_t ticket = storage->external_ticket.fetch_add(1, std::memory_order_acq_rel);
            const std::uint32_t start = ticket % storage->worker_count;

            for (std::uint32_t i = 0; i < storage->worker_count; ++i)
            {
                WorkerSlot& target = storage->workers[(start + i) % storage->worker_count];
                if (target.inbound.try_push(job_handle))
                {
                    storage->wake.release();
                    return true;
                }
            }

            if (!storage->running.load(std::memory_order_acquire))
            {
                return false;
            }

            std::this_thread::yield();
        }
    }

    bool JobSystem::acquire_job(WorkerSlot& worker, JobHandle& out) noexcept
    {
        if (worker.deque.pop_bottom(out))
        {
            return true;
        }
        if (worker.inbound.try_pop(out))
        {
            return true;
        }
        return steal_job(worker.index, out);
    }

    bool JobSystem::steal_job(std::uint32_t thief_index, JobHandle& out) noexcept
    {
        Storage* storage = m_storage;
        if (!storage)
        {
            return false;
        }

        const std::uint32_t start = (thief_index == INVALID_WORKER_INDEX)
            ? 0u
            : ((thief_index + 1u) % storage->worker_count);

        for (std::uint32_t i = 0; i < storage->worker_count; ++i)
        {
            const std::uint32_t victim_index = (start + i) % storage->worker_count;
            if (victim_index == thief_index)
            {
                continue;
            }

            WorkerSlot& victim = storage->workers[victim_index];
            if (victim.deque.steal(out))
            {
                if (thief_index < storage->worker_count)
                {
                    storage->workers[thief_index].steal_success.fetch_add(1, std::memory_order_relaxed);
                }
                return true;
            }
        }

        if (thief_index < storage->worker_count)
        {
            storage->workers[thief_index].steal_miss.fetch_add(1, std::memory_order_relaxed);
        }
        return false;
    }

    void JobSystem::execute_job(JobHandle job_handle, WorkerSlot* worker) noexcept
    {
        Storage* storage = m_storage;
        if (!storage || !handle_valid(job_handle))
        {
            return;
        }

        JobRecord& job = storage->jobs[job_handle.index];
        std::uint32_t expected = static_cast<std::uint32_t>(JobState::Queued);
        if (!job.state.compare_exchange_strong(expected,
                                               static_cast<std::uint32_t>(JobState::Running),
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire))
        {
            return;
        }

        JobFunction fn = job.function;
        if (fn)
        {
            fn(job.user);
        }

        job.state.store(static_cast<std::uint32_t>(JobState::Completed), std::memory_order_release);

        storage->total_executed.fetch_add(1, std::memory_order_relaxed);
        if (worker)
        {
            worker->executed_jobs.fetch_add(1, std::memory_order_relaxed);
        }

        std::uint32_t link_index = job.first_dependent.load(std::memory_order_acquire);
        while (link_index != INVALID_LINK_INDEX && link_index < storage->dependency_capacity)
        {
            const DependentLink& link = storage->links[link_index];
            if (handle_valid(link.dependent))
            {
                JobRecord& dependent = storage->jobs[link.dependent.index];
                const std::uint32_t previous =
                    dependent.unfinished_dependencies.fetch_sub(1, std::memory_order_acq_rel);
                if (previous == 1u)
                {
                    (void)try_schedule_ready(link.dependent);
                }
            }
            link_index = link.next;
        }

        storage->outstanding_jobs.fetch_sub(1, std::memory_order_acq_rel);
        storage->wake.release();
    }

    void JobSystem::worker_loop(WorkerSlot& worker) noexcept
    {
        tls_job_system = this;
        tls_worker_index = worker.index;

        Storage* storage = m_storage;
        while (storage && storage->running.load(std::memory_order_acquire))
        {
            JobHandle job{};
            if (acquire_job(worker, job))
            {
                execute_job(job, &worker);
                continue;
            }

            worker.idle_loops.fetch_add(1, std::memory_order_relaxed);
            (void)storage->wake.try_acquire_for(std::chrono::microseconds(200));
        }

        tls_worker_index = INVALID_WORKER_INDEX;
        tls_job_system = nullptr;
    }

    void JobSystem::worker_entry(void* user) noexcept
    {
        auto* worker = static_cast<WorkerSlot*>(user);
        if (worker && worker->owner)
        {
            worker->owner->worker_loop(*worker);
        }
    }
}

#if defined(ACHERON_JOB_SYSTEM_UNIT_TEST)

#include <atomic>

namespace
{
    struct IncrementJob
    {
        std::atomic<std::uint32_t>* counter = nullptr;
        std::atomic<std::uint32_t>* worker_hits = nullptr;
        std::uint32_t spin_iterations = 0;
    };

    static void increment_entry(void* user) noexcept
    {
        auto* job = static_cast<IncrementJob*>(user);
        for (std::uint32_t i = 0; i < job->spin_iterations; ++i)
        {
            std::atomic_signal_fence(std::memory_order_acq_rel);
        }
        job->counter->fetch_add(1, std::memory_order_acq_rel);

        const std::uint32_t worker = acheron::simulation::scheduler::JobSystem::current_worker_index();
        if (worker != acheron::simulation::scheduler::INVALID_WORKER_INDEX && worker < acheron::simulation::scheduler::MAX_JOB_WORKERS)
        {
            job->worker_hits[worker].fetch_add(1, std::memory_order_acq_rel);
        }
    }

    struct DependencyProbe
    {
        std::atomic<std::uint32_t> stage{ 0 };
        std::atomic<bool> valid{ true };
    };

    struct SpawnContext
    {
        acheron::simulation::scheduler::JobSystem* system = nullptr;
        IncrementJob* jobs = nullptr;
        acheron::simulation::scheduler::JobHandle* handles = nullptr;
        std::atomic<std::uint32_t>* counter = nullptr;
        std::atomic<std::uint32_t>* worker_hits = nullptr;
        std::uint32_t count = 0;
        std::atomic<bool>* valid = nullptr;
    };

    static void dependency_root(void* user) noexcept
    {
        auto* probe = static_cast<DependencyProbe*>(user);
        probe->stage.store(1, std::memory_order_release);
    }

    static void dependency_child(void* user) noexcept
    {
        auto* probe = static_cast<DependencyProbe*>(user);
        if (probe->stage.load(std::memory_order_acquire) != 1u)
        {
            probe->valid.store(false, std::memory_order_release);
        }
        probe->stage.store(2, std::memory_order_release);
    }

    static void spawn_children_entry(void* user) noexcept
    {
        auto* ctx = static_cast<SpawnContext*>(user);
        for (std::uint32_t i = 0; i < ctx->count; ++i)
        {
            ctx->jobs[i].counter = ctx->counter;
            ctx->jobs[i].worker_hits = ctx->worker_hits;
            ctx->jobs[i].spin_iterations = 512u;
            ctx->handles[i] = ctx->system->create_job(&increment_entry, &ctx->jobs[i]);
            if (!acheron::simulation::scheduler::is_valid(ctx->handles[i]) ||
                !ctx->system->dispatch(ctx->handles[i]))
            {
                ctx->valid->store(false, std::memory_order_release);
            }
        }
    }

    static bool scalability_benchmark() noexcept
    {
        constexpr std::uint32_t job_count = 4096;
        static IncrementJob jobs[job_count];
        static acheron::simulation::scheduler::JobHandle handles[job_count];
        static std::atomic<std::uint32_t> worker_hits[acheron::simulation::scheduler::MAX_JOB_WORKERS];

        std::atomic<std::uint32_t> counter{ 0 };
        for (std::uint32_t i = 0; i < acheron::simulation::scheduler::MAX_JOB_WORKERS; ++i)
        {
            worker_hits[i].store(0, std::memory_order_relaxed);
        }

        acheron::simulation::scheduler::JobSystem system;
        acheron::simulation::scheduler::JobSystemConfig cfg{};
        cfg.worker_count = 4;
        cfg.job_capacity = job_count + 16u;
        cfg.dependency_capacity = 1024;
        cfg.worker_queue_capacity = 512;
        cfg.pin_workers = false;

        if (!system.init(cfg))
        {
            return false;
        }

        const auto begin = std::chrono::steady_clock::now();
        for (std::uint32_t i = 0; i < job_count; ++i)
        {
            jobs[i].counter = &counter;
            jobs[i].worker_hits = worker_hits;
            jobs[i].spin_iterations = 0;
            handles[i] = system.create_job(&increment_entry, &jobs[i]);
            if (!acheron::simulation::scheduler::is_valid(handles[i]) || !system.dispatch(handles[i]))
            {
                system.shutdown();
                return false;
            }
        }

        system.wait_idle();
        const auto end = std::chrono::steady_clock::now();

        const auto stats = system.stats();
        system.shutdown();

        if (counter.load(std::memory_order_acquire) != job_count)
        {
            return false;
        }

        std::uint32_t active_workers = 0;
        for (std::uint32_t i = 0; i < stats.worker_count; ++i)
        {
            if (worker_hits[i].load(std::memory_order_acquire) != 0u)
            {
                ++active_workers;
            }
        }

        const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        std::printf("[job_system] %u jobs on %u workers: %lld us, active workers=%u, steals=%llu\n",
                    job_count,
                    stats.worker_count,
                    static_cast<long long>(elapsed_us),
                    active_workers,
                    static_cast<unsigned long long>(stats.total_steal_success));

        return active_workers >= 2u && stats.total_executed_jobs == job_count;
    }

    static bool work_stealing_validation() noexcept
    {
        constexpr std::uint32_t child_count = 2048;
        static IncrementJob jobs[child_count];
        static acheron::simulation::scheduler::JobHandle handles[child_count];
        static std::atomic<std::uint32_t> worker_hits[acheron::simulation::scheduler::MAX_JOB_WORKERS];

        for (std::uint32_t i = 0; i < acheron::simulation::scheduler::MAX_JOB_WORKERS; ++i)
        {
            worker_hits[i].store(0, std::memory_order_relaxed);
        }

        std::atomic<std::uint32_t> counter{ 0 };
        std::atomic<bool> valid{ true };

        acheron::simulation::scheduler::JobSystem system;
        acheron::simulation::scheduler::JobSystemConfig cfg{};
        cfg.worker_count = 4;
        cfg.pin_workers = false;
        cfg.job_capacity = child_count + 8u;
        cfg.dependency_capacity = 16u;
        cfg.worker_queue_capacity = 4096u;

        if (!system.init(cfg))
        {
            return false;
        }

        SpawnContext ctx{};
        ctx.system = &system;
        ctx.jobs = jobs;
        ctx.handles = handles;
        ctx.counter = &counter;
        ctx.worker_hits = worker_hits;
        ctx.count = child_count;
        ctx.valid = &valid;

        const auto root = system.create_job(&spawn_children_entry, &ctx);
        if (!system.dispatch(root))
        {
            system.shutdown();
            return false;
        }

        system.wait_idle();
        const auto stats = system.stats();
        system.shutdown();

        std::uint32_t active_workers = 0;
        for (std::uint32_t i = 0; i < stats.worker_count; ++i)
        {
            if (worker_hits[i].load(std::memory_order_acquire) != 0u)
            {
                ++active_workers;
            }
        }

        std::printf("[job_system] work stealing: active workers=%u, steals=%llu\n",
                    active_workers,
                    static_cast<unsigned long long>(stats.total_steal_success));

        return valid.load(std::memory_order_acquire) &&
               counter.load(std::memory_order_acquire) == child_count &&
               active_workers >= 2u &&
               stats.total_steal_success > 0u;
    }

    static bool dependency_tracking_validation() noexcept
    {
        acheron::simulation::scheduler::JobSystem system;
        acheron::simulation::scheduler::JobSystemConfig cfg{};
        cfg.worker_count = 2;
        cfg.pin_workers = false;
        cfg.job_capacity = 16;
        cfg.dependency_capacity = 16;
        cfg.worker_queue_capacity = 64;

        if (!system.init(cfg))
        {
            return false;
        }

        DependencyProbe probe{};
        const auto root = system.create_job(&dependency_root, &probe);
        const auto child = system.create_job(&dependency_child, &probe);
        const bool ok =
            acheron::simulation::scheduler::is_valid(root) &&
            acheron::simulation::scheduler::is_valid(child) &&
            system.add_dependency(child, root) &&
            system.dispatch(child) &&
            system.dispatch(root);

        if (!ok)
        {
            system.shutdown();
            return false;
        }

        system.wait(child);
        system.wait_idle();
        const bool result = probe.valid.load(std::memory_order_acquire) &&
                            probe.stage.load(std::memory_order_acquire) == 2u;
        system.shutdown();
        return result;
    }
}

int main()
{
    if (!dependency_tracking_validation())
    {
        std::printf("[job_system] dependency tracking: FAIL\n");
        return 1;
    }
    std::printf("[job_system] dependency tracking: OK\n");

    if (!scalability_benchmark())
    {
        std::printf("[job_system] scalability benchmark: FAIL\n");
        return 1;
    }
    std::printf("[job_system] scalability benchmark: OK\n");

    if (!work_stealing_validation())
    {
        std::printf("[job_system] work stealing: FAIL\n");
        return 1;
    }
    std::printf("[job_system] work stealing: OK\n");
    return 0;
}

#endif
