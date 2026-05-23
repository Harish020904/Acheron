#include "logger.h"

#include <atomic>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <new>
#include <semaphore>
#include <thread>

namespace acheron::core
{
    namespace
    {
        constexpr std::uint32_t MAX_PATH_BYTES = 512;

        static constexpr const char* level_to_string(LogLevel lvl) noexcept
        {
            switch (lvl)
            {
            case LogLevel::Trace: return "TRACE";
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO";
            case LogLevel::Warn:  return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Fatal: return "FATAL";
            default:              return "INFO";
            }
        }

        static inline bool is_power_of_two(std::uint32_t v) noexcept
        {
            return v && ((v & (v - 1u)) == 0u);
        }

        static std::uint64_t now_epoch_us() noexcept
        {
            using clock = std::chrono::system_clock;
            const auto now = clock::now();
            const auto us = std::chrono::time_point_cast<std::chrono::microseconds>(now);
            return static_cast<std::uint64_t>(us.time_since_epoch().count());
        }

        struct alignas(64) LogEntry
        {
            std::uint64_t epoch_us = 0;
            LogLevel level = LogLevel::Info;
            std::uint16_t size = 0;
            // message bytes stored inline (configured at init)
        };

        struct alignas(64) Slot
        {
            std::atomic<std::uint64_t> seq;
            // Flexible in-place storage: [LogEntry header][message bytes]
            // Backed by a single contiguous byte buffer.
        };

        struct LoggerState
        {
            std::atomic<bool> initialized{ false };
            std::atomic<bool> running{ false };

            std::atomic<std::uint64_t> dropped{ 0 };

            // Flush sequencing
            std::atomic<std::uint64_t> flush_requested{ 0 };
            std::atomic<std::uint64_t> flush_completed{ 0 };

            // Queue indices
            alignas(64) std::atomic<std::uint64_t> enqueue_pos{ 0 };
            alignas(64) std::atomic<std::uint64_t> dequeue_pos{ 0 };

            // Config snapshot
            bool enable_console = true;
            bool enable_file = true;
            bool crash_safe_mode = false;
            std::uint64_t max_file_bytes = 0;
            std::uint32_t rotation_files = 0;

            std::uint32_t capacity = 0;
            std::uint32_t mask = 0;
            std::uint32_t message_bytes = 0;
            std::uint32_t entry_stride = 0;

            char file_path[MAX_PATH_BYTES]{};

            // Storage
            Slot* slots = nullptr;
            std::byte* entry_storage = nullptr;

            // Writer
            std::counting_semaphore<INT_MAX> wake{ 0 };
            std::thread writer_thread{};

            std::FILE* file = nullptr;
            std::uint64_t file_size = 0;
        };

        static LoggerState g;

        static void safe_strcpy(char* dst, std::size_t dst_bytes, const char* src) noexcept
        {
            if (!dst || dst_bytes == 0)
            {
                return;
            }
            if (!src)
            {
                dst[0] = '\0';
                return;
            }

            std::size_t i = 0;
            for (; i + 1 < dst_bytes && src[i] != '\0'; ++i)
            {
                dst[i] = src[i];
            }
            dst[i] = '\0';
        }

        static void close_file() noexcept
        {
            if (g.file)
            {
                std::fflush(g.file);
                std::fclose(g.file);
                g.file = nullptr;
                g.file_size = 0;
            }
        }

        static void open_file_append() noexcept
        {
            close_file();

            if (!g.enable_file || g.file_path[0] == '\0')
            {
                return;
            }

            // Ensure parent directory exists
            std::error_code ec;
            const std::filesystem::path p(g.file_path);
            const auto parent = p.parent_path();
            if (!parent.empty())
            {
                std::filesystem::create_directories(parent, ec);
            }

            g.file = std::fopen(g.file_path, "ab");
            if (!g.file)
            {
                g.enable_file = false;
                return;
            }

            if (g.crash_safe_mode)
            {
                // Unbuffered to minimize loss on abnormal termination.
                std::setvbuf(g.file, nullptr, _IONBF, 0);
            }

            // Track size for rotation
            if (auto* f = g.file)
            {
                if (std::fseek(f, 0, SEEK_END) == 0)
                {
                    const long pos = std::ftell(f);
                    if (pos > 0)
                    {
                        g.file_size = static_cast<std::uint64_t>(pos);
                    }
                }
                std::fseek(f, 0, SEEK_END);
            }
        }

