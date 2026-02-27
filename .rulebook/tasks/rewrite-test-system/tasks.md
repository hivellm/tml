# Tasks: Rewrite Test System

**Status**: In Progress (80%) — BLOCKED: coverage at 73.2%, must reach 80%+ to complete

## Phase 1: Process Abstraction (Cross-Platform Subprocess Management)

- [x] 1.1.1 Create `compiler/src/testing/` directory and CMakeLists integration
- [x] 1.1.2 Create `process.hpp` with Process struct (Win: HANDLE process/job/stdout_pipe/stderr_pipe, Unix: pid_t/pgid/stdout_fd/stderr_fd, common: exe_path/start_time/timeout)
- [x] 1.1.3 Win: `Process::launch()` — CreateProcessA + CREATE_NEW_PROCESS_GROUP + CREATE_SUSPENDED, CreateJobObject + AssignProcessToJobObject (nextest pattern for descendant termination), CreatePipe with SECURITY_ATTRIBUTES.bInheritHandle=TRUE
- [x] 1.1.4 Unix: `Process::launch()` — fork + execvp (NOT fork-only to avoid multi-thread deadlock), setpgid(0,0) in child for process group, pipe() + dup2 for stdout/stderr redirect
- [x] 1.1.5 `Process::is_done()` non-blocking — Win: WaitForSingleObject(process, 0), Unix: waitpid(pid, &status, WNOHANG)
- [x] 1.1.6 `Process::wait()` with timeout — Win: WaitForSingleObject(process, timeout_ms), Unix: poll on stdout_fd+stderr_fd with timeout + waitpid loop
- [x] 1.1.7 `Process::kill()` with signal escalation (nextest pattern) — Win: TerminateJobObject (kills entire process tree), Unix: kill(-pgid, SIGTERM) + grace period + kill(-pgid, SIGKILL)
- [x] 1.1.8 Pipe reading with deadlock prevention — multiplex stdout+stderr via PeekNamedPipe/poll (if only one drained, other fills 64KB OS buffer and blocks child)
- [x] 1.1.9 `Process::read_stdout()` / `Process::read_stderr()` — Win: PeekNamedPipe + ReadFile with bounded buffer, Unix: non-blocking read + poll
- [x] 1.1.10 Environment variable injection (TML_COVERAGE_FILE, TML_SHARD_INDEX, TML_TOTAL_SHARDS, PATH with runtime dirs)
- [x] 1.1.11 C++ unit tests: launch echo process, read stdout, verify exit code
- [x] 1.1.12 C++ unit tests: process timeout (launch sleep, verify kill after timeout, verify exit code)
- [x] 1.1.13 C++ unit tests: concurrent launch (10 processes, verify no handle/pipe/fd leaks)
- [x] 1.1.14 C++ unit tests: descendant process termination (launch parent that spawns child, kill parent, verify child also dead)
- [ ] 1.1.15 Verify all process tests pass on Windows, macOS, Linux

## Phase 2: JSON Protocol (NDJSON) and Dispatcher Generation

