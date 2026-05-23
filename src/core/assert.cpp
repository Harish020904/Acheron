#include "assert.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace acheron::core
{
    namespace
    {
        struct AssertOutput
        {
            std::atomic<AssertWriteFn> fn{ nullptr };
            std::atomic<void*> user{ nullptr };
        };

        static AssertOutput g_out;

        static void write_bytes(const char* data, std::size_t size) noexcept
        {
            if (!data || size == 0)
            {
                return;
            }

            if (auto* fn = g_out.fn.load(std::memory_order_acquire))
            {
                fn(data, size, g_out.user.load(std::memory_order_acquire));
                return;
            }

            (void)std::fwrite(data, 1, size, stderr);
        }

        static void write_cstr(const char* s) noexcept
        {
            if (!s)
            {
                return;
            }
            write_bytes(s, std::strlen(s));
        }

        static void writef(const char* fmt, ...) noexcept
        {
            char buf[2048];
            std::va_list args;
            va_start(args, fmt);
#if defined(_WIN32)
            const int n = _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
#else
            const int n = std::vsnprintf(buf, sizeof(buf), fmt, args);
#endif
            va_end(args);

            if (n <= 0)
            {
                std::size_t len = 0;
                while (len < sizeof(buf) && buf[len] != '\0')
                {
                    ++len;
                }
                if (len)
                {
                    write_bytes(buf, len);
                }
                return;
            }
            write_bytes(buf, static_cast<std::size_t>(n));
        }

        [[maybe_unused]] static void platform_debug_break() noexcept
        {
#if defined(_WIN32)
            if (::IsDebuggerPresent())
            {
                ::DebugBreak();
            }
#endif
        }

        [[noreturn]] static void platform_terminate(std::uint32_t code) noexcept
        {
#if defined(_WIN32)
            ::TerminateProcess(::GetCurrentProcess(), code);
            for (;;)
            {
            }
#else
            (void)code;
            std::abort();
#endif
        }

        static void dump_stack_trace(const StackTrace& st) noexcept
        {
            writef("StackTrace (%u frames):\n", st.count);
            for (std::uint32_t i = 0; i < st.count; ++i)
            {
                writef("  #%02u 0x%p\n", i, st.frames[i]);
            }
        }

#if defined(_WIN32)
        static StackTrace capture_stack_trace_win32(std::uint32_t skip_frames) noexcept
        {
            StackTrace st{};
            const ULONG to_skip = static_cast<ULONG>(skip_frames + 2u);
            const ULONG to_capture = static_cast<ULONG>(StackTrace::max_frames);
            const USHORT captured = ::RtlCaptureStackBackTrace(to_skip, to_capture, st.frames, nullptr);
            st.count = static_cast<std::uint32_t>(captured);
            return st;
        }
#endif
    }

    void set_assert_output(AssertWriteFn fn, void* user) noexcept
    {
        g_out.user.store(user, std::memory_order_release);
        g_out.fn.store(fn, std::memory_order_release);
    }

    StackTrace capture_stack_trace(std::uint32_t skip_frames) noexcept
    {
#if defined(_WIN32)
        return capture_stack_trace_win32(skip_frames);
#else
        (void)skip_frames;
        return {};
#endif
    }

    [[noreturn]] void assert_fail(const char* expr,
                                 const char* file,
                                 int line,
                                 const char* func,
                                 const char* msg) noexcept
    {
        write_cstr("\n=== ACHERON ASSERTION FAILED ===\n");
        writef("expr: %s\n", expr ? expr : "<null>");
        writef("file: %s\n", file ? file : "<null>");
        writef("line: %d\n", line);
        writef("func: %s\n", func ? func : "<null>");
        if (msg && msg[0])
        {
            writef("msg : %s\n", msg);
        }

        const StackTrace st = capture_stack_trace(0);
        dump_stack_trace(st);

        (void)std::fflush(stderr);

#if !defined(NDEBUG)
        platform_debug_break();
#endif
        platform_terminate(0xE0000001u);
    }

    [[noreturn]] void assert_failf(const char* expr,
                                  const char* file,
                                  int line,
                                  const char* func,
                                  const char* fmt,
                                  ...) noexcept
    {
        char msg_buf[1024];
        msg_buf[0] = '\0';

        if (fmt && fmt[0])
        {
            std::va_list args;
            va_start(args, fmt);
#if defined(_WIN32)
            (void)_vsnprintf_s(msg_buf, sizeof(msg_buf), _TRUNCATE, fmt, args);
#else
            (void)std::vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
#endif
            va_end(args);
        }

        assert_fail(expr, file, line, func, msg_buf[0] ? msg_buf : nullptr);
    }
}

