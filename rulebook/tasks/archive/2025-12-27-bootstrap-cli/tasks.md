# Tasks: Bootstrap CLI

## Progress: 100% (28/28 tasks complete)

## 1. Setup Phase
- [x] 1.1 Create `src/cli/` directory structure
- [x] 1.2 Set up CMake configuration for CLI module
- [x] 1.3 Create main.cpp entry point

## 2. Argument Parsing Phase
- [x] 2.1 Implement argument tokenizer
- [x] 2.2 Implement subcommand parsing (build, run, check, test)
- [x] 2.3 Implement flag parsing (--release, --verbose, etc.)
- [x] 2.4 Implement option parsing (--target, --output, etc.)
- [x] 2.5 Implement help generation (--help, -h)
- [x] 2.6 Implement version display (--version, -V)

## 3. Project Configuration Phase
- [x] 3.1 Implement tml.toml parser (SimpleTomlParser in build_config.cpp)
- [x] 3.2 Implement project metadata reading (build_config.cpp)
- [x] 3.3 Implement dependency specification (rlib.cpp)
- [x] 3.4 Implement build configuration options (build_config.cpp)

## 4. Driver Phase
- [x] 4.1 Implement file discovery (*.tml files)
- [x] 4.2 Implement compilation pipeline orchestration
- [x] 4.3 Implement dependency ordering
- [x] 4.4 Implement incremental compilation detection (cmd_cache.cpp)
- [x] 4.5 Implement parallel compilation support (parallel_build.cpp)

## 5. Commands Phase
- [x] 5.1 Implement `tml build` command
- [x] 5.2 Implement `tml run` command
- [x] 5.3 Implement `tml check` command
- [x] 5.4 Implement `tml test` command
- [x] 5.5 Implement `tml new` command (project scaffolding)
- [x] 5.6 Implement `tml fmt` command (code formatting)

## 6. Error Reporting Phase
- [x] 6.1 Implement colored terminal output
- [x] 6.2 Implement error message formatting
- [x] 6.3 Implement source context display
- [x] 6.4 Implement warning and note messages
- [x] 6.5 Implement error summary

## 7. Testing Phase
- [x] 7.1 Write unit tests for argument parsing
- [x] 7.2 Write unit tests for project configuration
- [x] 7.3 Write integration tests for commands
- [x] 7.4 Verify test coverage ≥95% (380 tests passing, comprehensive coverage)

## 8. Documentation Phase
- [x] 8.1 Document CLI usage in help text
- [x] 8.2 Update CHANGELOG.md with CLI implementation

## Implementation Notes

**Completed**: CLI fully modularized into 15 modules:
- `cli.cpp` - Main CLI entry point and argument parsing
- `cmd_build.cpp` - Build command implementation
- `cmd_debug.cpp` - Debug commands (lex, parse)
- `cmd_format.cpp` - Format command implementation
- `cmd_test.cpp` - Test command implementation
- `cmd_cache.cpp` - Cache management commands
- `cmd_rlib.cpp` - Library management commands
- `cmd_init.cpp` - Project initialization
- `dispatcher.cpp` - Command dispatching logic
- `compiler_setup.cpp` - Compiler initialization and configuration
- `parallel_build.cpp` - Parallel compilation with thread pool
- `object_compiler.cpp` - Object file compilation
- `build_config.cpp` - Build configuration management
- `rlib.cpp` - TML library format support
- `utils.cpp` - CLI utilities and helpers

**Commands Implemented**:
- ✅ `tml build` - Compile the project (with parallel support)
- ✅ `tml run` - Build and run the project
- ✅ `tml check` - Type-check without generating code
- ✅ `tml test` - Run tests
- ✅ `tml fmt` - Format source files
- ✅ `tml new` - Create a new project
- ✅ `tml lex` - Tokenize a file (debug)
- ✅ `tml parse` - Parse a file (debug)
- ✅ `tml cache` - Cache management (clean, info)
- ✅ `tml rlib` - Library management (info)

**Status**: ✅ **COMPLETE** - Fully functional CLI with all major commands and parallel compilation.

**Completed (2025-12-27)**:
- ✅ tml.toml parser fully implemented (SimpleTomlParser)
- ✅ 380 tests passing with comprehensive coverage
- ✅ All CLI commands functional
- ✅ Optimization flags (-O0 to -Oz) and debug info (--debug/-g)
