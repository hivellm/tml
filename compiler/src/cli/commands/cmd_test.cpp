//! # Test Command Module
//!
//! This file serves as the entry point for the `tml test` command.
//! Implementation is split across the `tester/` directory for maintainability.
//!
//! ## Module Structure
//!
//! ```text
//! cmd_test.cpp             (this file - module documentation)
//! tester/
//!   ├─ tester_internal.hpp - Shared types and declarations
//!   ├─ helpers.cpp         - Utilities (ColorOutput, format_duration, etc.)
//!   ├─ discovery.cpp       - Test file discovery (*.test.tml)
//!   ├─ execution.cpp       - Test compilation and execution
//!   ├─ suite_execution.cpp - Suite-based parallel compilation
//!   ├─ output.cpp          - Result formatting (vitest-style)
//!   ├─ benchmark.cpp       - Benchmark functionality (@bench)
//!   ├─ fuzzer.cpp          - Fuzz testing (@fuzz)
//!   └─ run.cpp             - Main run_test() and argument parsing
//! ```
//!
//! ## Test Discovery
//!
//! Tests are discovered by scanning for:
//! - `*.test.tml` files anywhere in the project
//! - `*.tml` files in `tests/` directories
//! - Excludes `errors/` and `pending/` directories
//!
//! ## Execution Modes
//!
//! | Mode           | Flag              | Description                    |
//! |----------------|-------------------|--------------------------------|
//! | Standard       | (default)         | Run all tests in parallel      |
//! | Suite          | `--suite`         | Compile suites into single DLLs|
//! | Verbose        | `-v, --verbose`   | Show detailed output           |
//! | Benchmark      | `--bench`         | Run @bench functions           |
//! | Fuzzing        | `--fuzz`          | Run @fuzz functions            |
//! | Profile        | `--profile`       | Show phase timing stats        |
//! | Coverage       | `--coverage`      | Generate coverage report       |

// This file is intentionally minimal - all implementation is in tester/
