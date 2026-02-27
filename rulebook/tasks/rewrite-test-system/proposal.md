# Proposal: Rewrite Test System

**Status**: Approved
**Priority**: Critical
**Estimated Effort**: 8-12 weeks
**Impact**: Core developer infrastructure

---

## Why

The current TML test system is a **15,464-line patchwork across 25 C++ files** that has grown through accretion rather than design. It has critical bugs (coverage hangs, suite codegen bug, pipe leaks), multiple workarounds masking unresolved issues, and two incompatible execution models neither of which works fully. This is the most important developer tool in the project and it needs to be rebuilt from scratch with a clean architecture inspired by Go, Rust, and industry best practices.

## What Changes

Complete replacement of the test runner infrastructure with a subprocess-based architecture:
- **New directory**: `compiler/src/testing/` (parallel to existing `compiler/src/cli/tester/`)
- **Subprocess execution only** (no DLL loading) for crash isolation
- **JSON protocol** between test subprocess and coordinator
- **Correct test caching** (source + dependencies + compiler version + flags)
- **Reliable coverage** via codegen instrumentation (no LLVM profiling runtime)
- **Per-test timeouts** (hung test does not lose other results)
- **Cross-platform from day one**: Windows, macOS, Linux
- **Complete replacement**: old system (DLL + EXE modes) deleted entirely once new system is validated
- **Code reduction**: 25 files / 15,464 lines to ~15 files / ~7,000 lines

## Impact

- Affected specs: docs/specs/09-CLI.md, docs/specs/10-TESTING.md
- Affected code: compiler/src/testing/ (NEW), compiler/src/cli/tester/ (DELETED), lib/test/runtime/, cmd_test.cpp
- Breaking change: NO (same CLI interface)
- User benefit: Coverage works, crash isolation, per-test timeouts, JSON output, cross-platform

---

## 1. Problem Statement (20 Issues)

### Critical Architectural

1. **Two incompatible execution models** (DLL vs EXE) - neither fully works. ~80% code duplication between `test_runner.cpp` (DLL) and `exe_test_runner.cpp` (EXE). Any bug fix requires 2x effort.
2. **Suite merging codegen bug** - workaround forces `max_per_suite=1` destroying 8x speedup. TWO separate workarounds: `suite_execution.cpp:244` (DLL mode) and `exe_suite_runner.cpp:129` (EXE mode, forces individual for ALL tests).
3. **Coverage hangs** - `tml test --coverage` hangs after `fmt_helpers_sign.test`. LLVM profiling runtime interacts badly with DLL loading. Coverage mode forces single-threaded compilation (`suite_execution.cpp:891`).
4. **Disabled precompiled symbol cache** - `#if 0` block in `test_runner.cpp:150-267` (117 dead lines). Comment: "Causes linkage conflicts." Every suite rebuilds all symbols from scratch.
5. **Thread-unsafe output capture** - `OutputCapture` uses global `dup2` fd redirection. DISABLED in suite mode (`test_runner_exec.cpp:645`) because it "causes deadlocks in parallel mode." Test output silently discarded.

### Reliability

6. **Fragile crash handling** - Global `char` arrays without synchronization: `g_current_test_name[512]`, `g_current_test_file[1024]`, `g_current_suite_name[256]`, `g_current_phase[128]`. Written from any thread. Crash filter writes to `crash_report.txt` — not thread-safe.
7. **String-based error detection** - Matches substrings "Lexer", "Parser", "Type", "Codegen" in error messages (`execution.cpp:55-60`). Fragile and locale-dependent.
8. **Incomplete Unix async** - `launch_subprocess_async()` returns empty handle on Unix (`exe_test_execution.cpp:262`). `subprocess_is_done()` returns false on Unix (line 333). `wait_for_subprocess()` can't read output (TODO line 305). Entire coverage/EXE path is **Windows-only**.
9. **Pipe handle leak** - Handles not stored in `AsyncSubprocessHandle`. Explicit TODO acknowledging the leak.
10. **Cache misses library changes** - CRC32C hash labeled as "sha512" (misleading). `get_known_suite_hashes()` returns EMPTY vector — orphan cleanup is broken. Cache JSON serialized on hot path after every suite completion.
11. **Watchdog kills entire process** - `TerminateProcess` on timeout (`test_runner_exec.cpp:721`) kills the ENTIRE process, not just the hung test. No recovery possible.

### Performance and Complexity