- [x] 2.1.1 Define NDJSON protocol in `protocol.hpp` — events: suite_start, test_start, test_output, test_pass, test_fail, test_crash, test_timeout, test_skip, coverage, suite_end (inspired by Go test2json)
- [x] 2.1.2 Implement `event_to_json()` serialization — hand-written JSON emitter, no external deps
- [x] 2.1.3 Implement `parse_json_event()` line parser — one JSON object per line, UTF-8 strings, no partial lines
- [x] 2.1.4 Rewrite `generate_ndjson_dispatcher_ir()` — generates LLVM IR with NDJSON events, timing via clock(), per-test pass/fail/crash counters
- [x] 2.1.5 Dispatcher `--list` mode — emit test metadata as JSON array (name, file, index), exit(0)
- [x] 2.1.6 Dispatcher `--run-all` mode — iterate all tests, emit NDJSON events per test, emit suite_end summary
- [x] 2.1.7 Dispatcher `--test-index=N` mode — run single test, emit NDJSON for just that test
- [ ] 2.1.8 Per-test timeout — deferred to Phase 3 coordinator (subprocess-level timeout via Process::wait)
- [ ] 2.1.9 Per-test crash isolation — deferred to Phase 3 coordinator (subprocess crash detection via Process exit code)
- [ ] 2.1.10 Coverage data emission — deferred to Phase 5 (depends on tml_cover_func integration)
- [ ] 2.1.11 Integration test: compile a 5-test suite, run dispatcher --run-all, verify NDJSON events
- [ ] 2.1.12 Integration test: compile suite with crashing test, verify crash detection
- [ ] 2.1.13 Integration test: compile suite, run dispatcher --list, verify JSON array
- [ ] 2.1.14 Verify dispatcher works on Windows, macOS, Linux

## Phase 3: Coordinator (Main Orchestration Loop)

- [x] 3.1.1 Create `coordinator.hpp` — public API: `run_tests(TestConfig)`, `TestRunResult`, `SuiteRunResult`, `CoordinatorTestResult` structs
- [x] 3.1.2 Reuse `discover_test_files()` from tester_discovery.cpp (superseded by Phase 5b independent discovery)
- [x] 3.1.3 Reuse `group_tests_into_suites()` with configurable max_per_suite (superseded by Phase 5b independent grouping)
- [x] 3.1.4 Reuse `compile_test_suite_exe()` (superseded by Phase 5b QueryContext-based compile pipeline)
- [x] 3.1.5 Implement parallel compilation — std::thread pool with atomic work queue, configurable compile_threads
- [x] 3.1.6 Implement parallel execution — Process::launch() with polling loop, NDJSON parsing via parse_json_event()
- [x] 3.1.7 Implement result aggregation — parse NDJSON events into SuiteRunResult, aggregate into TestRunResult
- [x] 3.1.8 Implement fail-fast mode — atomic should_stop flag, kill remaining subprocesses on first failure
- [x] 3.1.9 Implement suite filtering — filter suites by group/name substring matching
- [x] 3.1.10 Implement pattern filtering — filter test files by slash-normalized substring matching
- [x] 3.1.11 Implement --no-cache — pass through to compile_test_suite_exe()
- [ ] 3.1.12 Implement sharding — TML_SHARD_INDEX/TML_TOTAL_SHARDS env vars, round-robin suite assignment (deferred)
- [x] 3.1.13 Per-suite timeout via Process::wait(timeout), global timeout tracking
- [x] 3.1.14 Wire coordinator into cmd_test_v2.cpp via --new-runner flag
- [x] 3.1.15 Smoke test: `--new-runner --suite=core/str` passes 21 test files (3 suites), matches old system (253 test functions in those 21 files)

## Phase 4: Cache System (Lean, Go-Inspired)

- [x] 4.1.1 Create `testing_test_cache.hpp` — SuiteCacheEntry, TestResultCache class (CRC32C, single JSON file)
- [x] 4.1.2 Implement source file hashing — CRC32C of each .tml file in suite (sorted)
- [x] 4.1.3 Implement compiler version tracking — CRC32C of tml.exe binary (GetModuleFileNameA / /proc/self/exe)
- [x] 4.1.4 Implement flag tracking — hash of coverage + max_per_suite
- [x] 4.1.5 Implement cache-passing-only rule (Go model) — only cache suites where ALL tests pass
- [x] 4.1.6 Implement cache storage — single JSON file at build/debug/.new-test-cache.json
- [x] 4.1.7 Implement --no-cache bypass — skip all cache lookup and storage
- [x] 4.1.8 Integrate cache into coordinator — load/check/partition/update/save flow
- [x] 4.1.9 Write 20 unit tests: cache operations, round-trip, hash computation, cache-passing-only
- [x] 4.1.10 Smoke test: cold run (6.7s) → warm run (65ms, 107x speedup)
- [ ] 4.1.11 Implement dependency hash tracking — hash of transitively imported library .tml files (deferred)
- [ ] 4.1.12 Implement cache auto-trim — remove entries unused for 7 days (deferred)
- [ ] 4.1.13 Implement EXE binary caching — store compiled EXE for cache hits (deferred, uses existing compile cache)

