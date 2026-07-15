//! # TTY-aware output helpers
//!
//! Provides a unified API for writing to stdout/stderr that:
//!
//! 1. Detects whether the stream is connected to a TTY (via `_isatty`/`isatty`).
//! 2. Strips ANSI color codes automatically when the stream is NOT a TTY.
//! 3. Sets line buffering on pipe/file streams so output flushes promptly and
//!    never fills a 64KB pipe buffer, which is the root cause of the hang
//!    observed by MCP/CI/AI agents.
//! 4. Honors `TML_NO_COLOR=1` env var and process-wide `--no-color` /
//!    `--non-interactive` flags.
//!
//! Call `tty::init_runtime_mode()` once from `main`/`main_frontend` before any
//! other output happens.

#pragma once

#include <cstdio>
#include <string>
#include <string_view>

namespace tml::cli::tty {

/// Process-wide output configuration.
///
/// Populated once at startup by `init_runtime_mode()`.
struct Mode {
    bool stdout_is_tty = false;
    bool stderr_is_tty = false;
    bool colors_enabled = false;   ///< Honors `TML_NO_COLOR` and `--no-color`.
    bool non_interactive = false;  ///< Honors `--non-interactive` and non-TTY output.
};

/// Returns the cached process-wide mode. Lazy-initialises on first call.
const Mode& mode();

/// Re-detects TTY status and applies line buffering. Should be called once
/// early in `main` (after `argc`/`argv` are known) so the mode reflects the
/// actual runtime environment. May be called again by tests.
///
/// - `force_no_color`: set from `--no-color` CLI flag.
/// - `force_non_interactive`: set from `--non-interactive` CLI flag.
void init_runtime_mode(bool force_no_color = false, bool force_non_interactive = false);

/// Installs `setvbuf(_IOLBF)` on stdout/stderr when writing to a pipe/file,
/// so individual writes flush on `\n` and never saturate the pipe buffer.
/// Safe to call multiple times; idempotent.
void install_line_buffering();

/// Registers an `atexit` handler that flushes stdout and stderr. Safe to call
/// multiple times; idempotent.
void install_exit_flush();

/// Writes `s` to stdout. Strips ANSI escape sequences (`\x1b[...m`) when
/// colors are disabled. Always flushes on newline via line-buffering.
void out(std::string_view s);

/// Writes `s` to stderr with the same rules as `out()`.
void err(std::string_view s);

/// Printf-style write to stdout.
void outf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

/// Printf-style write to stderr.
void errf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

/// Returns `s` unchanged if colors are enabled; otherwise returns a copy with
/// every `\x1b[...m` sequence stripped.
std::string strip_ansi(std::string_view s);

/// Returns true when stdout is a TTY AND colors are enabled.
bool stdout_supports_color();

/// Returns true when stderr is a TTY AND colors are enabled.
bool stderr_supports_color();

}  // namespace tml::cli::tty