12. **I/O-bound linking** (37s/100s) - NVMe at 100% utilization during link phase.
13. **Redundant lex/parse** - Both `test_runner.cpp` (lines 466-498) and `exe_test_runner.cpp` (lines 274-304) lex+parse ALL test files just to extract `use` declarations, even for cached files.
14. **EXE mode compiles sequentially** - `exe_suite_runner.cpp:229` iterates suites in a serial `for` loop. 15 cores idle.
15. **Thread count capped at [2,4] per suite** - `calc_codegen_threads()` limits to 2-4 threads per suite even on 16+ core machines.
16. **Massive code duplication** (~2500 lines total):
    - `PreviousCoverage` copy-pasted between `suite_execution.cpp` and `exe_suite_runner.cpp`
    - Library scanner (~200 lines) copy-pasted between `library_coverage.cpp` and `library_coverage_report.cpp`
    - Borrow error switch (137 lines) duplicated for Polonius vs NLL in `diagnostic_execution.cpp`
    - `calc_codegen_threads()` vs `exe_calc_codegen_threads()` with different bounds
17. **Fragile external tool invocation** - `std::system()` for llvm-profdata/llvm-cov with hand-constructed command strings. No subprocess management, no error codes, no timeout.
18. **Hand-written LLVM IR** - `exe_dispatcher_gen.cpp` generates raw IR as string concatenation with hardcoded constant sizes. Off-by-one potential.
19. **25 files, 15,464 lines, 4 execution paths** - DLL individual, DLL suite, EXE individual, EXE suite. Each with different behavior.
20. **Coverage runtime** - `coverage.c` (676 lines) with fixed 4093-entry lock-free hash table, manual fprintf-based HTML/JSON report generation.

---

## 2. Deep Research: Lessons from Go, Rust, and C++ Test Frameworks

### 2.1 Go (`testing` package, `cmd/go/internal/test`)

**Source-code-level analysis of Go's test infrastructure:**

#### Test Binary Generation
- `loadTestFuncs()` walks AST to discover `TestXxx`, `BenchmarkXxx`, `ExampleXxx` functions
- Generates `_testmain.go` via `text/template` with static test array and `testing.MainStart()`
- One binary per package (amortizes compile/link cost across all tests in package)
- Test binaries accept `-test.run`, `-test.timeout`, `-test.v` flags

#### Parallel Execution (t.Parallel)
- Three-channel barrier pattern per test:
  - `barrier chan bool` — parent closes to release all waiting parallel subtests
  - `signal chan bool` — subtest notifies parent of completion (buffered, cap 1)
  - `startParallel chan struct{}` — global semaphore limiting concurrent parallel tests
- `testState.maxParallel` defaults to GOMAXPROCS
- Barrier is per-parent, not global — parent releases only its own parallel children

#### Dual-ID Cache System (CRITICAL — adopt this)
- **Level 1 (ActionID)**: SHA-256 of `"testResult" + BuildActionID + cacheableArgs`
- **Level 2 (ContentID)**: SHA-256 of `"testResult" + BuildContentID + cacheableArgs` — catches identical binaries from different source paths
- **Level 3 (TestInputsID)**: SHA-256 of test log entries (getenv, open, stat, chdir from previous run)
- Cache salted with `runtime.Version()` — prevents cross-version pollution
- Only caches **passing** tests — failing tests always re-run
- `-count=1` bypasses cache (flag not in cacheable list)
- Cache stored in content-addressable format: `$GOCACHE/{00-ff}/{hex-id}-a` (action) and `-d` (data)
- Auto-trim: 24h scan interval, 5-day retention, mtime-based access tracking

#### Coverage (Source-Level, Go 1.20+)
- Compiler-level instrumentation inserts counter arrays at block boundaries
- Three modes: `set` (binary, single MOV instruction ~3% overhead), `count` (increment), `atomic` (thread-safe)
- Meta-data and counter data as separate binary files with FNV-128a hashing
- Per-process counter files with `%p` (PID) and `%m` (binary signature) patterns
- `go tool covdata merge` combines across runs/platforms
- **NO LLVM profiling runtime** — all instrumentation at source/compiler level

#### test2json Protocol
- Event types: `start`, `run`, `pass`, `fail`, `skip`, `pause`, `cont`, `output`
- Each event has `Time`, `Action`, `Package`, `Test`, `Elapsed`, `Output`
- Lines prefixed with `=== ` are update lines, `--- ` are report lines
- `^V` (0x16) marker byte for JSON mode output parsing

#### Timeout
- `time.AfterFunc(*timeout, panicHandler)` — goroutine that dumps all running tests then panics
- `running` sync.Map tracks currently executing tests with start times
- Default 10 minutes per test binary

