#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdarg>

namespace acheron::core
{
    struct StackTrace
    {
        static constexpr std::uint32_t max_frames = 64;
        void* frames[max_frames]{};
        std::uint32_t count = 0;
    };

    using AssertWriteFn = void (*)(const char* data, std::size_t size, void* user) noexcept;

    // Optional: override output sink (defaults to stderr). Intended for tests.
    void set_assert_output(AssertWriteFn fn, void* user) noexcept;

    StackTrace capture_stack_trace(std::uint32_t skip_frames = 0) noexcept;

    [[noreturn]] void assert_fail(const char* expr,
                                 const char* file,
                                 int line,
                                 const char* func,
                                 const char* msg) noexcept;

    [[noreturn]] void assert_failf(const char* expr,
                                  const char* file,
                                  int line,
                                  const char* func,
                                  const char* fmt,
                                  ...) noexcept;
}

// Fatal assertions (always enabled).
#define ACH_ASSERT(expr) \
    do \
    { \
        if (!(expr)) \
        { \
            ::acheron::core::assert_fail(#expr, __FILE__, __LINE__, __func__, nullptr); \
        } \
    } while (0)

#define ACH_ASSERT_MSG(expr, msg) \
    do \
    { \
        if (!(expr)) \
        { \
            ::acheron::core::assert_fail(#expr, __FILE__, __LINE__, __func__, (msg)); \
        } \
    } while (0)

#define ACH_ASSERTF(expr, fmt, ...) \
    do \
    { \
        if (!(expr)) \
        { \
            ::acheron::core::assert_failf(#expr, __FILE__, __LINE__, __func__, (fmt) __VA_OPT__(, ) __VA_ARGS__); \
        } \
    } while (0)

#if !defined(NDEBUG)
#define ACH_DEBUG_ASSERT(expr) ACH_ASSERT(expr)
#else
#define ACH_DEBUG_ASSERT(expr) \
    do \
    { \
        (void)sizeof(expr); \
    } while (0)
#endif
