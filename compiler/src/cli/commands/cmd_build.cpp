TML_MODULE("compiler")

//! # Build Command Module
//!
//! This file serves as the entry point for all build-related commands.
//! Implementation is split across the `builder/` directory for maintainability.
//!
//! ## Module Structure
//!
//! ```text
//! cmd_build.cpp          (this file - module documentation)
//! builder/
//!   ├─ helpers.cpp       - Shared utilities (hashing, diagnostics, etc.)
//!   ├─ build.cpp         - run_build() and run_build_ex()
//!   ├─ run.cpp           - run_run() and run_run_quiet()
//!   └─ run_profiled.cpp  - run_run_profiled() with phase timing
//! ```
//!
//! ## Build Pipeline
//!
//! ```text
//! Source → Lex → Parse → TypeCheck → BorrowCheck → MIR → LLVM IR → Object → Link
//!   │                                                        │         │
//!   └── run_build() ─────────────────────────────────────────┴─────────┴──→ .exe/.dll/.a
//! ```
//!
//! ## Commands
//!
//! | Function              | Description                              | Command         |
//! |-----------------------|------------------------------------------|-----------------|
//! | `run_build()`         | Full compilation to executable/library   | `tml build`     |
//! | `run_build_ex()`      | Extended build with timing options       | `tml build -t`  |
//! | `run_run()`           | Compile and execute immediately          | `tml run`       |
//! | `run_run_quiet()`     | Build and run with output capture        | (internal)      |
//! | `run_run_profiled()`  | Build and run with phase timing          | `tml run -t`    |
//!
//! ## Output Types
//!
//! | Type          | Extension (Win/Unix)  | Flag              |
//! |---------------|-----------------------|-------------------|
//! | Executable    | `.exe` / (none)       | (default)         |
//! | Static Lib    | `.lib` / `.a`         | `--crate-type=lib`|
//! | Dynamic Lib   | `.dll` / `.so`        | `--crate-type=dylib`|
//! | TML Library   | `.rlib`               | `--crate-type=rlib`|

#include "cmd_build.hpp"

// All implementations are in builder/*.cpp files.
// This file exists to document the module structure and maintain
// backwards compatibility with the include path.

namespace tml::cli {

// Function implementations are in:
// - run_build()        -> builder/build.cpp
// - run_build_ex()     -> builder/build.cpp
// - run_run()          -> builder/run.cpp
// - run_run_quiet()    -> builder/run.cpp
// - run_run_profiled() -> builder/run_profiled.cpp

} // namespace tml::cli
