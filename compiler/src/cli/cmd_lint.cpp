// cmd_lint.cpp - Entry point for lint command
// Implementation split into linter/ directory for maintainability:
//   - linter/linter_internal.hpp  - Shared header with types and declarations
//   - linter/helpers.cpp          - ANSI colors, naming checks, help text
//   - linter/config.cpp           - Configuration loading from tml.toml
//   - linter/style.cpp            - Style linting (tabs, trailing whitespace, etc.)
//   - linter/semantic.cpp         - Semantic linting (AST-based analysis)
//   - linter/discovery.cpp        - File discovery and linting
//   - linter/run.cpp              - Main run_lint function

// This file is intentionally minimal - all implementation is in linter/
