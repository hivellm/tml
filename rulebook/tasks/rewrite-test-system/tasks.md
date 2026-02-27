# Tasks: Rewrite Test System

**Status**: Pending (0%)

## Phase 1: Process Abstraction (Cross-Platform Subprocess Management)

- [ ] 1.1.1 Create `compiler/src/testing/` directory and CMakeLists integration
- [ ] 1.1.2 Create `process.hpp` with Process struct (Win: HANDLE process/job/stdout_pipe/stderr_pipe, Unix: pid_t/pgid/stdout_fd/stderr_fd, common: exe_path/start_time/timeout)
- [ ] 1.1.3 Win: `Process::launch()` — CreateProcessW + CREATE_NEW_PROCESS_GROUP + CREATE_SUSPENDED, CreateJobObject + AssignProcessToJobObject (nextest pattern for descendant termination), CreatePipe with SECURITY_ATTRIBUTES.bInheritHandle=TRUE
- [ ] 1.1.4 Unix: `Process::launch()` — fork + execvp (NOT fork-only to avoid multi-thread deadlock), setpgid(0,0) in child for process group, pipe() + dup2 for stdout/stderr redirect
- [ ] 1.1.5 `Process::is_done()` non-blocking — Win: WaitForSingleObject(process, 0), Unix: waitpid(pid, &status, WNOHANG)
- [ ] 1.1.6 `Process::wait()` with timeout — Win: WaitForSingleObject(process, timeout_ms), Unix: poll on stdout_fd+stderr_fd with timeout + waitpid loop
- [ ] 1.1.7 `Process::kill()` with signal escalation (nextest pattern) — Win: TerminateJobObject (kills entire process tree), Unix: kill(-pgid, SIGTERM) + grace period + kill(-pgid, SIGKILL)
- [ ] 1.1.8 Pipe reading with deadlock prevention — multiplex stdout+stderr via poll/select (if only one drained, other fills 64KB OS buffer and blocks child)
- [ ] 1.1.9 `Process::read_stdout()` / `Process::read_stderr()` — Win: PeekNamedPipe + ReadFile with bounded buffer, Unix: non-blocking read + poll
- [ ] 1.1.10 Environment variable injection (TML_COVERAGE_FILE, TML_SHARD_INDEX, TML_TOTAL_SHARDS, PATH with runtime dirs)
- [ ] 1.1.11 C++ unit tests: launch echo process, read stdout, verify exit code
- [ ] 1.1.12 C++ unit tests: process timeout (launch sleep, verify kill after timeout, verify exit code)
- [ ] 1.1.13 C++ unit tests: concurrent launch (10 processes, verify no handle/pipe/fd leaks)
- [ ] 1.1.14 C++ unit tests: descendant process termination (launch parent that spawns child, kill parent, verify child also dead)
- [ ] 1.1.15 Verify all process tests pass on Windows, macOS, Linux

## Phase 2: JSON Protocol (NDJSON) and Dispatcher Generation

- [ ] 2.1.1 Define NDJSON protocol in `protocol.hpp` — events: suite_start, test_start, test_output, test_pass, test_fail, test_crash, test_timeout, test_skip, coverage, suite_end (inspired by Go test2json)
- [ ] 2.1.2 Implement `emit_json()` as generated IR functions (not string concat — use proper LLVM IR builder patterns)
- [ ] 2.1.3 Implement `parse_json_event()` line parser — one JSON object per line, UTF-8 strings, no partial lines
- [ ] 2.1.4 Rewrite `generate_dispatcher_ir()` — generate proper LLVM IR (not hand-written string), emit NDJSON events via emit_json calls
- [ ] 2.1.5 Dispatcher `--list` mode — emit test metadata as JSON array (name, file, line for each test), exit(0)
- [ ] 2.1.6 Dispatcher `--run-all` mode — iterate all tests, emit NDJSON events per test, emit suite_end summary
- [ ] 2.1.7 Dispatcher `--test-index=N` mode — run single test, emit NDJSON for just that test
- [ ] 2.1.8 Dispatcher per-test timeout — Win: timer thread with WaitForSingleObject(event, timeout_ms) sets flag, Unix: setitimer(ITIMER_REAL) + SIGALRM handler sets flag. Test checks flag. Emit test_timeout event on expiry.
- [ ] 2.1.9 Dispatcher per-test crash isolation — Win: SEH __try/__except wrapping each test call (catches access violation, stack overflow, div-by-zero, SIGILL). Unix: sigaction + sigaltstack for SIGSEGV/SIGBUS/SIGFPE/SIGILL/SIGABRT. On crash: emit test_crash event, continue to next test.
- [ ] 2.1.10 Dispatcher coverage data emission — call existing tml_cover_func() instrumentation, write covered function names to TML_COVERAGE_FILE, emit coverage event before suite_end
- [ ] 2.1.11 Integration test: compile a 5-test suite, run dispatcher --run-all, verify all NDJSON events parse correctly
- [ ] 2.1.12 Integration test: compile suite with crashing test, verify test_crash event emitted and remaining tests still run
- [ ] 2.1.13 Integration test: compile suite, run dispatcher --list, verify JSON array matches test count
- [ ] 2.1.14 Verify dispatcher works on Windows, macOS, Linux

## Phase 3: Coordinator (Main Orchestration Loop)