## Phase 5: Coverage System (Reuse Existing Infrastructure)

- [x] 5.1.1 Add getenv + tml_coverage_write_file declarations to NDJSON dispatcher IR
- [x] 5.1.2 Add .str.cov_env string constant for "TML_COVERAGE_FILE" env var name
- [x] 5.1.3 Dispatcher: coverage write epilogue in run_all_tests() — getenv → null check → write_file
- [x] 5.1.4 Add covered_functions_count field to TestRunResult in coordinator.hpp
- [x] 5.1.5 Coordinator: inject TML_COVERAGE_FILE env var when launching subprocesses
- [x] 5.1.6 Coordinator: read coverage temp files after subprocess completes, aggregate into std::set
- [x] 5.1.7 Coordinator: generate coverage reports (superseded by Phase 5b independent coverage module)
- [x] 5.1.8 cmd_test_v2.cpp: display covered function count and coverage report path in summary
- [x] 5.1.9 Smoke test: --suite=core/str --coverage --no-cache — 282 functions covered, HTML+JSON reports generated
- [x] 5.1.10 Verified: NO hangs during coverage (the primary goal of this rewrite)
- [ ] 5.1.11 Full suite coverage test: verify numbers match old system (±5%)
- [ ] 5.1.12 Remove LLVM profiling dependencies (deferred to Phase 9)

## Phase 5b: V3 Independence (Zero cli/tester/ Dependencies)

- [x] 5b.1 Create `testing_discovery.hpp/cpp` — independent test file discovery + suite grouping (replaces cli/tester/tester_discovery.cpp + test_runner.cpp)
- [x] 5b.2 Create `testing_compile.hpp/cpp` — independent QueryContext-based compilation pipeline (replaces cli/tester/exe_test_runner.cpp)
- [x] 5b.3 Create `testing_coverage.hpp/cpp` — independent coverage reports: console + HTML + JSON (replaces cli/tester/library_coverage.cpp + library_coverage_report.cpp)
- [x] 5b.4 Rewrite `testing_coordinator.cpp` — zero `#include "cli/tester/*"` headers, uses only testing/ modules
- [x] 5b.5 Update CMakeLists.txt with 3 new source files
- [x] 5b.6 Verify zero `#include "cli/tester/*"` in compiler/src/testing/ and compiler/include/testing/
- [x] 5b.7 Build succeeds — all 3 new files compile without errors
- [x] 5b.8 96 C++ unit tests pass (Process, Protocol, DispatcherGen, Coordinator, TestCache)
- [x] 5b.8b Fix log level bug: test-v2 command now gets `meta=off` filter in dispatcher (same as test command) so TML_LOG_INFO messages are visible
- [x] 5b.9 Smoke test: `--new-runner --suite=core/str --no-cache` — 21/21 passed, summary prints correctly
- [x] 5b.10 Full suite test: `--new-runner --no-cache` — 1092 suites, 965 passed, 100 compile errors (pre-existing: OpenSSL/crypto 54, file/path runtime 8, codegen bugs 8, no-main tests 4), 27 test failures (sqlite, zlib, zstd), 5.6 min duration
- [x] 5b.11 Coverage integration test: `--new-runner --coverage --suite=core/str --no-cache` — 21/21 passed, 282 functions covered, HTML+JSON+JSONL reports generated
- [x] 5b.12 Full coverage test: `--new-runner --coverage --no-cache` — 1025 passed, 73 compile errors fixed (vcpkg linking), 4136/5647 (73.2%) library coverage, HTML+JSON+JSONL reports generated, NO HANGS
- [x] 5b.13 Coverage guard: partial runs (--suite or patterns) do NOT save HTML/JSON/JSONL — console only
- [x] 5b.14 Coverage regression protection: new report must not regress from previous coverage percentage
- [x] 5b.15 Coverage zero guard: zero covered functions = FATAL error, files NOT generated
- [x] 5b.16 LLVM profraw redirect: subprocess profraw files go to build/coverage/profraw/, not project root
- [x] 5b.17 .gitignore: added *.profraw and *.profdata patterns
- [x] 5b.18 Coverage HTML: 5 tabs (Overview, Module Coverage, Priorities, Uncovered, Test Suites) + search + accordion
- [x] 5b.19 Coverage stats fix: "Tests Passed" uses actual passed count from suites, not total_tests
- [x] 5b.20 Fix vcpkg library linking in v3 compile pipeline — added platform-specific linking for OpenSSL, sqlite3, ws2_32, advapi32, userenv, crypt32, /STACK:67108864
- [x] 5b.21 Fix `has_crypto_modules()` — changed from explicit module list to prefix-scanning for all `std::crypto::*` submodules
- [x] 5b.22 Fix runtime DLL discovery — added `ensure_runtime_dlls()` to copy vcpkg DLLs (zlib1, zstd, brotli*, sqlite3, libcrypto, libssl) to test exe cache dir
- [x] 5b.23 Fix sqlite module detection — prefix-scanning for `std::sqlite::*` submodules (both in testing_compile.cpp and builder_helpers.cpp)
- [ ] 5b.24 **MANDATORY**: Coverage must reach 80%+ (currently 73.2%) — migration NOT complete until this is achieved. Gap: ~380 uncovered functions from remaining 27 compile errors + test failures

