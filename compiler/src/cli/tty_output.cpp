TML_MODULE("compiler")

//! # TTY-aware output implementation
//!
//! Implements the API declared in `tty_output.hpp`.

#include "tty_output.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#define TML_ISATTY(fd) _isatty(fd)
#define TML_FILENO(f) _fileno(f)
#else
#include <unistd.h>
#define TML_ISATTY(fd) isatty(fd)
#define TML_FILENO(f) fileno(f)
#endif

#include "log/log.hpp"

namespace tml::cli::tty {

namespace {

Mode g_mode;
std::once_flag g_init_once;
bool g_line_buffering_installed = false;
bool g_exit_flush_installed = false;

bool env_true(const char* name) {
    const char* v = std::getenv(name);
    if (!v) return false;
    if (v[0] == '\0') return false;
    if (std::strcmp(v, "0") == 0) return false;
    if (std::strcmp(v, "false") == 0) return false;
    if (std::strcmp(v, "FALSE") == 0) return false;
    return true;
}

#ifdef _WIN32
/// Enables ANSI virtual-terminal processing on Windows 10+. Harmless to call
/// repeatedly; returns false when stream is not a real console.
bool enable_windows_vt(DWORD std_handle_kind) {
    HANDLE h = GetStdHandle(std_handle_kind);
    if (h == INVALID_HANDLE_VALUE || h == nullptr) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return false;  // not a console (pipe/file)
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(h, mode) != 0;
}
#endif

void detect_mode(bool force_no_color, bool force_non_interactive) {
    g_mode.stdout_is_tty = TML_ISATTY(TML_FILENO(stdout)) != 0;
    g_mode.stderr_is_tty = TML_ISATTY(TML_FILENO(stderr)) != 0;

    bool any_tty = g_mode.stdout_is_tty || g_mode.stderr_is_tty;
    g_mode.non_interactive = force_non_interactive || !any_tty;

#ifdef _WIN32
    // Best-effort VT enablement; if it fails we just emit colors as plain text
    // and let strip_ansi() remove them in non-TTY paths.
    if (g_mode.stdout_is_tty) enable_windows_vt(STD_OUTPUT_HANDLE);
    if (g_mode.stderr_is_tty) enable_windows_vt(STD_ERROR_HANDLE);
#endif

    bool env_no_color = env_true("TML_NO_COLOR") || env_true("NO_COLOR");
    g_mode.colors_enabled = any_tty && !force_no_color && !env_no_color;
}

}  // namespace

const Mode& mode() {
    std::call_once(g_init_once, []() { detect_mode(false, false); });
    return g_mode;
}

void init_runtime_mode(bool force_no_color, bool force_non_interactive) {
    detect_mode(force_no_color, force_non_interactive);
    std::call_once(g_init_once, []() {});  // mark initialised
    install_line_buffering();
    install_exit_flush();
}

void install_line_buffering() {
    if (g_line_buffering_installed) return;
    g_line_buffering_installed = true;

    // Intentionally empty on this build: `setvbuf` must be called before any
    // I/O on the stream (C17 §7.21.5.6), which is true only in `main()`. The
    // real work happens in `compiler/src/main.cpp`, where stdout is switched
    // to line-buffered mode before `tml_main()` is invoked. This function is
    // kept as a no-op so callers can still request an explicit install point
    // without racing against previous output.
}

void install_exit_flush() {
    if (g_exit_flush_installed) return;
    g_exit_flush_installed = true;
    std::atexit([]() {
        std::fflush(stdout);
        std::fflush(stderr);
    });
}

std::string strip_ansi(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
            // Skip until final byte in range 0x40..0x7E (CSI terminator).
            i += 2;
            while (i < s.size() && (s[i] < 0x40 || s[i] > 0x7E)) ++i;
            // i now points at terminator; outer loop will ++i past it.
            continue;
        }
        out.push_back(s[i]);
    }
    return out;
}

void out(std::string_view s) {
    const auto& m = mode();
    if (m.colors_enabled) {
        std::fwrite(s.data(), 1, s.size(), stdout);
    } else {
        auto stripped = strip_ansi(s);
        std::fwrite(stripped.data(), 1, stripped.size(), stdout);
    }
}

void err(std::string_view s) {
    const auto& m = mode();
    if (m.colors_enabled) {
        std::fwrite(s.data(), 1, s.size(), stderr);
    } else {
        auto stripped = strip_ansi(s);
        std::fwrite(stripped.data(), 1, stripped.size(), stderr);
    }
}

void outf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (static_cast<size_t>(n) < sizeof(buf)) {
        out(std::string_view(buf, static_cast<size_t>(n)));
        return;
    }
    std::string heap(static_cast<size_t>(n), '\0');
    va_start(ap, fmt);
    std::vsnprintf(heap.data(), heap.size() + 1, fmt, ap);
    va_end(ap);
    out(heap);
}

void errf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (static_cast<size_t>(n) < sizeof(buf)) {
        err(std::string_view(buf, static_cast<size_t>(n)));
        return;
    }
    std::string heap(static_cast<size_t>(n), '\0');
    va_start(ap, fmt);
    std::vsnprintf(heap.data(), heap.size() + 1, fmt, ap);
    va_end(ap);
    err(heap);
}

bool stdout_supports_color() { return mode().stdout_is_tty && mode().colors_enabled; }
bool stderr_supports_color() { return mode().stderr_is_tty && mode().colors_enabled; }

}  // namespace tml::cli::tty