- [ ] 3.1.1 Create `coordinator.hpp` — public API: `int run_tests(TestOptions opts)`, `TestRunResult`, `SuiteResult`, `TestResult` structs
- [ ] 3.1.2 Implement `discovery.cpp` — recursive scan for *.test.tml, *.bench.tml, *.fuzz.tml, *.error.tml with skip-directory support
- [ ] 3.1.3 Implement suite grouping by module path — core/str, std/json, compiler/codegen etc. Configurable max_per_suite (default 8, --no-suite sets to 1)
- [ ] 3.1.4 Implement `compiler.cpp` — extract compilation logic from existing codegen pipeline (lex→parse→typecheck→borrow→codegen→object→link), output EXE only (no DLL)
- [ ] 3.1.5 Implement parallel compilation — std::thread pool with N compile workers (default: hardware_concurrency/2). Each worker compiles one suite to EXE independently.
- [ ] 3.1.6 Implement parallel execution — launch compiled suite EXEs as subprocesses, read NDJSON from stdout pipes concurrently. Max concurrent subprocesses configurable.
- [ ] 3.1.7 Implement result aggregation — parse NDJSON events from each subprocess, build TestRunResult with per-suite and per-test results
- [ ] 3.1.8 Implement fail-fast mode — on first test_fail/test_crash event, kill all remaining subprocesses (via Process::kill), report partial results
- [ ] 3.1.9 Implement suite filtering — --suite=core/str filters to lib/core/tests/str/ only
- [ ] 3.1.10 Implement pattern filtering — --filter=regex matches test names
- [ ] 3.1.11 Implement --no-cache — force recompile all suites, skip cache lookup
- [ ] 3.1.12 Implement sharding — TML_SHARD_INDEX/TML_TOTAL_SHARDS env vars, round-robin suite assignment (gtest model for CI parallelism)
- [ ] 3.1.13 Implement three-layer timeout (Go model) — per-test timeout in dispatcher, per-suite timeout in coordinator (kill subprocess if no output for N seconds), global timeout (--timeout flag)
- [ ] 3.1.14 Wire coordinator into cmd_test.cpp via --new-test-runner flag during development
- [ ] 3.1.15 Smoke test: run full suite with new coordinator, compare pass/fail counts with old system exactly

## Phase 4: Cache System (Dual-ID, Go-Inspired)

- [ ] 4.1.1 Create `cache.hpp` — CacheKey (ActionID + ContentID), CacheEntry, CacheManager
- [ ] 4.1.2 Implement Level 1 ActionID — SHA-256 of ("tmlTestResult" + sorted source hashes + sorted dependency hashes + compiler version + flags hash)
- [ ] 4.1.3 Implement Level 2 ContentID — SHA-256 of ("tmlTestResult" + compiled EXE binary hash + flags hash). Catches identical binaries from different source paths.
- [ ] 4.1.4 Implement source file hashing — SHA-256 of each .tml file content in suite
- [ ] 4.1.5 Implement dependency hash tracking — SHA-256 of all transitively imported library .tml files
- [ ] 4.1.6 Implement compiler version tracking — SHA-256 of tml.exe binary or embedded version string (Go's salt pattern)
- [ ] 4.1.7 Implement flag tracking — hash of backend, coverage, release, features, defines
- [ ] 4.1.8 Implement cache-passing-only rule (Go model) — only cache suites where ALL tests pass. Failing suites always re-run.
- [ ] 4.1.9 Implement cache storage — content-addressed directory: build/{config}/.test-cache/{00-ff}/ with 256-way sharding (Go pattern)
- [ ] 4.1.10 Implement EXE binary caching — store compiled EXE, copy on cache hit to skip recompilation entirely
- [ ] 4.1.11 Implement cache auto-trim — remove entries unused for 7 days, scan at most once per 24 hours
- [ ] 4.1.12 Implement --no-cache bypass — skip all cache lookup and storage
- [ ] 4.1.13 Write tests: cache hit (nothing changed), cache miss (source changed)
- [ ] 4.1.14 Write tests: cache miss (dependency changed), cache miss (flag changed), cache miss (compiler changed)
- [ ] 4.1.15 Write tests: failing suite NOT cached, passes after fix without manual cache clear

## Phase 5: Coverage System (Codegen-Based, No LLVM Profiling)

- [ ] 5.1.1 Create `coverage.hpp` — CoverageCollector, CoverageReport, FunctionCoverage structs
- [ ] 5.1.2 Verify existing tml_cover_func() codegen instrumentation works in EXE mode
- [ ] 5.1.3 Dispatcher: write covered function names + hit counts to TML_COVERAGE_FILE temp file
- [ ] 5.1.4 Coordinator: read coverage temp files from all finished subprocesses
- [ ] 5.1.5 Implement coverage aggregation — union of covered functions across all suites with hit count summation
- [ ] 5.1.6 Implement library function scanner — scan lib/core/, lib/std/, lib/test/ .tml files for function declarations (replace regex-based scanner with AST-based)
- [ ] 5.1.7 Implement coverage diff with previous run — show new coverage, lost coverage, percentage change
- [ ] 5.1.8 Remove ALL LLVM profiling dependencies — no profraw, profdata, llvm-cov, llvm-profdata. No std::system() calls.
- [ ] 5.1.9 Delete lib/test/runtime/coverage.c (676 lines) — replaced by simple file-write in dispatcher
- [ ] 5.1.10 Verify coverage numbers match old system (same functions covered, same percentages ±1%)
- [ ] 5.1.11 Test: run --coverage on full suite, verify NO hangs (the primary goal of this rewrite)

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