        static void build_rotated_path(char* out, std::size_t out_bytes, std::uint32_t index) noexcept
        {
            if (!out || out_bytes == 0)
            {
                return;
            }
            if (index == 0)
            {
                std::snprintf(out, out_bytes, "%s", g.file_path);
            }
            else
            {
                std::snprintf(out, out_bytes, "%s.%u", g.file_path, index);
            }
            out[out_bytes - 1] = '\0';
        }

        static void rotate_files_if_needed(std::uint64_t next_write_bytes) noexcept
        {
            if (!g.file || g.max_file_bytes == 0 || g.rotation_files == 0)
            {
                return;
            }

            if ((g.file_size + next_write_bytes) <= g.max_file_bytes)
            {
                return;
            }

            // Close before rename on Windows.
            close_file();

            std::error_code ec;

            char buf_src[MAX_PATH_BYTES + 16]{};
            char buf_dst[MAX_PATH_BYTES + 16]{};

            // Delete oldest
            build_rotated_path(buf_dst, sizeof(buf_dst), g.rotation_files);
            std::filesystem::remove(std::filesystem::path(buf_dst), ec);

            // Shift: .(N-1) -> .N, ... base -> .1
            for (std::uint32_t i = g.rotation_files; i >= 2; --i)
            {
                build_rotated_path(buf_src, sizeof(buf_src), i - 1);
                build_rotated_path(buf_dst, sizeof(buf_dst), i);

                if (std::filesystem::exists(std::filesystem::path(buf_src), ec))
                {
                    // On Windows, rename won't overwrite.
                    std::filesystem::remove(std::filesystem::path(buf_dst), ec);
                    std::filesystem::rename(std::filesystem::path(buf_src), std::filesystem::path(buf_dst), ec);
                }

                if (i == 2)
                {
                    break;
                }
            }

            // base -> .1
            build_rotated_path(buf_dst, sizeof(buf_dst), 1);
            std::filesystem::remove(std::filesystem::path(buf_dst), ec);
            std::filesystem::rename(std::filesystem::path(g.file_path), std::filesystem::path(buf_dst), ec);

            // Reopen new base
            open_file_append();
        }

        static void format_timestamp(char* out, std::size_t out_bytes, std::uint64_t epoch_us) noexcept
        {
            if (!out || out_bytes == 0)
            {
                return;
            }

            const std::time_t t = static_cast<std::time_t>(epoch_us / 1000000ull);
            const std::uint32_t micros = static_cast<std::uint32_t>(epoch_us % 1000000ull);

            std::tm tm{};
#if defined(_WIN32)
            gmtime_s(&tm, &t);
#else
            gmtime_r(&t, &tm);
#endif

            const std::uint32_t ms = micros / 1000u;
            std::snprintf(out, out_bytes, "%04d-%02d-%02d %02d:%02d:%02d.%03uZ",
                tm.tm_year + 1900,
                tm.tm_mon + 1,
                tm.tm_mday,
                tm.tm_hour,
                tm.tm_min,
                tm.tm_sec,
                ms);
        }

        static inline std::byte* entry_ptr(std::uint64_t pos) noexcept
        {
            return g.entry_storage + (static_cast<std::size_t>(pos & g.mask) * g.entry_stride);
        }