**What to adopt from Go:**
- Dual-ID cache with environment tracking (Level 3)
- Only cache passing tests
- Source-level coverage (no LLVM profiling runtime)
- test2json-style structured output with per-test attribution
- Timeout with running-test dump
- One binary per suite (amortize overhead)

### 2.2 Rust (`libtest` + `cargo-nextest`)

**Source-code-level analysis:**

#### libtest Internals
- `#[test]` generates `TestDescAndFn` instances at compile time in static array
- `test_main_static()` → `test_main()` → `console::run_tests_console()`
- `run_tests()` uses channel-based result collection: `(tx, rx) = channel::<CompletedTest>()`
- `RunStrategy`: `InProcess` (thread + `catch_unwind`) or `SpawnPrimary` (subprocess for `panic=abort`)
- Multi-threaded path: fill slots up to concurrency limit, `recv_timeout` for completion, `TimeoutEntry` queue for deadline tracking
- `CompletedTest` struct: `id`, `desc`, `result`, `exec_time`, `stdout: Vec<u8>`
- `RunningTest`: `HashMap<TestId, JoinHandle>` with deterministic hasher

#### Output Capture (AVOID this pattern)
- Uses `io::set_output_capture()` — thread-local `Arc<Mutex<Vec<u8>>>` buffer
- Only captures `print!`/`println!` macros, NOT direct `io::stdout().write()` or child processes
- Global thread pools (Rayon, Tokio) do NOT inherit capture context
- **Fundamentally leaky** — TML should use pipe-based capture via subprocess instead

#### cargo-nextest (Process-Per-Test)
- Two-phase: `--list --format terse` for discovery, then `<binary> <name> --nocapture --exact` for execution
- Per-test process isolation via:
  - Unix: `setpgid` process group + SIGTERM/SIGKILL escalation
  - Windows: Job objects for descendant process tree management
- Configurable timeout: `slow-timeout = { period = "60s", terminate-after = 2, grace-period = "10s" }`
- Retry with exponential backoff: `retries = { backoff = "exponential", count = 3, delay = "1s", jitter = true }`
- Flaky classification: passes on retry = "flaky" (ultimately successful, exit code 0)
- Hash-based test sharding: deterministic hash of binary+name, stable when tests added/removed
- Performance: 1.4x-3.4x speedup over `cargo test` from cross-binary parallelism

#### Rust Coverage (-C instrument-coverage)
- MIR-level instrumentation via `llvm.instrprof.increment` intrinsic
- Four granularity levels: function, instantiation, line, region
- `compiler-rt` profiler runtime linked statically
- `LLVM_PROFILE_FILE` with `%p`/`%m` patterns for per-process output
- `llvm-profdata merge -sparse` combines profraw files
- **Moved FROM gcov TO source-based** because LLVM can't understand Rust control flow

**What to adopt from Rust/nextest:**
- Process-per-suite with per-test crash isolation within the dispatcher
- SIGTERM-wait-SIGKILL escalation on Unix, Job objects on Windows
- Configurable timeout with grace period
- Hash-based sharding for CI parallelism
- Two-phase protocol: `--list` for discovery, `--run-all` / `--test-index=N` for execution
- DO NOT adopt: thread-local output capture (fundamentally broken)
- DO NOT adopt: test result caching (neither Rust nor nextest caches results — only compilation)

### 2.3 Google Test + Catch2

**Source-code-level analysis:**

#### gtest: Static Registration (Zero-Cost at Runtime)
- `TEST()` macro expands to class definition + static `TestInfo*` initialized via `MakeAndRegisterTestInfo()`
- File-scope static variable → constructor runs during C runtime static initialization
- `UnitTest::GetInstance()` singleton with function-local static (thread-safe in C++11)
- Fresh test instance created per test (heap alloc + dealloc, negligible overhead)
- Registration cost: O(1) per test, sub-millisecond for 10,000 tests

#### gtest: Death Tests (CRITICAL for crash testing)
- Class hierarchy: `DeathTest` → `DeathTestImpl` → `WindowsDeathTest` / `ForkingDeathTest` → `NoExecDeathTest` / `ExecDeathTest`
- Parent-child communicate via anonymous pipe, child writes status byte before exit
- Windows: `CreateProcessA()` with `--gtest_internal_run_death_test` flag
- POSIX "fast" style: `fork()` only (risks deadlock in multi-threaded parent)
- POSIX "threadsafe" style: `fork()` + `exec()` (re-launches binary with targeted test)
- Linux optimization: `clone()` instead of `fork()` (faster, avoids hanging issues)
- Death test suites run FIRST (before other tests, because forking is safer early)