// ---------------------------
// Unit tests (self-contained)
// Compile with: -DACHERON_ASSERT_UNIT_TEST
// ---------------------------
#if defined(ACHERON_ASSERT_UNIT_TEST)

#include <string_view>

#if defined(_WIN32)
namespace
{
    static bool read_all(const wchar_t* path, char* out, std::size_t cap, std::size_t* out_size) noexcept
    {
        if (!out || cap == 0)
        {
            return false;
        }
        out[0] = '\0';
        if (out_size)
        {
            *out_size = 0;
        }

        HANDLE f = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        DWORD read = 0;
        const BOOL ok = ::ReadFile(f, out, static_cast<DWORD>(cap - 1), &read, nullptr);
        ::CloseHandle(f);
        if (!ok)
        {
            return false;
        }

        out[read] = '\0';
        if (out_size)
        {
            *out_size = static_cast<std::size_t>(read);
        }
        return true;
    }

    static bool contains(std::string_view hay, std::string_view needle) noexcept
    {
        return hay.find(needle) != std::string_view::npos;
    }

    static int child_main() noexcept
    {
        // Ensure no modal crash UI.
        ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

        auto probe_c = []() noexcept { ACH_ASSERTF(false, "test-marker-%d", 123); };
        auto probe_b = [&]() noexcept { probe_c(); };
        auto probe_a = [&]() noexcept { probe_b(); };
        probe_a();
        return 0;
    }

    static bool run_assertion_failure_test() noexcept
    {
        wchar_t exe_path[MAX_PATH]{};
        if (::GetModuleFileNameW(nullptr, exe_path, MAX_PATH) == 0)
        {
            return false;
        }

        wchar_t tmp_dir[MAX_PATH]{};
        if (::GetTempPathW(MAX_PATH, tmp_dir) == 0)
        {
            return false;
        }

        wchar_t out_path[MAX_PATH]{};
        (void)swprintf_s(out_path, L"%sacheron_assert_test_output.txt", tmp_dir);

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE out = ::CreateFileW(out_path, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (out == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        wchar_t cmd[2 * MAX_PATH]{};
        (void)swprintf_s(cmd, L"\"%s\" --child --out \"%s\"", exe_path, out_path);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = out;
        si.hStdError = out;

        PROCESS_INFORMATION pi{};
        const BOOL ok = ::CreateProcessW(nullptr, cmd, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
        ::CloseHandle(out);

        if (!ok)
        {
            return false;
        }

        (void)::WaitForSingleObject(pi.hProcess, 10'000);

        DWORD exit_code = 0;
        (void)::GetExitCodeProcess(pi.hProcess, &exit_code);

        ::CloseHandle(pi.hThread);
        ::CloseHandle(pi.hProcess);

        char buf[32 * 1024]{};
        std::size_t sz = 0;
        if (!read_all(out_path, buf, sizeof(buf), &sz))
        {
            return false;
        }

        const std::string_view out_view(buf, sz);
        if (exit_code == STILL_ACTIVE)
        {
            return false;
        }
        if (exit_code == 0)
        {
            return false;
        }
        if (!contains(out_view, "ACHERON ASSERTION FAILED"))
        {
            return false;
        }
        if (!contains(out_view, "StackTrace"))
        {
            return false;
        }
        if (!contains(out_view, "#00"))
        {
            return false;
        }
        if (!contains(out_view, "test-marker-123"))
        {
            return false;
        }
        return true;
    }

    static bool run_stack_capture_test() noexcept
    {
        const acheron::core::StackTrace st = acheron::core::capture_stack_trace(0);
        if (st.count == 0)
        {
            return false;
        }
        if (st.frames[0] == nullptr)
        {
            return false;
        }
        return true;
    }
}
#endif

int main(int argc, char** argv)
{
#if defined(_WIN32)
    if (argc >= 2 && std::strcmp(argv[1], "--child") == 0)
    {
        return child_main();
    }

    if (!run_stack_capture_test())
    {
        std::fprintf(stderr, "[assert] stack capture: FAIL\n");
        return 1;
    }
    std::fprintf(stderr, "[assert] stack capture: OK\n");

    if (!run_assertion_failure_test())
    {
        std::fprintf(stderr, "[assert] assertion failure: FAIL\n");
        return 1;
    }
    std::fprintf(stderr, "[assert] assertion failure: OK\n");

    return 0;
#else
    (void)argc;
    (void)argv;
    std::fprintf(stderr, "[assert] tests skipped (non-Windows)\n");
    return 0;
#endif
}

#endif