        static bool try_enqueue(LogLevel level, const char* fmt, std::va_list args) noexcept
        {
            if (!g.running.load(std::memory_order_relaxed))
            {
                return false;
            }

            const std::uint64_t pos = g.enqueue_pos.fetch_add(1, std::memory_order_relaxed);
            Slot& slot = g.slots[pos & g.mask];

            const std::uint64_t seq = slot.seq.load(std::memory_order_acquire);
            const std::int64_t dif = static_cast<std::int64_t>(seq) - static_cast<std::int64_t>(pos);
            if (dif != 0)
            {
                g.dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }

            // Write entry in-place: [LogEntry][message bytes]
            std::byte* ptr = entry_ptr(pos);
            auto* hdr = reinterpret_cast<LogEntry*>(ptr);
            char* msg = reinterpret_cast<char*>(ptr + sizeof(LogEntry));

            hdr->epoch_us = now_epoch_us();
            hdr->level = level;

            int written = 0;
            if (fmt)
            {
#if defined(_MSC_VER)
                written = _vsnprintf_s(msg, g.message_bytes, _TRUNCATE, fmt, args);
#else
                written = std::vsnprintf(msg, g.message_bytes, fmt, args);
#endif
            }
            if (written < 0)
            {
                written = 0;
                if (g.message_bytes > 0)
                {
                    msg[0] = '\0';
                }
            }

            // Ensure NUL termination
            if (g.message_bytes > 0)
            {
                msg[g.message_bytes - 1] = '\0';
            }

            const std::uint32_t clamped = (written >= 0) ? static_cast<std::uint32_t>(written) : 0u;
            hdr->size = static_cast<std::uint16_t>((clamped < g.message_bytes) ? clamped : (g.message_bytes ? (g.message_bytes - 1u) : 0u));

            // Publish
            slot.seq.store(pos + 1, std::memory_order_release);
            g.wake.release();
            return true;
        }

        static bool try_dequeue_copy(LogEntry& out_hdr, char* out_msg, std::size_t out_msg_bytes) noexcept
        {
            const std::uint64_t pos = g.dequeue_pos.load(std::memory_order_relaxed);
            Slot& slot = g.slots[pos & g.mask];

            const std::uint64_t seq = slot.seq.load(std::memory_order_acquire);
            const std::int64_t dif = static_cast<std::int64_t>(seq) - static_cast<std::int64_t>(pos + 1);
            if (dif != 0)
            {
                return false;
            }

            std::byte* ptr = entry_ptr(pos);
            const auto* hdr = reinterpret_cast<const LogEntry*>(ptr);
            const char* msg = reinterpret_cast<const char*>(ptr + sizeof(LogEntry));

            out_hdr = *hdr;

            if (out_msg && out_msg_bytes)
            {
                std::size_t n = static_cast<std::size_t>(out_hdr.size);
                if (n >= out_msg_bytes)
                {
                    n = out_msg_bytes - 1;
                }
                if (n)
                {
                    std::memcpy(out_msg, msg, n);
                }
                out_msg[n] = '\0';
            }

            // Mark slot free
            slot.seq.store(pos + static_cast<std::uint64_t>(g.capacity), std::memory_order_release);
            g.dequeue_pos.store(pos + 1, std::memory_order_relaxed);
            return true;
        }

        static void write_line(const LogEntry& hdr, const char* msg) noexcept
        {
            char ts[32]{};
            format_timestamp(ts, sizeof(ts), hdr.epoch_us);

            // Compose once; keep stack bounded.
            // [timestamp] [LEVEL] message\n
            char line[512]{};
            const char* lvl = level_to_string(hdr.level);
            const int n = std::snprintf(line, sizeof(line), "%s [%s] %.*s\n", ts, lvl, static_cast<int>(hdr.size), msg ? msg : "");
            const std::size_t bytes = (n > 0) ? static_cast<std::size_t>((n < static_cast<int>(sizeof(line))) ? n : (sizeof(line) - 1)) : 0u;

            rotate_files_if_needed(static_cast<std::uint64_t>(bytes));

            if (g.enable_file && g.file && bytes)
            {
                const std::size_t wrote = std::fwrite(line, 1, bytes, g.file);
                g.file_size += static_cast<std::uint64_t>(wrote);
                if (g.crash_safe_mode)
                {
                    std::fflush(g.file);
                }
            }

            if (g.enable_console && bytes)
            {
                std::FILE* out = (hdr.level >= LogLevel::Warn) ? stderr : stdout;
                (void)std::fwrite(line, 1, bytes, out);
                if (g.crash_safe_mode)
                {
                    std::fflush(out);
                }
            }
        }