#### gtest: Sharding
- `GTEST_TOTAL_SHARDS` / `GTEST_SHARD_INDEX` environment variables
- Algorithm: test `i` runs on shard `k` if `i % GTEST_TOTAL_SHARDS == k`
- Simple round-robin, does not account for test execution time
- `gtest-parallel` external tool provides time-weighted distribution

#### gtest: Output Formats
- `TestEventListeners` collection broadcasts to all registered listeners
- JSON/XML listeners accumulate results, write complete document in `OnTestProgramEnd`
- `RecordProperty("key", "value")` for custom metadata in structured output
- Linear-time glob matching (Russ Cox algorithm) for `--gtest_filter`

#### gtest: Crash Handling (KEY LIMITATION)
- Windows: SEH (`__try/__except`) catches access violations, stack overflows
- POSIX: **NO signal handlers** for SIGSEGV during normal execution. Segfault = process dies.
- Rationale: impossible to cleanly recover from segfault (corrupted memory state)
- Only safe crash handling is via process isolation (death tests)

#### Catch2: Section Re-execution Model (DO NOT adopt)
- `SECTION("name")` creates tree of test paths, test case re-executed per leaf section
- O(leaves) × O(depth) redundant setup execution
- Dynamic discovery (must run to discover sections) — cannot list tests without executing
- Too complex for LLM comprehension, hidden control flow

#### Catch2: Multi-Reporter Architecture (ADOPT)
- `IEventListener` with 21 events across 4 groups (program, test, section, assertion)
- `StreamingReporterBase` for console, `CumulativeReporterBase` for JSON/XML
- Multiple reporters simultaneously: console to stdout AND JSON to file
- 8 built-in reporters: Console, Compact, JUnit, XML, JSON, SonarQube, TAP, Automake

#### Catch2: Crash Handling
- Windows SEH: `__try/__except` for SIGSEGV, stack overflow, div-by-zero, SIGILL
- POSIX: `sigaction` for SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT with `sigaltstack` for stack overflow
- Reports crash but cannot safely continue (same limitation as gtest on POSIX)

**What to adopt from C++ frameworks:**
- Event-based reporter interface (Catch2 model) with simultaneous multi-format output
- Death test subprocess pattern (gtest) for crash isolation
- Static registration pattern (gtest) — zero-cost compile-time discovery
- Flat test model (gtest) — NOT section re-execution (Catch2)
- SEH on Windows + signal handlers on POSIX for crash reporting within dispatcher
- Round-robin sharding via environment variables (gtest) for CI
- DO NOT adopt: test result caching (no framework caches results)

### 2.4 Cross-Framework Synthesis: Engineering Decisions

| Decision | Go | Rust libtest | Rust nextest | gtest | Catch2 | **TML (New)** |
|----------|----|----|------|-------|--------|---------|
| Execution unit | Binary per package | Thread per test | Process per test | In-process per test | In-process per section | **EXE per suite** |
| Crash isolation | Process boundary | `catch_unwind` | Process boundary | SEH/death tests | SEH/signals | **Process + SEH/signals** |
| Output capture | Per-goroutine buffer | Thread-local redirect | Process pipes | No capture | No capture | **Process pipes** |
| Cache scope | Test results | Compilation only | Compilation only | None | None | **Test results (Go model)** |
| Cache key | Source+deps+env+version | Fingerprints | N/A | N/A | N/A | **Source+deps+compiler+flags** |
| Coverage | Source-level (compiler) | MIR-level (LLVM) | N/A | N/A | N/A | **Codegen-level (tml_cover_func)** |
| Structured output | test2json NDJSON | JSON/Pretty/Terse | JUnit XML/JSON | JSON/XML | JSON/XML/JUnit/TAP | **NDJSON + JUnit XML** |
| Timeout | Per-binary (10min) | Per-test (recv_timeout) | Per-test (configurable) | None | None | **Per-test + per-suite** |
| Parallelism | Package-level + t.Parallel | Thread-level (-parallel) | Process-level | None built-in | None | **Suite-level + per-suite** |
| Sharding | -p flag (packages) | N/A | hash:m/n | env vars | N/A | **Round-robin env vars** |

---

## 3. New Architecture

### 3.1 Principles

1. **Subprocess-only** — No DLL loading. Each test suite compiles to an EXE. Eliminates: output capture races, crash non-isolation, coverage hangs, DLL unload issues, TLS conflicts. Cost: ~5ms/suite process creation overhead (measured: nextest process-per-test adds ~1-10ms on Linux, ~10ms on Windows).

