# Changelog — TML Backtrace Library (`lib/backtrace`)

All notable changes to the TML backtrace library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] — 2026-03-19

### Added
- **Test Framework Integration** — Backtraces automatically captured on test assertion failures
  - `--no-backtrace` flag to disable capture in tests
  - Shows source file and line number on assertion failures

- **Internal Frame Filtering** — Automatic filtering of runtime/system frames
  - Filtered: `panic`, `assert_tml`, `longjmp`, `setjmp`, `RaiseException`
  - Filtered: `__scrt_common_main_seh`, `invoke_main`, `_start`
  - Cleaner output focused on user code

- **Panic Handler Integration** — Backtrace captured automatically on panic

### Fixed
- **VEH Crash Handler** (2026-02-16) — Changed from unsafe `longjmp` to `EXCEPTION_CONTINUE_SEARCH`
  - Prevents test suite crashes from corrupted stack longjmp

## [0.1.0] — 2025-12-22

### Added
- Initial release
- `Backtrace` type with `capture()`, `resolve()`, `to_string()`, `print()`
- `BacktraceFrame` and `BacktraceSymbol` types
- Convenience functions: `print_backtrace()`, `capture_backtrace()`
- Windows: DbgHelp (`RtlCaptureStackBackTrace`, `SymFromAddr`, `SymGetLineFromAddr64`)
- Linux/macOS: `backtrace()` + `dladdr()`
- Lazy symbol resolution for performance
- Frame skipping with `capture_from(n)`
- Maximum 128 frames per capture