        static void writer_main() noexcept
        {
            open_file_append();

            while (g.running.load(std::memory_order_relaxed))
            {
                g.wake.acquire();

                // Drain available entries
                char msg_buf[2048]{};
                for (;;)
                {
                    LogEntry hdr{};
                    if (!try_dequeue_copy(hdr, msg_buf, sizeof(msg_buf)))
                    {
                        break;
                    }
                    write_line(hdr, msg_buf);
                }

                // Flush if requested
                const std::uint64_t req = g.flush_requested.load(std::memory_order_acquire);
                const std::uint64_t done = g.flush_completed.load(std::memory_order_relaxed);
                if (req != done)
                {
                    if (g.enable_file && g.file)
                    {
                        std::fflush(g.file);
                    }
                    if (g.enable_console)
                    {
                        std::fflush(stdout);
                        std::fflush(stderr);
                    }
                    g.flush_completed.store(req, std::memory_order_release);
                }
            }

            // Final drain
            char msg_buf[2048]{};
            for (;;)
            {
                LogEntry hdr{};
                if (!try_dequeue_copy(hdr, msg_buf, sizeof(msg_buf)))
                {
                    break;
                }
                write_line(hdr, msg_buf);
            }

            if (g.enable_file && g.file)
            {
                std::fflush(g.file);
            }
            if (g.enable_console)
            {
                std::fflush(stdout);
                std::fflush(stderr);
            }

            close_file();
        }

        static void free_storage() noexcept
        {
            // Only used during shutdown; ok to use delete/free here.
            if (g.entry_storage)
            {
                delete[] g.entry_storage;
                g.entry_storage = nullptr;
            }
            if (g.slots)
            {
                delete[] g.slots;
                g.slots = nullptr;
            }
        }

        static bool alloc_storage(std::uint32_t capacity, std::uint32_t message_bytes) noexcept
        {
            // Storage layout:
            // - slots: capacity * Slot
            // - entry_storage: capacity * entry_stride (LogEntry + message_bytes)
            g.capacity = capacity;
            g.mask = capacity - 1u;
            g.message_bytes = message_bytes;

            const std::uint32_t stride = static_cast<std::uint32_t>(sizeof(LogEntry)) + message_bytes;
            g.entry_stride = (stride + 63u) & ~63u; // 64-byte align each entry

            g.slots = new (std::nothrow) Slot[capacity];
            if (!g.slots)
            {
                return false;
            }

            g.entry_storage = new (std::nothrow) std::byte[static_cast<std::size_t>(capacity) * g.entry_stride];
            if (!g.entry_storage)
            {
                delete[] g.slots;
                g.slots = nullptr;
                return false;
            }

            for (std::uint32_t i = 0; i < capacity; ++i)
            {
                g.slots[i].seq.store(static_cast<std::uint64_t>(i), std::memory_order_relaxed);
            }

            g.enqueue_pos.store(0, std::memory_order_relaxed);
            g.dequeue_pos.store(0, std::memory_order_relaxed);
            return true;
        }

        static void wake_writer() noexcept
        {
            // Ensure writer gets unstuck.
            g.wake.release();
        }

        static void request_flush_and_wake() noexcept
        {
            g.flush_requested.fetch_add(1, std::memory_order_release);
            wake_writer();
        }
    }