2. **Compile-time registration** — `@test` generates dispatch table entries in the binary at compile time (Go + Rust model). Zero runtime discovery overhead. Test list available via `--list` flag on the binary itself.

3. **Dual-ID cache** — Go's proven dual-level cache: ActionID (hash of build inputs) + ContentID (hash of binary). Only cache passing tests. SHA-256 salted with compiler version. Environment tracking: record which env vars and files the test consulted.

4. **Codegen coverage** — TML compiler already inserts `tml_cover_func()` calls. Subprocess writes covered function names to file via `TML_COVERAGE_FILE` env var. Coordinator reads and aggregates. No LLVM profiling runtime, no profraw/profdata, no hangs.

5. **JSON protocol (NDJSON)** — One JSON object per line from subprocess stdout. Inspired by Go's `test2json`. Events: `suite_start`, `test_start`, `test_pass`, `test_fail`, `test_crash`, `test_timeout`, `coverage`, `suite_end`. Coordinator parses in real-time for streaming output.

6. **Multi-format reporter** — Catch2-inspired event listener interface. Multiple reporters active simultaneously: TerminalReporter (console) + JsonReporter (file) + JUnitReporter (CI). Reporter receives events and formats output independently.

7. **Cross-platform** — Windows, macOS, Linux from day one. Only `process.cpp` and `reporter.cpp` have platform-specific code behind `#ifdef`. Everything else is standard C++17.

8. **Per-test crash isolation** — Dispatcher binary uses SEH on Windows, `sigaltstack`+`sigaction` on POSIX for per-test crash catching. On crash: emit `test_crash` JSON event, continue to next test. Coordinator has suite-level timeout as safety net (kills subprocess if no output for N seconds).

### 3.2 Development Strategy

The new system is built in a **separate directory** (`compiler/src/testing/`) that does NOT touch the old code at all. Both systems coexist during development:

```
compiler/src/
  cli/tester/       <-- OLD (untouched during development)
  testing/          <-- NEW (built in parallel)
```

- `cmd_test.cpp` routes to the new system via an internal flag during development
- Once the new system passes ALL validation (same results, same coverage, no regressions), the old `cli/tester/` directory is **deleted entirely**
- No `--legacy-runner` flag. No fallback. Clean replacement.

### 3.3 Architecture

```
                        tml test (CLI)
                            |
              +-------------+-------------+
              |             |             |
         Discovery     Coordinator    Reporter(s)
              |             |             |
              +-------------+------+------+
                            |      |
                      +-----+------+--------+
                      |     |      |        |
                 Compile Compile Compile Compile  (thread pool, N workers)
                      |     |      |        |
                  suite.exe suite.exe suite.exe   (EXE binaries)
                      |     |      |        |
                  subprocess subprocess subprocess (async I/O)
                      |     |      |        |
                   JSON stdout (NDJSON structured results)
                      |     |      |        |
                  +---+-----+------+--------+
                  |                          |
              Aggregator              Coverage Collector
                  |                          |
            TestRunResult              CoverageReport
```

### 3.4 JSON Protocol (NDJSON)

Inspired by Go's `test2json` format. Each line is a self-contained JSON object.

```json
{"event":"suite_start","name":"core_str","test_count":12,"file_count":3}
{"event":"test_start","index":0,"name":"test_concat","file":"basic.test.tml"}
{"event":"test_output","index":0,"stream":"stdout","data":"running concat..."}
{"event":"test_pass","index":0,"duration_us":1523}
{"event":"test_fail","index":1,"name":"test_split","error":"assert_eq: expected 3, got 2","file":"basic.test.tml","line":42,"duration_us":892}
{"event":"test_crash","index":2,"name":"test_bad","signal":"SIGSEGV","duration_us":0}
{"event":"test_timeout","index":3,"name":"test_hang","timeout_ms":20000}
{"event":"test_skip","index":4,"name":"test_platform","reason":"windows-only"}
{"event":"coverage","functions":["str_concat","str_split","str_len"],"hits":[5,3,0]}
{"event":"suite_end","passed":10,"failed":1,"crashed":1,"timed_out":1,"skipped":1,"duration_us":45230}
```

**Protocol guarantees:**
- Events are ordered: `suite_start` first, `suite_end` last
- `test_start` always precedes `test_pass`/`test_fail`/`test_crash`/`test_timeout` for same index
- `test_output` events only appear between `test_start` and test completion
- One JSON object per line (newline-delimited), no partial lines
- All strings are UTF-8, no control characters except in `data` fields

