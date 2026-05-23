#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

namespace acheron::threading
{
    static constexpr std::uint32_t MAX_THREAD_NAME_BYTES = 64;
    static constexpr std::uint32_t INVALID_CPU_INDEX = 0xFFFF'FFFFu;

    using ThreadEntry = void (*)(void* user) noexcept;

    struct ThreadAffinity
    {
        std::uint32_t cpu_index = INVALID_CPU_INDEX;
    };

    struct ThreadConfig
    {
        const char* name = nullptr;
        ThreadAffinity affinity{};
    };

    struct ThreadStartResult
    {
        bool started = false;
        bool affinity_applied = false;
        bool name_applied = false;
    };

    std::uint32_t hardware_thread_count() noexcept;
    std::uint32_t current_cpu_index() noexcept;

    bool set_current_thread_name(const char* name) noexcept;
    bool set_current_thread_affinity(ThreadAffinity affinity) noexcept;

    class WorkerThread final
    {
    public:
        WorkerThread() noexcept = default;
        ~WorkerThread() noexcept;

        WorkerThread(const WorkerThread&) = delete;
        WorkerThread& operator=(const WorkerThread&) = delete;

        WorkerThread(WorkerThread&& other) noexcept;
        WorkerThread& operator=(WorkerThread&& other) noexcept;

        ThreadStartResult start(const ThreadConfig& config, ThreadEntry entry, void* user) noexcept;
        void request_stop() noexcept;
        void join() noexcept;

        bool joinable() const noexcept { return m_thread.joinable(); }
        bool stop_requested() const noexcept { return m_stop_requested.load(std::memory_order_acquire); }

        ThreadAffinity affinity() const noexcept { return m_affinity; }
        bool affinity_applied() const noexcept { return m_affinity_applied; }
        bool name_applied() const noexcept { return m_name_applied; }

        const char* name() const noexcept { return m_name; }

    private:
        static void thread_main(WorkerThread* self) noexcept;

        void reset_metadata() noexcept;
        void move_from(WorkerThread& other) noexcept;

        std::thread m_thread{};
        ThreadEntry m_entry = nullptr;
        void* m_user = nullptr;

        std::atomic<bool> m_stop_requested{ false };
        ThreadAffinity m_affinity{};
        bool m_affinity_applied = false;
        bool m_name_applied = false;
        char m_name[MAX_THREAD_NAME_BYTES]{};
    };
}