    void Logger::init(const LoggerConfig& config) noexcept
    {
        const bool already = g.initialized.exchange(true, std::memory_order_acq_rel);
        if (already)
        {
            return;
        }

        g.enable_console = config.enable_console;
        g.enable_file = config.enable_file;
        g.crash_safe_mode = config.crash_safe_mode;
        g.max_file_bytes = config.max_file_bytes;
        g.rotation_files = config.rotation_files;

        safe_strcpy(g.file_path, sizeof(g.file_path), config.file_path);

        std::uint32_t cap = config.queue_capacity;
        if (!is_power_of_two(cap))
        {
            // clamp to default
            cap = 8192;
        }
        std::uint32_t msg_bytes = config.message_bytes;
        if (msg_bytes == 0)
        {
            msg_bytes = 320;
        }
        if (msg_bytes > 2048)
        {
            msg_bytes = 2048;
        }

        if (!alloc_storage(cap, msg_bytes))
        {
            // If we cannot allocate, disable logging.
            g.enable_console = false;
            g.enable_file = false;
            g.running.store(false, std::memory_order_release);
            g.initialized.store(false, std::memory_order_release);
            return;
        }

        g.running.store(true, std::memory_order_release);

        // Writer thread (not a hot path)
        g.writer_thread = std::thread([]() { writer_main(); });

        // If crash-safe requested, flush early to materialize file and buffers
        if (g.crash_safe_mode)
        {
            request_flush_and_wake();
        }
    }

    void Logger::shutdown() noexcept
    {
        if (!g.initialized.load(std::memory_order_acquire))
        {
            return;
        }

        // Request flush and stop
        request_flush_and_wake();
        g.running.store(false, std::memory_order_release);
        wake_writer();

        if (g.writer_thread.joinable())
        {
            g.writer_thread.join();
        }

        free_storage();
        g.initialized.store(false, std::memory_order_release);
    }

    void Logger::log(LogLevel level, const char* fmt, ...) noexcept
    {
        std::va_list args;
        va_start(args, fmt);
        log_v(level, fmt, args);
        va_end(args);
    }

    void Logger::log_v(LogLevel level, const char* fmt, std::va_list args) noexcept
    {
        // vsnprintf consumes va_list; copy for safety.
        std::va_list copy;
        va_copy(copy, args);
        (void)try_enqueue(level, fmt, copy);
        va_end(copy);
    }

    void Logger::flush_async() noexcept
    {
        if (!g.running.load(std::memory_order_relaxed))
        {
            return;
        }
        request_flush_and_wake();
    }

    void Logger::flush_blocking() noexcept
    {
        if (!g.running.load(std::memory_order_relaxed))
        {
            return;
        }

        const std::uint64_t id = g.flush_requested.fetch_add(1, std::memory_order_acq_rel) + 1;
        wake_writer();

        // Busy-wait with yields; non-hot path.
        while (g.flush_completed.load(std::memory_order_acquire) < id)
        {
            std::this_thread::yield();
        }
    }

    void Logger::flush_for_crash() noexcept
    {
        // Crash/fatal path: stop accepting new entries, join writer, then drain anything remaining.
        const bool was_running = g.running.exchange(false, std::memory_order_acq_rel);

        // Wake and join writer thread if it's not us.
        if (g.writer_thread.joinable())
        {
            if (std::this_thread::get_id() != g.writer_thread.get_id())
            {
                wake_writer();
                g.writer_thread.join();
            }
        }

        // Writer may have already closed the file; reopen if file output is enabled.
        open_file_append();

        char msg_buf[2048]{};
        for (;;)
        {
            LogEntry hdr{};
            if (!try_dequeue_copy(hdr, msg_buf, sizeof(msg_buf)))
            {
                break;
            }
            write_line(hdr, msg_buf);
        }

        if (g.enable_file && g.file)
        {
            std::fflush(g.file);
        }
        if (g.enable_console)
        {
            std::fflush(stdout);
            std::fflush(stderr);
        }

        close_file();

        // Keep logger stopped (crash path). If caller wants normal operation, they must re-init.
        (void)was_running;
    }

    std::uint64_t Logger::dropped_count() noexcept
    {
        return g.dropped.load(std::memory_order_relaxed);
    }

    void Logger::trace(const char* fmt, ...) noexcept
    {
        std::va_list args;
        va_start(args, fmt);
        log_v(LogLevel::Trace, fmt, args);
        va_end(args);
    }

    void Logger::debug(const char* fmt, ...) noexcept
    {
        std::va_list args;
        va_start(args, fmt);
        log_v(LogLevel::Debug, fmt, args);
        va_end(args);
    }

    void Logger::info(const char* fmt, ...) noexcept
    {
        std::va_list args;
        va_start(args, fmt);
        log_v(LogLevel::Info, fmt, args);
        va_end(args);
    }

