#include "thread.h"

#include <chrono>
#include <exception>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#if defined(__linux__)
#include <sched.h>
#include <unistd.h>
#endif
#endif

namespace acheron::threading
{
    namespace
    {
        static void copy_name(char (&dst)[MAX_THREAD_NAME_BYTES], const char* src) noexcept
        {
            if (!src)
            {
                dst[0] = '\0';
                return;
            }

            std::uint32_t i = 0;
            for (; i + 1u < MAX_THREAD_NAME_BYTES && src[i] != '\0'; ++i)
            {
                dst[i] = src[i];
            }
            dst[i] = '\0';
        }

        static bool has_affinity(ThreadAffinity affinity) noexcept
        {
            return affinity.cpu_index != INVALID_CPU_INDEX;
        }

#if defined(_WIN32)
        using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);

        static SetThreadDescriptionFn load_set_thread_description() noexcept
        {
            HMODULE kernel = ::GetModuleHandleW(L"Kernel32.dll");
            if (!kernel)
            {
                return nullptr;
            }

            const FARPROC proc = ::GetProcAddress(kernel, "SetThreadDescription");
            SetThreadDescriptionFn fn = nullptr;
            static_assert(sizeof(fn) == sizeof(proc));
            std::memcpy(&fn, &proc, sizeof(fn));
            return fn;
        }

        static bool set_thread_name_win32(HANDLE handle, const char* name) noexcept
        {
            if (!handle || !name || name[0] == '\0')
            {
                return false;
            }

            const SetThreadDescriptionFn fn = load_set_thread_description();
            if (!fn)
            {
                return false;
            }

            wchar_t wide[MAX_THREAD_NAME_BYTES]{};
            std::uint32_t i = 0;
            for (; i + 1u < MAX_THREAD_NAME_BYTES && name[i] != '\0'; ++i)
            {
                const unsigned char c = static_cast<unsigned char>(name[i]);
                wide[i] = (c < 128u) ? static_cast<wchar_t>(c) : L'?';
            }
            wide[i] = L'\0';

            const HRESULT hr = fn(handle, wide);
            return SUCCEEDED(hr);
        }

        static bool set_thread_affinity_win32(HANDLE handle, ThreadAffinity affinity) noexcept
        {
            if (!handle || !has_affinity(affinity))
            {
                return false;
            }

            const DWORD_PTR bits = static_cast<DWORD_PTR>(sizeof(DWORD_PTR) * 8u);
            if (affinity.cpu_index >= bits)
            {
                return false;
            }

            const DWORD_PTR mask = (static_cast<DWORD_PTR>(1u) << affinity.cpu_index);
            return ::SetThreadAffinityMask(handle, mask) != 0;
        }
#endif

#if defined(__linux__)
        static bool set_thread_affinity_pthread(pthread_t thread, ThreadAffinity affinity) noexcept
        {
            if (!has_affinity(affinity))
            {
                return false;
            }

            cpu_set_t set;
            CPU_ZERO(&set);
            CPU_SET(static_cast<int>(affinity.cpu_index), &set);
            return pthread_setaffinity_np(thread, sizeof(cpu_set_t), &set) == 0;
        }
#endif

#if defined(__linux__) || defined(__APPLE__)
        static bool set_thread_name_pthread(pthread_t thread, const char* name) noexcept
        {
            if (!name || name[0] == '\0')
            {
                return false;
            }

            char short_name[16]{};
            std::uint32_t i = 0;
            for (; i + 1u < sizeof(short_name) && name[i] != '\0'; ++i)
            {
                short_name[i] = name[i];
            }
            short_name[i] = '\0';

#if defined(__APPLE__)
            (void)thread;
            return pthread_setname_np(short_name) == 0;
#else
            return pthread_setname_np(thread, short_name) == 0;
#endif
        }
#endif
    }

    std::uint32_t hardware_thread_count() noexcept
    {
        const unsigned int count = std::thread::hardware_concurrency();
        return (count == 0u) ? 1u : static_cast<std::uint32_t>(count);
    }

    std::uint32_t current_cpu_index() noexcept
    {
#if defined(_WIN32)
        return static_cast<std::uint32_t>(::GetCurrentProcessorNumber());
#elif defined(__linux__)
        const int cpu = sched_getcpu();
        return (cpu >= 0) ? static_cast<std::uint32_t>(cpu) : INVALID_CPU_INDEX;
#else
        return INVALID_CPU_INDEX;
#endif
    }

    bool set_current_thread_name(const char* name) noexcept
    {
#if defined(_WIN32)
        return set_thread_name_win32(::GetCurrentThread(), name);
#elif defined(__linux__) || defined(__APPLE__)
        return set_thread_name_pthread(pthread_self(), name);
#else
        (void)name;
        return false;
#endif
    }

    bool set_current_thread_affinity(ThreadAffinity affinity) noexcept
    {
#if defined(_WIN32)
        return set_thread_affinity_win32(::GetCurrentThread(), affinity);
#elif defined(__linux__)
        return set_thread_affinity_pthread(pthread_self(), affinity);
#else
        (void)affinity;
        return false;
#endif
    }

    WorkerThread::~WorkerThread() noexcept
    {
        request_stop();
        join();
    }

    WorkerThread::WorkerThread(WorkerThread&& other) noexcept
    {
        move_from(other);
    }

    WorkerThread& WorkerThread::operator=(WorkerThread&& other) noexcept
    {
        if (this != &other)
        {
            request_stop();
            join();
            reset_metadata();
            move_from(other);
        }
        return *this;
    }

    ThreadStartResult WorkerThread::start(const ThreadConfig& config, ThreadEntry entry, void* user) noexcept
    {
        ThreadStartResult result{};
        if (joinable() || !entry)
        {
            return result;
        }

        m_entry = entry;
        m_user = user;
        m_affinity = config.affinity;
        m_affinity_applied = false;
        m_name_applied = false;
        m_stop_requested.store(false, std::memory_order_release);
        copy_name(m_name, config.name);

#if defined(__cpp_exceptions)
        try
        {
            m_thread = std::thread(&WorkerThread::thread_main, this);
        }
        catch (...)
        {
            reset_metadata();
            return result;
        }
#else
        m_thread = std::thread(&WorkerThread::thread_main, this);
#endif

        result.started = true;

        result.affinity_applied = m_affinity_applied;
        result.name_applied = m_name_applied;
        return result;
    }

    void WorkerThread::request_stop() noexcept
    {
        m_stop_requested.store(true, std::memory_order_release);
    }

    void WorkerThread::join() noexcept
    {
        if (m_thread.joinable())
        {
#if defined(__cpp_exceptions)
            try
            {
                m_thread.join();
            }
            catch (...)
            {
                std::terminate();
            }
#else
            m_thread.join();
#endif
        }

        m_entry = nullptr;
        m_user = nullptr;
    }

    void WorkerThread::thread_main(WorkerThread* self) noexcept
    {
        if (!self)
        {
            return;
        }

        if (self->m_name[0] != '\0')
        {
            self->m_name_applied = set_current_thread_name(self->m_name);
        }
        if (has_affinity(self->m_affinity))
        {
            self->m_affinity_applied = set_current_thread_affinity(self->m_affinity);
        }

        ThreadEntry entry = self->m_entry;
        if (entry)
        {
            entry(self->m_user);
        }
    }

    void WorkerThread::reset_metadata() noexcept
    {
        m_entry = nullptr;
        m_user = nullptr;
        m_stop_requested.store(false, std::memory_order_release);
        m_affinity = {};
        m_affinity_applied = false;
        m_name_applied = false;
        m_name[0] = '\0';
    }

    void WorkerThread::move_from(WorkerThread& other) noexcept
    {
        m_thread = std::move(other.m_thread);
        m_entry = other.m_entry;
        m_user = other.m_user;
        m_stop_requested.store(other.m_stop_requested.load(std::memory_order_acquire), std::memory_order_release);
        m_affinity = other.m_affinity;
        m_affinity_applied = other.m_affinity_applied;
        m_name_applied = other.m_name_applied;
        copy_name(m_name, other.m_name);

        other.reset_metadata();
    }
}

