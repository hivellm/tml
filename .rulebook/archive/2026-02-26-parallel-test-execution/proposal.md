# Proposal: Parallel Test Execution with Thread Pool

## Status: PROPOSED

## Why

The current test execution architecture processes all tests serially through a single worker thread, even when `--test-threads=N` flag is provided (which only affects compilation thread count, not test execution). This causes:

- Tests take significantly longer than necessary on multi-core systems
- Coverage mode hangs when test execution completes (e.g., at `fmt_helpers_sign.test`)
- No visibility into whether a test execution thread crashes or hangs
- Central logging/coverage/profile data is generated one test at a time, increasing latency
- Coverage instrumentation (LLVM_PROFILE_FILE) with multiple test workers creates merge contention

The user's testing showed that with `--test-threads=8`, tests should appear simultaneously in logs but appeared serially, indicating the current single-threaded execution bottleneck.

## What Changes

### Architecture Overview

Replace the single execute_worker thread model with a thread pool architecture:

- **Main Thread**: Loads DLL, enqueues tests in batches (10 tests per batch), monitors worker health, collects results
- **N Worker Threads**: Each dequeues TestTask from shared queue, executes test, records result, detects crashes

### 1. TestTask Structure

New struct to represent a single test execution task:

```cpp
struct TestTask {
    DynamicLibrary* lib_ptr;          // Loaded DLL
    std::string suite_name;            // Suite identifier
    std::string suite_group;           // For logging
    std::string dll_path;              // For crash reporting
    int test_index;                    // Which test in suite
    const SuiteTestInfo* test_info;    // Test metadata
};
```

### 2. Worker Thread Function

New lambda `test_exec_worker()` that processes tasks from queue:

- Dequeues TestTask from shared queue
- Executes `run_suite_test(lib, test_index, ...)`
- Catches crashes/exceptions and signals crash to main
- Records result in `results_vec[test_index]`
- Continues to next task or exits if queue empty

### 3. Main Thread Changes

In `run_tests_suite_mode()`:

- After DLL compilation, enqueue all tests into `test_task_queue`
- Spin up N worker threads (default 4, configurable via `opts.test_threads`)
- Main thread batches tests: every 10 tests added to queue
- Wait for all workers to finish via condition variable
- Detect if any worker crashed during execution
- Generate centralized logs/coverage/profile data

### 4. Synchronization

- `test_task_queue`: `std::deque<TestTask>` protected by `queue_mutex`
- `queue_cv`: `std::condition_variable` to signal workers when queue has work
- `test_exec_done`: `std::atomic<bool>` to signal workers when no more tasks
- `test_worker_crashed`: `std::atomic<bool>` to detect if any worker crashed
- `crashed_test_file`: `std::string` to record which file caused crash

### 5. Crash Detection and Reporting

If any worker thread crashes/exits unexpectedly:
- Main thread detects via `test_worker_crashed` flag
- Main prints panic message with problematic file path
- Main prints captured stderr/stdout from crashed test
- Main exits with failure code immediately (stops remaining workers)

## Impact

- **Affected specs**: `09-CLI.md` (test execution semantics), `10-TESTING.md` (threading model)
- **Affected code**: `compiler/src/cli/tester/suite_execution.cpp` (primary), test_runner.hpp/.cpp, cmd_test.hpp
- **Breaking change**: NO (--test-threads flag interpretation becomes clearer, no user-facing API changes)
- **User benefit**: Significantly faster test execution, better crash reporting, more reliable coverage mode

## Dependencies

- LLVM coverage instrumentation (LLVM_PROFILE_FILE) must support merging profiles from parallel test runs
- DynamicLibrary must be thread-safe for concurrent test execution from multiple threads
- `run_suite_test()` function must be thread-safe (no shared state modifications)

## Success Criteria

1. ✅ Default test execution uses 4 worker threads (configurable via --test-threads)
2. ✅ Tests appear simultaneously in logs (batched enqueueing, 10 per batch)
3. ✅ Worker thread crash detected, main panics with file and error context
4. ✅ Coverage mode no longer hangs at test execution phase
5. ✅ All 224 tests pass in parallel mode (no race conditions)
6. ✅ Performance improvement: test execution time ≤ 50% of original single-threaded time on 4+ core systems
