//! # TML Compiler Entry Point
//!
//! This file is the main entry point for the TML (To Machine Language) compiler.
//! It simply delegates to the CLI driver which handles all command parsing and
//! execution.
//!
//! ## Binary Name
//!
//! The compiled binary is named `tml` and provides a unified CLI for all
//! compiler operations.
//!
//! ## Usage
//!
//! ```bash
//! tml build file.tml          # Compile a TML source file
//! tml run file.tml            # Compile and run immediately
//! tml test                    # Run tests in the current project
//! tml fmt file.tml            # Format source code
//! tml check file.tml          # Type check without codegen
//! ```
//!
//! ## Architecture
//!
//! The `main()` function is intentionally minimal. All functionality is
//! implemented in the CLI driver (`cli/driver.hpp`) which:
//! - Parses command-line arguments
//! - Dispatches to appropriate subcommands
//! - Manages compilation pipelines
//! - Handles error reporting
//!
//! ## See Also
//!
//! - `cli/driver.hpp` - CLI driver implementation
//! - `cli/dispatcher.cpp` - Command dispatching logic
//! - `docs/specs/09-CLI.md` - CLI specification

#include "cli/driver.hpp"

#include <cstdio>

/// Main entry point for the TML compiler.
///
/// Delegates all work to `tml_main()` which handles argument parsing,
/// command dispatch, and error handling.
///
/// @param argc Argument count from the operating system
/// @param argv Argument vector (null-terminated strings)
/// @return Exit code: 0 for success, non-zero for errors
int main(int argc, char* argv[]) {
    // Make stderr unbuffered so diagnostics ("[dead-func-elim] ...",
    // "[codegen-timing] ...", panic messages) are visible even when the
    // compiler crashes or is killed before graceful shutdown. Without
    // this, stderr is fully buffered when redirected to a file and the
    // buffer is lost on abnormal termination (e.g., stack overflow).
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    return tml_main(argc, argv);
}