## Phase 6: Reporter System (Multi-Format, Catch2-Inspired)

- [ ] 6.1.1 Create `reporter.hpp` — ITestReporter abstract interface with ~12 event methods (on_run_start/end, on_suite_start/end, on_test_start/pass/fail/crash/timeout/skip, on_coverage_report)
- [ ] 6.1.2 Implement MultiReporter — broadcasts events to all registered reporters simultaneously (Catch2 pattern)
- [ ] 6.1.3 Implement TerminalReporter — colored vitest-style output with progress indicator, suite grouping, failure details, summary
- [ ] 6.1.4 TerminalReporter: cross-platform ANSI — Win: SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING), detect isatty/TERM/WT_SESSION/COLORTERM. Fall back to plain text when piped.
- [ ] 6.1.5 Implement JsonReporter — NDJSON to file, one JSON object per event (--output=json)
- [ ] 6.1.6 Implement JunitXmlReporter — JUnit-compatible XML for CI (GitHub Actions, Jenkins) (--output=junit)
- [ ] 6.1.7 Implement CoverageHtmlReporter — function coverage HTML report with per-module tables
- [ ] 6.1.8 Implement CoverageJsonReporter — function coverage JSON at build/coverage/coverage.json
- [ ] 6.1.9 Implement profile statistics — --profile: compilation time per suite, execution time per suite, total phase breakdown
- [ ] 6.1.10 Implement leak detection reporting — per-file memory leak table (if leak detection enabled)
- [ ] 6.1.11 Add --output=terminal|json|junit CLI flag, support multiple simultaneous: --output=terminal --output=json:results.json

## Phase 7: Cross-Platform Validation

