TML_MODULE("compiler")

//! # CLI Module Placeholder
//!
//! This file serves as a placeholder for the CLI module namespace. The actual
//! CLI implementation is distributed across multiple files in the `cli/`
//! directory for better organization and maintainability.
//!
//! ## CLI Architecture
//!
//! The CLI subsystem is organized as follows:
//!
//! ```text
//! cli/
//! ├── driver.hpp          # Main entry point (tml_main)
//! ├── dispatcher.cpp      # Command line parsing and dispatch
//! ├── cmd_build.cpp       # 'tml build' command
//! ├── cmd_test.cpp        # 'tml test' command
//! ├── cmd_format.cpp      # 'tml fmt' command
//! ├── cmd_debug.cpp       # 'tml lex/parse/check' commands
//! ├── cmd_cache.cpp       # 'tml cache' command
//! ├── cmd_init.cpp        # 'tml init' command
//! ├── cmd_pkg.cpp         # 'tml pkg' command
//! ├── builder/            # Build system implementation
//! │   ├── build.cpp       # Main build logic
//! │   ├── run.cpp         # Run command implementation
//! │   └── helpers.cpp     # Shared build utilities
//! ├── tester/             # Test framework implementation
//! │   ├── run.cpp         # Test execution
//! │   ├── discovery.cpp   # Test file discovery
//! │   └── output.cpp      # Test result formatting
//! └── linter/             # Lint system implementation
//!     ├── run.cpp         # Lint execution
//!     ├── style.cpp       # Style checks
//!     └── semantic.cpp    # Semantic checks
//! ```
//!
//! ## See Also
//!
//! - `cli/driver.hpp` - Main CLI driver
//! - `docs/specs/09-CLI.md` - CLI specification

namespace tml::cli {

// This file intentionally left minimal.
// See the cli/ directory for the actual implementation.

} // namespace tml::cli
