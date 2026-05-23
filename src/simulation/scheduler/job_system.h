#pragma once

#include "../../threading/thread.h"

#include <atomic>
#include <cstdint>

namespace acheron::simulation::scheduler
{
    static constexpr std::uint32_t MAX_JOB_WORKERS = 64;
    static constexpr std::uint32_t INVALID_WORKER_INDEX = 0xFFFF'FFFFu;
    static constexpr std::uint32_t INVALID_JOB_INDEX = 0xFFFF'FFFFu;

    using JobFunction = void (*)(void* user) noexcept;

    struct JobHandle
    {
        std::uint32_t index = INVALID_JOB_INDEX;
        std::uint32_t generation = 0;
    };

    constexpr bool is_valid(JobHandle handle) noexcept
    {
        return handle.index != INVALID_JOB_INDEX && handle.generation != 0u;
    }

    struct JobSystemConfig
    {
        std::uint32_t worker_count = 0;
        std::uint32_t job_capacity = 8192;
        std::uint32_t dependency_capacity = 32768;
        std::uint32_t worker_queue_capacity = 1024;
        bool pin_workers = true;
        const char* worker_name_prefix = "ach_worker";
    };

    struct JobSystemStats
    {
        std::uint32_t worker_count = 0;
        std::uint32_t outstanding_jobs = 0;
        std::uint64_t total_executed_jobs = 0;
        std::uint64_t total_steal_success = 0;
        std::uint64_t total_steal_miss = 0;
        std::uint64_t total_idle_loops = 0;
        std::uint64_t worker_executed_jobs[MAX_JOB_WORKERS]{};
    };

    class JobSystem final
    {
    public:
        JobSystem() noexcept = default;
        ~JobSystem() noexcept;

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        bool init(const JobSystemConfig& config) noexcept;
        void shutdown() noexcept;

        bool reset() noexcept;

        JobHandle create_job(JobFunction function, void* user) noexcept;
        bool add_dependency(JobHandle job, JobHandle dependency) noexcept;
        bool dispatch(JobHandle job) noexcept;

        bool is_complete(JobHandle job) const noexcept;
        void wait(JobHandle job) noexcept;
        void wait_idle() noexcept;

        bool help_one() noexcept;

        std::uint32_t worker_count() const noexcept;
        JobSystemStats stats() const noexcept;

        bool is_current_worker_thread() const noexcept;
        static std::uint32_t current_worker_index() noexcept;

    private:
        struct Storage;
        struct WorkerSlot;

        bool handle_valid(JobHandle job) const noexcept;
        bool try_schedule_ready(JobHandle job) noexcept;
        bool enqueue_ready(JobHandle job) noexcept;
        bool acquire_job(WorkerSlot& worker, JobHandle& out) noexcept;
        bool steal_job(std::uint32_t thief_index, JobHandle& out) noexcept;
        void execute_job(JobHandle job, WorkerSlot* worker) noexcept;
        void worker_loop(WorkerSlot& worker) noexcept;

        static void worker_entry(void* user) noexcept;

        Storage* m_storage = nullptr;
    };
}