#if defined(ACHERON_THREAD_UNIT_TEST)

#include <atomic>

namespace
{
    struct ThreadProbe
    {
        std::atomic<bool> entered{ false };
        std::atomic<std::uint32_t> cpu{ acheron::threading::INVALID_CPU_INDEX };
    };

    static void probe_entry(void* user) noexcept
    {
        auto* probe = static_cast<ThreadProbe*>(user);
        probe->cpu.store(acheron::threading::current_cpu_index(), std::memory_order_release);
        probe->entered.store(true, std::memory_order_release);
    }

    static bool thread_creation_benchmark() noexcept
    {
        constexpr std::uint32_t iterations = 128;
        ThreadProbe probes[iterations];

        const auto begin = std::chrono::steady_clock::now();
        for (std::uint32_t i = 0; i < iterations; ++i)
        {
            char name[32]{};
            std::snprintf(name, sizeof(name), "ach_bench_%u", i);

            acheron::threading::WorkerThread thread;
            acheron::threading::ThreadConfig cfg{};
            cfg.name = name;
            const acheron::threading::ThreadStartResult result = thread.start(cfg, &probe_entry, &probes[i]);
            if (!result.started)
            {
                return false;
            }
            thread.join();
            if (!probes[i].entered.load(std::memory_order_acquire))
            {
                return false;
            }
        }
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        std::printf("[thread] create/join %u workers: %lld us\n", iterations, static_cast<long long>(elapsed_us));
        return true;
    }

    static bool affinity_verification() noexcept
    {
        const std::uint32_t hw = acheron::threading::hardware_thread_count();
        if (hw == 0u)
        {
            return false;
        }

        ThreadProbe probe{};
        acheron::threading::WorkerThread thread;
        acheron::threading::ThreadConfig cfg{};
        cfg.name = "ach_affinity";
        cfg.affinity.cpu_index = 0;

        const acheron::threading::ThreadStartResult result = thread.start(cfg, &probe_entry, &probe);
        if (!result.started)
        {
            return false;
        }
        thread.join();

        const bool applied = thread.affinity_applied();
        if (!applied)
        {
            std::printf("[thread] affinity verification: skipped (platform did not apply affinity)\n");
            return true;
        }

        const std::uint32_t observed = probe.cpu.load(std::memory_order_acquire);
        return observed == 0u;
    }
}

int main()
{
    if (!thread_creation_benchmark())
    {
        std::printf("[thread] creation benchmark: FAIL\n");
        return 1;
    }
    std::printf("[thread] creation benchmark: OK\n");

    if (!affinity_verification())
    {
        std::printf("[thread] affinity verification: FAIL\n");
        return 1;
    }
    std::printf("[thread] affinity verification: OK\n");
    return 0;
}

#endif
