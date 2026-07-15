//! # TML Compiler Daemon Command
//!
//! Implements the `tml daemon` command for persistent background compilation.
//!
//! ## Architecture
//!
//! ```text
//! tml daemon start          Spawn background server process, write PID file
//! tml daemon stop           Send shutdown request via named pipe
//! tml daemon status         Check if daemon is running and responsive
//!
//! tml build ...             Detects running daemon, forwards request over pipe
//!                           Falls back to direct compilation if no daemon
//! ```
//!
//! ## IPC Protocol
//!
//! JSON-lines over a Windows named pipe (message mode):
//!
//! Client → Server:
//! ```json
//! {"id": 1, "cmd": "build", "argv": ["build", "main.tml", ...], "cwd": "/path"}
//! {"id": 2, "cmd": "shutdown"}
//! ```
//!
//! Server → Client:
//! ```json
//! {"id": 1, "exit": 0, "out": "...", "err": "..."}
//! {"id": 2, "exit": 0}
//! ```
//!
//! ## Pipe name
//!
//! `\\.\pipe\tml-daemon-<CRC32 of cwd>` — one daemon per project directory.
//!
//! ## Lifetime
//!
//! The daemon auto-shuts down after 30 minutes of inactivity.

#pragma once

#include <string>
#include <vector>

namespace tml::cli {

/// Implements `tml daemon <subcommand>`.
///
/// Subcommands: start, stop, status
///
/// Returns exit code (0 = success).
auto cmd_daemon(const std::vector<std::string>& args) -> int;

/// Hidden internal entry point — called in the background process.
/// Runs the named pipe server loop until shutdown or idle timeout.
///
/// Returns exit code (0 = normal shutdown, 1 = error).
auto cmd_daemon_server(const std::vector<std::string>& args) -> int;

/// Attempt to forward a build/run/check request to a running daemon.
///
/// Returns the daemon's exit code if the daemon handled the request,
/// or -1 if no daemon is running (caller should fall back to direct compile).
auto try_daemon_forward(int argc, char* argv[]) -> int;

} // namespace tml::cli