### 3.5 Cache System (Go-Inspired Dual-ID)

**Cache key components:**
```
Level 1 (ActionID) = SHA-256(
    "tmlTestResult" +
    sorted_source_file_hashes +    // SHA-256 of each .tml file in suite
    sorted_dependency_hashes +     // SHA-256 of all transitively imported library .tml files
    compiler_version_hash +        // SHA-256 of tml.exe binary or embedded version string
    flags_hash                     // backend, coverage, release, features, defines
)

Level 2 (ContentID) = SHA-256(
    "tmlTestResult" +
    binary_content_hash +          // SHA-256 of compiled suite.exe
    flags_hash
)
```

**Cache rules (from Go):**
- Only cache PASSING suites — failing suites always re-run
- `-count=1` or `--no-cache` bypasses cache entirely
- Cache salted with compiler version — prevents stale results after compiler update
- Cache stored in `build/{config}/.test-cache/` as JSON index + binary cache
- Auto-trim: remove entries unused for 7 days, scan at most once per 24 hours

### 3.6 Dispatcher Binary Architecture

Each suite compiles to a self-contained EXE with a generated `main()` that:

```
suite.exe --list              → JSON array of test metadata, then exit
suite.exe --run-all           → Run all tests, emit NDJSON events
suite.exe --test-index=N      → Run single test, emit NDJSON events
suite.exe --run-all --timeout=20000  → Per-test timeout in ms
```

**Dispatcher generated code structure:**
```
main():
  parse_args()
  if --list:
    emit test metadata as JSON array
    exit(0)

  for each test (or single test if --test-index):
    emit {"event":"test_start",...}

    // Per-test isolation:
    // Windows: __try/__except wrapping test call
    // Unix: sigaction + sigaltstack for SIGSEGV/SIGBUS/SIGFPE/SIGILL/SIGABRT

    start_timer(timeout_ms)
    result = call_test_function()
    stop_timer()

    if crashed:
      emit {"event":"test_crash",...}
    elif timed_out:
      emit {"event":"test_timeout",...}
    elif failed:
      emit {"event":"test_fail",...}
    else:
      emit {"event":"test_pass",...}

  if coverage_enabled:
    emit {"event":"coverage",...}

  emit {"event":"suite_end",...}
```

**Per-test timeout in dispatcher:**
- Windows: Timer thread with `WaitForSingleObject(event, timeout_ms)`. On timeout, set flag, test function checks flag at safe points.
- Unix: `setitimer(ITIMER_REAL, ...)` + `SIGALRM` handler sets flag. Test function checks flag.
- If test doesn't check flag (infinite loop): suite-level timeout in coordinator kills the subprocess.

### 3.7 Process Management (Cross-Platform)

**Process struct:**
```cpp
struct Process {
    // Platform handles
#ifdef _WIN32
    HANDLE process;
    HANDLE stdout_pipe;
    HANDLE stderr_pipe;
    HANDLE job_object;      // For descendant process termination
#else
    pid_t pid;
    int stdout_fd;
    int stderr_fd;
    pid_t pgid;             // Process group for descendant termination
#endif

    std::string exe_path;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::milliseconds timeout;

    // API
    static Process launch(const std::string& exe, const std::vector<std::string>& args,
                         const std::map<std::string, std::string>& env);
    bool is_done();                    // Non-blocking check
    int wait(std::chrono::milliseconds timeout);  // Blocking wait with timeout
    void kill();                       // Force termination
    std::string read_stdout();         // Drain stdout pipe
    std::string read_stderr();         // Drain stderr pipe
};
```

**Windows implementation:**
- `CreateProcessW()` with `CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED`
- `CreateJobObject()` + `AssignProcessToJobObject()` — ensures all descendant processes are killed on termination (nextest pattern)
- `CreatePipe()` with `SECURITY_ATTRIBUTES.bInheritHandle = TRUE` for stdout/stderr
- `WaitForSingleObject(process, timeout_ms)` for blocking wait
- `TerminateJobObject()` on timeout (kills entire process tree)
- Non-blocking read: `PeekNamedPipe()` then `ReadFile()` with bounded buffer

**Unix implementation:**
- `fork()` + `execvp()` (NOT `fork()` alone — avoids multi-thread deadlock)
- `setpgid(0, 0)` in child — create new process group (nextest pattern)
- `pipe()` for stdout/stderr, `dup2()` in child to redirect
- `waitpid(pid, &status, WNOHANG)` for non-blocking check
- SIGTERM to process group (`kill(-pgid, SIGTERM)`), wait grace period, then SIGKILL (`kill(-pgid, SIGKILL)`)
- Non-blocking read: `poll()` on stdout_fd/stderr_fd with timeout

