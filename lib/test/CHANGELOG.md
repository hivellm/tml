# Changelog — TML Test Framework (`lib/test`)

All notable changes to the TML test framework will be documented in this file.

## [0.7.1] — 2026-03-22

### Changed
- **Test count** — 11,000+ tests across 1,400+ files (up from 9,000+)
- **Coverage** — 99% library coverage (15,528/15,628 functions)

## [0.7.0] — 2026-03-19

### Added
- **Subprocess-Based Test Architecture** — Each test suite compiles to an EXE and runs as a subprocess (Go model)
  - NDJSON protocol streams results from subprocess to coordinator
  - Eliminates DLL loading/unloading issues, supports true process isolation
  - Coverage via `TML_COVERAGE_FILE` env var — no LLVM profiling, no hangs

- **Suite-Level Filtering** (2026-02-19) — `--suite=` CLI flag for targeted test execution
  - `tml test --suite=core/str` runs only `core::str` test suites
  - `tml test --list-suites` shows all available suite groups with file/test counts
  - MCP `test` tool gains `suite` parameter

- **Test Runtime Archive** (2026-03-13) — Pre-built `tml_test_runtime.lib` archive
  - Each test suite links 1 .lib instead of ~15 individual .obj files
  - Built via `llvm-ar rcs`, cached and rebuilt only when source changes

- **Object Cache for LLVM Backend** (2026-03-14) — SHA-256 based `.obj` cache
  - Skips LLVM backend (IR→obj) on repeated runs for unchanged suites
  - Re-test time drops from seconds to milliseconds

- **Incremental IR Cache for Suites** (2026-03-14) — Per-file cache slots in suite mode
  - `CodegenUnitKey` includes `test_entry_index` for correct caching

- **Structured Test Output** — `--structured` flag returns parsed JSON results
  - `{ total, passed, failed, files, duration, failures[] }` format

- **Coverage Enhancements**
  - Incremental per-suite coverage save (survives mid-run crashes)
  - Assertion coverage tracking (builtin assertions tracked in reports)
  - Coverage history deduplication (reduced 4.1MB → 236KB)
  - Coverage scanner: skip bodyless behavior declarations

- **Parallel Execution Improvements**
  - No more hangs (parallel fix commit 43b1b721)
  - Coverage run ~11 minutes, full completion guaranteed

### Changed
- **Test System Migration** — Moved from DLL-based to subprocess-based architecture
  - Old: `--no-suite` (1 DLL per file), `--suite` (8 per DLL)
  - New: Each suite compiles to standalone EXE with NDJSON output
  - Testing infrastructure moved to `compiler/src/testing/`

### Fixed
- **Generic Struct Type Conflicts** (2026-03-14) — Fixed type redefinition when multiple test files instantiate the same generic struct with different type arguments
- **UBSan Checks Disabled** (2026-03-14) — Disabled false-positive UBSan in Zig CC toolchain
- **Test Crash/Failure Error Visibility** (2026-01-16) — Errors display without --verbose flag

## [0.6.0] — 2025-12-23

### Added
- **Test timeout support** — Default 20 second timeout per test, configurable via `--timeout=N`

## [0.5.0] — 2025-12-23

### Added
- **Benchmarking support** — `@bench` decorator for performance testing
  - Automatic 1000-iteration execution with microsecond timing

## [0.4.0] — 2025-12-23

### Added
- **Parallel test execution** — Multi-threaded test runner with auto-detection of CPU cores
- **Test filtering** — `--group=<path>` and `--suite=<path>` flags

## [0.3.0] — 2025-12-23

### Added
- **Module system integration** — Assertions require `use test` import
- **Full enum pattern matching** — `when` expressions work correctly in tests

### Changed
- **BREAKING**: Assertion functions removed from global scope, require `use test`

## [0.2.0] — 2025-12-23

### Added
- Complete runner, benchmark, and report modules
- Stats tracking, metadata management, progress indicators
- Multiple format support (Pretty, Quiet, Verbose)

## [0.1.0] — 2025-01-01

### Added
- Initial release with core assertions and test runner
- `@test`, `@should_panic`, `@ignore`, `@bench` decorators
- CLI integration (`tml test`)
- Polymorphic assertions (assert_eq, assert_ne, assert_gt, etc.)
