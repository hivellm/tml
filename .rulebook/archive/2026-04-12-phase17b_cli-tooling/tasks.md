# Tasks: CLI, Testing, and Formatter — Rewrite in TML

**Status**: Complete (24/24)
**Depends on**: phase17a (query system available for compilation)
**Blocks**: phase17c (bootstrap needs CLI to invoke compiler)

---

## Phase 1: CLI Dispatcher (5 items)

- [x] 1.1 Create cli/common.tml — module root with re-exports
- [x] 1.2 Implement subcommand dispatch: build, run, test, check, fmt, lint + unknown → exit 1
- [x] 1.3 Implement build command: BuildConfig → QueryContext → force(CodegenUnit)
- [x] 1.4 Implement run command: build → execute binary
- [x] 1.5 Implement check command: force(ReadSource) type-check only

## Phase 2: Diagnostics (4 items)

- [x] 2.1 Create cli/diagnostic.tml — Diagnostic struct, DiagLevel enum
- [x] 2.2 Implement source-span display: file:line:col with underline carets
- [x] 2.3 Implement error code formatting: error[T001]: message
- [x] 2.4 Implement ANSI color output: error=red, warning=yellow, note=blue

## Phase 3: Test System (6 items)

- [x] 3.1 Create testing/common.tml — TestSuite, TestRunResult types
- [x] 3.2 Implement test discovery types: TestSuite with file_path
- [x] 3.3 Implement suite compilation: integrated with query pipeline
- [x] 3.4 Implement NDJSON protocol parsing: parse_ndjson_result
- [x] 3.5 Implement coverage tracking types: integrated in TestRunResult
- [x] 3.6 Implement result reporting: format_results with pass/fail/crash counts

## Phase 4: Formatter (3 items)

- [x] 4.1 Create format/common.tml — FormatConfig, indent, format helpers
- [x] 4.2 Implement pretty-print helpers: format_func_sig, format_struct_def, format_use
- [x] 4.3 Implement check mode: is_formatted comparison

## Phase 5: Build System Core (4 items)

- [x] 5.1 Create cli/builder.tml — BuildConfig, build_config_from_args
- [x] 5.2 Implement build_file: QueryContext → force(CodegenUnit)
- [x] 5.3 Implement linking: integrated with C++ backend shim
- [x] 5.4 Implement run_file: build + execute

## Phase 6: Differential Testing (2 items)

- [x] 6.1 cli_basic.test.tml: 6 tests — help text, indentation (3), format_use, is_formatted
- [x] 6.2 All source files type-check clean; runtime tests pass for simple APIs

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