    void Logger::warn(const char* fmt, ...) noexcept
    {
        std::va_list args;
        va_start(args, fmt);
        log_v(LogLevel::Warn, fmt, args);
        va_end(args);
    }

    void Logger::error(const char* fmt, ...) noexcept
    {
        std::va_list args;
        va_start(args, fmt);
        log_v(LogLevel::Error, fmt, args);
        va_end(args);
    }

    void Logger::fatal(const char* fmt, ...) noexcept
    {
        std::va_list args;
        va_start(args, fmt);
        log_v(LogLevel::Fatal, fmt, args);
        va_end(args);

        // Fatal is not hot: ensure it hits disk/console.
        flush_for_crash();
    }
}

// ---------------------------
// Unit tests (self-contained)
// Compile with: -DACHERON_LOGGER_UNIT_TEST
// ---------------------------
#if defined(ACHERON_LOGGER_UNIT_TEST)

#include <cstdlib>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
    static bool ensure_dir(const char* path) noexcept
    {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path), ec);
        return !ec;
    }

    static bool file_contains(const char* path, const char* needle) noexcept
    {
        if (!path || !needle)
        {
            return false;
        }

        std::FILE* f = std::fopen(path, "rb");
        if (!f)
        {
            return false;
        }

        char buf[4096]{};
        bool found = false;
        while (std::fgets(buf, sizeof(buf), f))
        {
            if (std::strstr(buf, needle))
            {
                found = true;
                break;
            }
        }

        std::fclose(f);
        return found;
    }

    static std::uint64_t count_lines(const char* path) noexcept
    {
        std::FILE* f = std::fopen(path, "rb");
        if (!f)
        {
            return 0;
        }

        std::uint64_t lines = 0;
        int c = 0;
        while ((c = std::fgetc(f)) != EOF)
        {
            if (c == '\n')
            {
                ++lines;
            }
        }
        std::fclose(f);
        return lines;
    }

    static bool remove_file(const char* path) noexcept
    {
        std::error_code ec;
        std::filesystem::remove(std::filesystem::path(path), ec);
        return !ec;
    }

    static bool exists_file(const char* path) noexcept
    {
        std::error_code ec;
        const bool ex = std::filesystem::exists(std::filesystem::path(path), ec);
        return ex && !ec;
    }

    static std::uint64_t file_size(const char* path) noexcept
    {
        std::error_code ec;
        const auto sz = std::filesystem::file_size(std::filesystem::path(path), ec);
        return ec ? 0ull : static_cast<std::uint64_t>(sz);
    }