- [ ] 7.1.1 Build and test new system on Windows (primary development platform)
- [ ] 7.1.2 Build and test on Linux (Ubuntu/Debian with GCC and Clang)
- [ ] 7.1.3 Build and test on macOS (Apple Silicon with Clang)
- [ ] 7.1.4 Verify Process::launch/is_done/wait/kill/read on all 3 platforms
- [ ] 7.1.5 Verify dispatcher crash isolation: SEH catches on Windows, sigaction catches on Unix
- [ ] 7.1.6 Verify process group/job object termination kills descendants on all platforms
- [ ] 7.1.7 Verify ANSI terminal output on Windows Terminal, macOS Terminal, Linux xterm
- [ ] 7.1.8 Run full test suite on all 3 platforms, compare results with old system
- [ ] 7.1.9 Run coverage on all 3 platforms, verify no hangs

## Phase 8: Integration and Full Validation

- [ ] 8.1.1 Route cmd_test.cpp to new coordinator as primary path (remove --new-test-runner flag)
- [ ] 8.1.2 Run full test suite, verify pass/fail matches old system exactly (zero regressions)
- [ ] 8.1.3 Run coverage, verify percentages match old system (±1%)
- [ ] 8.1.4 Run benchmarks, verify timing within 10% of old system
- [ ] 8.1.5 Test suite filtering: --suite=core/str, --suite=std/json, --suite=compiler/codegen
- [ ] 8.1.6 Test fail-fast: --fail-fast with a failing test, verify early termination
- [ ] 8.1.7 Test --no-cache: force full recompilation, verify all suites rebuild
- [ ] 8.1.8 Test crash isolation: add test that segfaults, verify test_crash event + other tests continue
- [ ] 8.1.9 Test timeout isolation: add test that hangs, verify test_timeout event + other tests continue
- [ ] 8.1.10 Test JSON output: --output=json produces valid NDJSON (parse every line)
- [ ] 8.1.11 Test JUnit output: --output=junit produces valid XML (validate against schema)
- [ ] 8.1.12 Test sharding: TML_SHARD_INDEX=0 TML_TOTAL_SHARDS=4 runs ~25% of suites
- [ ] 8.1.13 Verify MCP test tool (mcp__tml__test) works with new system
- [ ] 8.1.14 Verify test cache: run twice, second run shows "(cached)" for passing suites

## Phase 9: Delete Old System

- [ ] 9.1.1 Delete entire compiler/src/cli/tester/ directory (25 files, 15,464 lines)
- [ ] 9.1.2 Delete lib/test/runtime/coverage.c (676 lines, replaced by dispatcher file-write)
- [ ] 9.1.3 Remove all old tester includes from other source files
- [ ] 9.1.4 Update CMakeLists.txt: remove old tester files, add compiler/src/testing/ files
- [ ] 9.1.5 Update compiler/include/ headers: remove old tester headers
- [ ] 9.1.6 Build compiler from clean state (scripts\build.bat --clean)
- [ ] 9.1.7 Run full test suite to verify clean deletion (zero regressions)
- [ ] 9.1.8 Run coverage to verify still works after deletion

## Phase 10: TML Runtime Updates

- [ ] 10.1.1 Simplify lib/test/runtime/coverage.c → simple file-write function (if kept) or remove entirely
- [ ] 10.1.2 Update lib/test/runtime/test.c assertions if needed for NDJSON protocol (stderr output format)
- [ ] 10.1.3 Update lib/test/src/ TML test framework modules if needed
- [ ] 10.1.4 Verify all test framework self-tests pass

## Phase 11: Documentation and Cleanup

- [ ] 11.1.1 Update docs/specs/09-CLI.md — test command documentation (new flags, output formats, sharding)
- [ ] 11.1.2 Update docs/specs/10-TESTING.md — new architecture description (subprocess model, NDJSON protocol, cache system)
- [ ] 11.1.3 Update CLAUDE.md — test system sections (file paths, build commands, MCP tools)
- [ ] 11.1.4 Update MCP server if any tool interfaces changed
- [ ] 11.1.5 Save architectural decisions to persistent memory
- [ ] 11.1.6 Archive improve-test-infrastructure items completed by this rewrite