**Deadlock prevention:**
- Stdout and stderr pipes read concurrently (separate thread or `poll`/`select`)
- If only one pipe is drained, the other may fill its OS buffer (default 64KB) and block the child
- Solution: use `poll()` to multiplex both pipes, read whichever has data

### 3.8 File Structure

```
compiler/src/testing/          <-- NEW directory (sibling to cli/)
  coordinator.cpp/.hpp         - Main orchestration: compile pool → exec pool → result aggregation
  discovery.cpp/.hpp           - Test file discovery, suite grouping, test counting
  compiler.cpp/.hpp            - Suite compilation to EXE (reuses existing codegen pipeline)
  process.cpp/.hpp             - Cross-platform subprocess: launch, pipes, timeout, kill
  cache.cpp/.hpp               - Dual-ID cache with SHA-256, environment tracking
  coverage.cpp/.hpp            - Codegen-based coverage: collect from files, aggregate, diff
  reporter.cpp/.hpp            - Event listener interface + Terminal/JSON/JUnit reporters
  dispatcher_gen.cpp/.hpp      - Dispatcher IR generation (JSON protocol, SEH/signals, timeout)
  benchmark.cpp/.hpp           - Benchmark runner with statistical analysis
  fuzzer.cpp/.hpp              - Fuzz runner with corpus management
  diagnostic.cpp/.hpp          - Diagnostic test runner (@expect-error verification)
```

**Cross-platform requirements per file:**

| File | Windows | macOS/Linux | Platform Code |
|------|---------|-------------|---------------|
| process.cpp | CreateProcess, Job objects, Named pipes, WaitForSingleObject | fork+execvp, Process groups, pipe(), waitpid+poll | ~300 lines |
| dispatcher_gen.cpp | SEH `__try/__except` | `sigaction` + `sigaltstack` | ~50 lines |
| reporter.cpp | `SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING)` | ANSI native | ~10 lines |
| All other files | Standard C++17 | Standard C++17 | 0 lines |

Only `process.cpp` has significant platform-specific code. Everything else is portable.

### 3.9 Reporter System (Catch2-Inspired)

```cpp
// Abstract event listener interface
class ITestReporter {
public:
    virtual ~ITestReporter() = default;

    // Test run lifecycle
    virtual void on_run_start(const TestRunConfig& config) = 0;
    virtual void on_run_end(const TestRunResult& result) = 0;

    // Suite lifecycle
    virtual void on_suite_compile_start(const std::string& suite_name) = 0;
    virtual void on_suite_compile_end(const std::string& suite_name, bool success, Duration elapsed) = 0;
    virtual void on_suite_start(const std::string& suite_name, size_t test_count) = 0;
    virtual void on_suite_end(const SuiteResult& result) = 0;

    // Test lifecycle
    virtual void on_test_start(const TestInfo& test) = 0;
    virtual void on_test_pass(const TestInfo& test, Duration elapsed) = 0;
    virtual void on_test_fail(const TestInfo& test, const std::string& error, Duration elapsed) = 0;
    virtual void on_test_crash(const TestInfo& test, const std::string& signal) = 0;
    virtual void on_test_timeout(const TestInfo& test, Duration timeout) = 0;
    virtual void on_test_skip(const TestInfo& test, const std::string& reason) = 0;

    // Coverage
    virtual void on_coverage_report(const CoverageReport& report) = 0;
};

// MultiReporter broadcasts to all registered reporters simultaneously
class MultiReporter : public ITestReporter { /* ... */ };
```

**Built-in reporters:**
- `TerminalReporter` — Colored vitest-style output with progress, summary, profile stats
- `JsonReporter` — NDJSON to file (one JSON object per event)
- `JunitXmlReporter` — JUnit XML for CI integration (GitHub Actions, Jenkins, etc.)
- `CoverageHtmlReporter` — Function coverage HTML report
- `CoverageJsonReporter` — Function coverage JSON (`build/coverage/coverage.json`)

### 3.10 Files DELETED After Switchover

The entire `compiler/src/cli/tester/` directory (25 files, 15,464 lines):

