#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>

namespace acheron::core
{
    enum class LogLevel : std::uint8_t
    {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
    };

    struct LoggerConfig
    {
        // Output
        const char* file_path = nullptr;          // e.g. "logs/acheron.log" (optional)
        bool enable_console = true;
        bool enable_file = true;

        // Rotation (file output only)
        std::uint64_t max_file_bytes = 16ull * 1024ull * 1024ull; // 16 MiB
        std::uint32_t rotation_files = 3;         // keeps .1 .. .N

        // Crash-safety:
        // - When enabled, the writer thread flushes after each entry.
        // - Use flush_for_crash() to synchronously drain and flush from a crash handler.
        bool crash_safe_mode = false;

        // Queue sizing (bounded). Must be power-of-two.
        // Larger -> fewer drops under load, more memory.
        std::uint32_t queue_capacity = 8192;

        // Max message bytes per entry (excluding timestamp/level prefix).
        std::uint32_t message_bytes = 320;
    };

    // Async logging system.
    // Hot-path rule: log_*() must not perform blocking I/O and must not allocate.
    class Logger final
    {
    public:
        static void init(const LoggerConfig& config) noexcept;
        static void shutdown() noexcept;

        static void log(LogLevel level, const char* fmt, ...) noexcept;
        static void log_v(LogLevel level, const char* fmt, std::va_list args) noexcept;

        // Non-blocking request; flush happens on the writer thread.
        static void flush_async() noexcept;

        // Blocks caller until the writer thread acknowledges a flush.
        // Not intended for hot paths.
        static void flush_blocking() noexcept;

        // Synchronously drains pending entries and flushes outputs.
        // Intended for crash/fatal paths (not hot).
        static void flush_for_crash() noexcept;

        static std::uint64_t dropped_count() noexcept;

        static void trace(const char* fmt, ...) noexcept;
        static void debug(const char* fmt, ...) noexcept;
        static void info(const char* fmt, ...) noexcept;
        static void warn(const char* fmt, ...) noexcept;
        static void error(const char* fmt, ...) noexcept;
        static void fatal(const char* fmt, ...) noexcept;

        Logger() = delete;
    };

    // Minimal usage:
    //   acheron::core::LoggerConfig cfg{};
    //   cfg.file_path = "logs/acheron.log";
    //   acheron::core::Logger::init(cfg);
    //   acheron::core::Logger::info("Hello %d", 42);
    //   acheron::core::Logger::shutdown();
}

// Optional convenience macros
#define ACH_LOG_TRACE(...) ::acheron::core::Logger::trace(__VA_ARGS__)
#define ACH_LOG_DEBUG(...) ::acheron::core::Logger::debug(__VA_ARGS__)
#define ACH_LOG_INFO(...)  ::acheron::core::Logger::info(__VA_ARGS__)
#define ACH_LOG_WARN(...)  ::acheron::core::Logger::warn(__VA_ARGS__)
#define ACH_LOG_ERROR(...) ::acheron::core::Logger::error(__VA_ARGS__)
#define ACH_LOG_FATAL(...) ::acheron::core::Logger::fatal(__VA_ARGS__)
