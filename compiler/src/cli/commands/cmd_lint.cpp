TML_MODULE("compiler")

//! # Lint Command Module
//!
//! This file serves as the entry point for the `tml lint` command.
//! Implementation is split across the `linter/` directory for maintainability.
//!
//! ## Module Structure
//!
//! ```text
//! cmd_lint.cpp             (this file - module documentation)
//! linter/
//!   ├─ linter_internal.hpp - Shared types and declarations
//!   ├─ helpers.cpp         - ANSI colors, naming checks, help text
//!   ├─ config.cpp          - Configuration from tml.toml
//!   ├─ style.cpp           - Style linting (whitespace, formatting)
//!   ├─ semantic.cpp        - Semantic linting (AST-based)
//!   ├─ discovery.cpp       - File discovery
//!   └─ run.cpp             - Main run_lint() function
//! ```
//!
//! ## Lint Rules
//!
//! | Category    | Rules                                   |
//! |-------------|-----------------------------------------|
//! | Style       | Tabs, trailing whitespace, line length  |
//! | Naming      | snake_case functions, PascalCase types  |
//! | Semantic    | Unused variables, dead code, shadowing  |
//!
//! ## Usage
//!
//! ```bash
//! tml lint                   # Lint current directory
//! tml lint --fix             # Auto-fix issues
//! tml lint --semantic        # Include semantic analysis
//! tml lint src/              # Lint specific directory
//! ```

// This file is intentionally minimal - all implementation is in linter/