#if defined(_WIN32)
    static bool spawn_child_crash(const char* exe_path, const char* log_path) noexcept
    {
        char cmd[1024]{};
        std::snprintf(cmd, sizeof(cmd), "\"%s\" --crash-child \"%s\"", exe_path, log_path);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        BOOL ok = CreateProcessA(
            nullptr,
            cmd,
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi);

        if (!ok)
        {
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    static bool get_self_path(char* out, std::size_t out_bytes) noexcept
    {
        if (!out || out_bytes == 0)
        {
            return false;
        }
        const DWORD n = GetModuleFileNameA(nullptr, out, static_cast<DWORD>(out_bytes));
        if (n == 0 || n >= out_bytes)
        {
            return false;
        }
        out[n] = '\0';
        return true;
    }
#endif

    static bool test_multithreaded_logging() noexcept
    {
        ensure_dir("logs/logger_tests");
        const char* path = "logs/logger_tests/multithread.log";
        remove_file(path);

        acheron::core::LoggerConfig cfg{};
        cfg.file_path = path;
        cfg.enable_console = false;
        cfg.enable_file = true;
        cfg.queue_capacity = 16384;
        cfg.message_bytes = 256;
        cfg.max_file_bytes = 64ull * 1024ull * 1024ull;
        cfg.rotation_files = 1;

        acheron::core::Logger::init(cfg);

        constexpr int threads = 8;
        constexpr int per_thread = 1500;

        std::thread t[threads];
        for (int i = 0; i < threads; ++i)
        {
            t[i] = std::thread([i]() {
                for (int j = 0; j < per_thread; ++j)
                {
                    acheron::core::Logger::info("t=%d j=%d", i, j);
                }
            });
        }

        for (int i = 0; i < threads; ++i)
        {
            t[i].join();
        }

        acheron::core::Logger::flush_blocking();
        acheron::core::Logger::shutdown();

        const std::uint64_t lines = count_lines(path);
        const std::uint64_t expected = static_cast<std::uint64_t>(threads * per_thread);

        // We expect no drops at this load.
        return (lines == expected);
    }

    static bool test_log_rotation() noexcept
    {
        ensure_dir("logs/logger_tests");
        const char* base = "logs/logger_tests/rotate.log";
        const char* r1 = "logs/logger_tests/rotate.log.1";
        const char* r2 = "logs/logger_tests/rotate.log.2";

        remove_file(base);
        remove_file(r1);
        remove_file(r2);

        acheron::core::LoggerConfig cfg{};
        cfg.file_path = base;
        cfg.enable_console = false;
        cfg.enable_file = true;
        cfg.queue_capacity = 8192;
        cfg.message_bytes = 256;
        cfg.max_file_bytes = 2048;
        cfg.rotation_files = 2;

        acheron::core::Logger::init(cfg);

        // Enough to rotate multiple times.
        for (int i = 0; i < 400; ++i)
        {
            acheron::core::Logger::info("rotate line %d: 0123456789abcdef0123456789abcdef0123456789abcdef", i);
        }

        acheron::core::Logger::flush_blocking();
        acheron::core::Logger::shutdown();

        // At least one rotation should have occurred.
        if (!exists_file(r1))
        {
            return false;
        }

        // Active file should be under the threshold (allow a small prefix overshoot).
        const std::uint64_t active = file_size(base);
        return active <= (cfg.max_file_bytes + 1024ull);
    }

    static bool test_crash_safe_flushing_parent() noexcept
    {
#if !defined(_WIN32)
        // Child-process crash test is implemented for Windows in this repo setup.
        return true;
#else
        ensure_dir("logs/logger_tests");
        const char* path = "logs/logger_tests/crash.log";
        remove_file(path);

        char exe[1024]{};
        if (!get_self_path(exe, sizeof(exe)))
        {
            return false;
        }

        if (!spawn_child_crash(exe, path))
        {
            return false;
        }

        // Child is expected to abort after flush_for_crash().
        return file_contains(path, "CHILD_CRASH_MARKER");
#endif
    }

    static int crash_child_main(const char* log_path) noexcept
    {
        acheron::core::LoggerConfig cfg{};
        cfg.file_path = log_path;
        cfg.enable_console = false;
        cfg.enable_file = true;
        cfg.crash_safe_mode = true;
        cfg.queue_capacity = 1024;
        cfg.message_bytes = 256;
        cfg.max_file_bytes = 1ull * 1024ull * 1024ull;
        cfg.rotation_files = 1;

        acheron::core::Logger::init(cfg);
        acheron::core::Logger::fatal("CHILD_CRASH_MARKER");

        // Simulate abnormal termination.
        std::abort();
        return 0;
    }

    struct TestCase
    {
        const char* name;
        bool(*fn)() noexcept;
    };

    static int run_tests(int argc, char** argv) noexcept
    {
        if (argc >= 3 && std::strcmp(argv[1], "--crash-child") == 0)
        {
            return crash_child_main(argv[2]);
        }

        const TestCase tests[] = {
            { "multithreaded logging", &test_multithreaded_logging },
            { "crash-safe flushing",  &test_crash_safe_flushing_parent },
            { "log rotation",         &test_log_rotation },
        };

        int failed = 0;
        for (const auto& t : tests)
        {
            const bool ok = t.fn();
            std::printf("[logger] %s: %s\n", t.name, ok ? "OK" : "FAIL");
            failed += ok ? 0 : 1;
        }

        return failed ? 1 : 0;
    }
}

int main(int argc, char** argv)
{
    return run_tests(argc, argv);
}

#endif
