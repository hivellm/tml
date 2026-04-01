//! # Inspect Command Interface
//!
//! This header defines the `tml inspect` command API for launching a
//! terminal-based debugger that connects to the TML inspector WebSocket server.
//!
//! ## Protocol
//!
//! The inspector uses the Chrome DevTools Protocol (CDP) over WebSocket (RFC 6455).
//! The target program is launched with `--inspect-brk` so it pauses before user
//! code and waits for the debugger to connect.
//!
//! ## REPL Commands
//!
//! | Command              | Alias | Description                             |
//! |----------------------|-------|-----------------------------------------|
//! | `break <file>:<line>`| `b`   | Set a breakpoint                        |
//! | `continue`           | `c`   | Resume execution                        |
//! | `step`               | `s`   | Step into next call                     |
//! | `next`               | `n`   | Step over to next line                  |
//! | `out`                | `o`   | Step out of current frame               |
//! | `backtrace`          | `bt`  | Show call stack                         |
//! | `print <expr>`       | `p`   | Evaluate expression                     |
//! | `locals`             | `l`   | Show local variables                    |
//! | `heap`               | -     | Show heap usage statistics              |
//! | `profile start`      | -     | Start CPU profiling                     |
//! | `profile stop`       | -     | Stop CPU profiling                      |
//! | `help`               | `h`   | Show command help                       |
//! | `quit`               | `q`   | Exit debugger                           |

#pragma once

#include <string>
#include <vector>

namespace tml::cli {

/**
 * @brief Launch a terminal debugger connected to the TML inspector.
 *
 * Builds the given .tml file, launches it with `--inspect-brk`, connects to
 * the WebSocket inspector server, and enters an interactive REPL.
 *
 * @param file_path  Path to the .tml source file to debug.
 * @param port       Inspector WebSocket port (default: 9229).
 * @param verbose    Enable verbose output.
 * @param args       Extra arguments forwarded to the target program.
 * @return 0 on success, non-zero on error.
 */
int run_inspect(const std::string& file_path, int port, bool verbose,
                const std::vector<std::string>& args);

} // namespace tml::cli
