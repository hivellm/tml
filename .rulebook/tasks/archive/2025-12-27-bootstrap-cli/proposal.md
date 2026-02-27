# Proposal: Bootstrap CLI

## Why

The CLI (Command Line Interface) is the user-facing entry point to the TML compiler. It handles command-line argument parsing, orchestrates compilation stages, manages project configurations, and provides helpful error output. A well-designed CLI is essential for developer experience, enabling common workflows like building, testing, and running TML programs.

## What Changes

### New Components

1. **Argument Parser** (`src/cli/args.hpp`, `src/cli/args.cpp`)
   - Command-line argument parsing
   - Subcommand handling (build, run, check, test)
   - Flag and option parsing

2. **Driver** (`src/cli/driver.hpp`, `src/cli/driver.cpp`)
   - Compilation pipeline orchestration
   - File discovery and dependency resolution
   - Incremental compilation support

3. **Project Manager** (`src/cli/project.hpp`, `src/cli/project.cpp`)
   - tml.toml parsing
   - Dependency management
   - Build configuration

4. **Error Reporter** (`src/cli/reporter.hpp`, `src/cli/reporter.cpp`)
   - Colored terminal output
   - Error formatting with context
   - Warning and info messages

5. **Main Entry** (`src/main.cpp`)
   - Entry point
   - Signal handling
   - Exit code management

### CLI Commands

Based on `docs/specs/17-TOOLING.md`:
- `tml build` - Compile project
- `tml run` - Build and run
- `tml check` - Type check without codegen
- `tml test` - Run tests
- `tml fmt` - Format code
- `tml doc` - Generate documentation

## Impact

- **Affected specs**: 17-TOOLING.md
- **Affected code**: New `src/cli/` directory and `src/main.cpp`
- **Breaking change**: NO (new component)
- **User benefit**: Usable compiler interface
- **Dependencies**: Requires all previous compiler components

## Success Criteria

1. All CLI commands work correctly
2. Error messages are clear and helpful
3. Project configuration is parsed correctly
4. Compilation pipeline is orchestrated correctly
5. Exit codes are meaningful
6. Test coverage ≥95%
