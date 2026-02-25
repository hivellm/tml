# Tasks: Parallel Test Execution

**Status**: Pending (0%)

## Phase 1: Infrastructure and Synchronization Primitives

- [ ] 1.1.1 Add TestTask struct to test_runner.hpp with lib_ptr, suite_name, suite_group, dll_path, test_index, test_info fields
- [ ] 1.1.2 Add shared synchronization primitives to suite_execution.cpp: test_task_queue (std::deque), queue_mutex, queue_cv
- [ ] 1.1.3 Add atomic flags: test_exec_done, test_worker_crashed
- [ ] 1.1.4 Add crash tracking: crashed_test_file string

## Phase 2: Worker Thread Implementation

- [ ] 2.1.1 Implement test_exec_worker lambda that dequeues TestTask from queue
- [ ] 2.1.2 Implement task execution: call run_suite_test() for dequeued test_index
- [ ] 2.1.3 Implement crash detection: catch exceptions and set test_worker_crashed flag
- [ ] 2.1.4 Implement result recording: store SuiteTestResult in results_vec indexed by test_index
- [ ] 2.1.5 Implement retry logic: continue to next task or wait for queue signal if empty
- [ ] 2.1.6 Implement graceful exit: check test_exec_done flag and exit when queue empty and done=true

## Phase 3: Main Thread Coordination

- [ ] 3.1.1 Create worker thread pool: instantiate N threads (default 4, configurable via opts.test_threads)
- [ ] 3.1.2 Implement test enqueueing: after DLL compilation, enqueue all test tasks to queue
- [ ] 3.1.3 Implement batch enqueueing: enqueue in batches of 10 tests (for load distribution)
- [ ] 3.1.4 Implement completion detection: wait for all workers via condition_variable::wait()
- [ ] 3.1.5 Implement timeout handling: add timeout for worker completion (prevent infinite hangs)
- [ ] 3.1.6 Implement thread joining: join all worker threads and verify completion

## Phase 4: Crash Detection and Error Reporting

- [ ] 4.1.1 Monitor test_worker_crashed flag during worker wait
- [ ] 4.1.2 If crash detected: retrieve crashed_test_file from atomic string
- [ ] 4.1.3 Print panic message with file path and suite name
- [ ] 4.1.4 Print captured stderr/stdout from crashed test
- [ ] 4.1.5 Exit immediately with non-zero status (don't wait for remaining workers)
- [ ] 4.1.6 Ensure main thread can safely stop remaining worker threads

## Phase 5: Integration and Configuration

- [ ] 5.1.1 Update cmd_test.hpp to honor --test-threads=N for execution thread count
- [ ] 5.1.2 Update suite_execution.cpp to read opts.test_threads and pass to thread pool
- [ ] 5.1.3 Set default to 4 threads if --test-threads not specified
- [ ] 5.1.4 Validate test_threads range: 1-64 (reasonable bounds)
- [ ] 5.1.5 Update help text to clarify --test-threads affects test execution, not just compilation

## Phase 6: Synchronization with Coverage Mode

- [ ] 6.1.1 Ensure LLVM_PROFILE_FILE is set before DLL execution (profile file path)
- [ ] 6.1.2 Coordinate profile merging: collect all profiles after worker threads exit
- [ ] 6.1.3 Merge profiles centrally in main thread (after all tests complete)
- [ ] 6.1.4 Verify coverage.json is generated correctly (no partial/incomplete data)
- [ ] 6.1.5 Test coverage mode with --test-threads=4 (previously hung)

## Phase 7: Testing and Validation

- [ ] 7.1.1 Smoke test: run `tml test --no-cache --test-threads=4` (224 tests should pass)
- [ ] 7.1.2 Parallel verification: check logs show tests executing simultaneously (not sequentially)
- [ ] 7.1.3 Crash handling test: create a test that intentionally crashes and verify error reporting
- [ ] 7.1.4 Performance benchmark: measure test execution time vs single-threaded baseline
- [ ] 7.1.5 Coverage regression test: run with --coverage --test-threads=4 (should not hang)
- [ ] 7.1.6 Thread count variation: test with --test-threads=1,2,4,8 (all should work)
- [ ] 7.1.7 Suite mode verification: ensure --no-suite still works with parallelism
- [ ] 7.1.8 Stress test: run multiple times to detect race conditions (min 5 runs)

## Phase 8: Documentation and Cleanup

- [ ] 8.1.1 Update docs/09-CLI.md: describe --test-threads for test execution parallelism
- [ ] 8.1.2 Update docs/10-TESTING.md: describe thread pool model in testing architecture
- [ ] 8.1.3 Add comments to suite_execution.cpp explaining worker thread pool design
- [ ] 8.1.4 Add comments to TestTask struct describing thread safety assumptions
- [ ] 8.1.5 Update CLAUDE.md if needed (test execution guidelines)
- [ ] 8.1.6 Create memory note: document thread pool implementation details and lessons learned