```
DELETE: test_runner.cpp (1728), test_runner.hpp (228), test_runner_exec.cpp (982)
DELETE: test_runner_single.cpp (729), test_runner_internal.hpp (141)
DELETE: suite_execution.cpp (1374), exe_suite_runner.cpp (827)
DELETE: exe_test_runner.cpp (1216), exe_test_runner.hpp (132)
DELETE: exe_test_execution.cpp (679), tester_run.cpp (764)
DELETE: tester_internal.hpp (318), tester_helpers.cpp (165)
DELETE: execution.cpp (282), library_coverage_report.cpp (1457)
DELETE: output.cpp (340), coverage.cpp (683), coverage.hpp (238)
DELETE: library_coverage.cpp (618), test_cache.cpp (831)
DELETE: tester_discovery.cpp (279), tester_helpers.cpp (165)
DELETE: benchmark.cpp (331), fuzzer.cpp (498), diagnostic_execution.cpp (403)
DELETE: exe_dispatcher_gen.cpp (221)
```

Also deleted: `lib/test/runtime/coverage.c` (676 lines) — replaced by simple file-write in dispatcher.

### 3.11 Key Engineering Decisions (with Justification)

| Decision | Chosen Approach | Alternative Considered | Why |
|----------|----------------|----------------------|-----|
| Execution model | Subprocess-only (EXE) | DLL in-process | Eliminates entire class of bugs: crash isolation, output capture, coverage hangs, TLS conflicts. 5ms overhead per suite is negligible. |
| Cache system | Dual-ID SHA-256 (Go model) | CRC32C single-hash (current) | SHA-256 is collision-resistant. Dual-ID catches identical binaries from different source paths. Environment tracking prevents false cache hits. |
| Coverage | Codegen `tml_cover_func()` + file write | LLVM profiling runtime (profraw) | Current LLVM profiling causes hangs. Codegen coverage is Go-inspired, zero external dependency, works in subprocess model. |
| Crash handling | SEH (Win) + signals (Unix) in dispatcher | Process-level only | Per-test crash isolation within a suite EXE. Crash one test, report it, continue to next. Coordinator has suite-level timeout as safety net. |
| Reporter | Event listener interface (Catch2) | Direct printf (current) | Clean separation of concerns. Multiple simultaneous reporters. Easy to add new formats. |
| Parallelism | Thread pool for compilation, async subprocess for execution | Current 3-stage pipeline | Simpler: compile workers feed directly into subprocess launch. No bridge thread. No serialization point. |
| Timeout | Per-test in dispatcher + per-suite in coordinator | Watchdog thread per DLL (current) | Current watchdog kills entire process. New design: dispatcher handles per-test timeout internally, coordinator kills only the subprocess if dispatcher hangs. |
| Protocol | NDJSON (Go's test2json) | Custom text parsing (current) | Machine-parseable, self-contained per line, streaming-compatible, no regex required. |
| Sharding | `TML_SHARD_INDEX`/`TML_TOTAL_SHARDS` env vars (gtest model) | None | Simple round-robin. Easy to implement. Enables CI parallelism. |
| Test result caching | Cache passing tests only (Go model) | Cache all results | Failing tests may depend on external state. Always re-run failures. |

---

## 4. Migration Strategy

1. Build new system in `compiler/src/testing/` (zero changes to old code)
2. Wire into `cmd_test.cpp` with internal development flag (`--new-test-runner`)
3. Validate: run full suite with BOTH systems, compare pass/fail counts exactly
4. Validate: run coverage with BOTH systems, compare function coverage percentages
5. Run cross-platform validation (Windows primary + Linux + macOS)
6. Once 100% validated: delete entire `compiler/src/cli/tester/` directory
7. Delete `lib/test/runtime/coverage.c` (replaced by dispatcher file-write)
8. Update CMakeLists.txt, CLAUDE.md, docs/specs/09-CLI.md, docs/specs/10-TESTING.md

---

## 5. Success Criteria

1. All existing tests pass (zero regressions)
2. Coverage mode works without hangs
3. Cache invalidation correct (no false hits, no false misses)
4. Crash isolation works (subprocess model, per-test SEH/signals)
5. Per-test timeouts work (hung test emits timeout event, other tests continue)
6. Code: 15,464 → ~7,000 lines (55% reduction)
7. JSON output available (`--output=json` produces valid NDJSON)
8. JUnit XML output available (`--output=junit` produces valid XML)
9. Cross-platform: Windows + Linux + macOS
10. Performance within 10% of current (compilation + execution)
11. `tml test --coverage` completes without hangs, under 3 minutes
12. Old system completely removed (no dead code, no fallback flags)
13. Cache system uses SHA-256 (not CRC32C), tracks dependencies + compiler version + flags
14. Multi-reporter support (console + file simultaneously)
