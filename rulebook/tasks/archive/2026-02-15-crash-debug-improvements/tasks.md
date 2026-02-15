# Tasks: Crash Debug Improvements

**Status**: Complete (100%)

## Phase 1: Rich Crash Diagnostics (essential.c VEH handler)

- [x] 1.1.1 Extract fault address from `ExceptionInformation[1]` for ACCESS_VIOLATION
- [x] 1.1.2 Extract access type (read=0/write=1/execute=8) from `ExceptionInformation[0]`
- [x] 1.1.3 Format fault info as "null pointer READ at 0x..." or "write to 0x..."
- [x] 1.1.4 Add RIP from `info->ContextRecord->Rip` to crash message
- [x] 1.1.5 Add RSP and RBP from `ContextRecord` to crash message
- [x] 1.1.6 Use static global buffer for STACK_OVERFLOW message (not stack-allocated `char msg[1024]`)
- [x] 1.1.7 Add `CaptureStackBackTrace()` call — capture up to 32 raw frame addresses
- [x] 1.1.8 Store raw frames in static global `tml_crash_backtrace_frames[32]` for C++ retrieval
- [x] 1.1.9 Export `tml_get_crash_backtrace` function for C++ test runner

## Phase 2: Stack Overflow Safety

- [x] 2.1.1 Add `#include <malloc.h>` for `_resetstkoflw()` declaration
- [x] 2.1.2 Call `_resetstkoflw()` in VEH handler before `longjmp` when `code == EXCEPTION_STACK_OVERFLOW`
- [x] 2.1.3 Define crash severity enum: `CRASH_NULL_DEREF`, `CRASH_USE_AFTER_FREE`, `CRASH_WRITE_VIOLATION`, `CRASH_DEP_VIOLATION`, `CRASH_STACK_OVERFLOW`, `CRASH_HEAP_CORRUPTION`, `CRASH_ARITHMETIC`, `CRASH_UNKNOWN`
- [x] 2.1.4 Set `tml_crash_severity` in VEH handler based on exception type and fault address
- [x] 2.1.5 Export `tml_get_crash_severity()` function for C++ test runner
- [x] 2.1.6 Write deliberate STACK_OVERFLOW test (`crash_stackoverflow.test.tml`) — VEH catches, `_resetstkoflw()` called, diagnostics printed

## Phase 3: C++ Test Runner Integration

- [x] 3.1.1 Read `tml_crash_severity` from DLL after crash recovery (exit_code == -2)
- [x] 3.1.2 Add abort-suite logic in `suite_execution.cpp`: skip remaining tests if severity is dangerous
- [x] 3.1.3 Report skipped tests as "skipped due to prior crash in suite"
- [x] 3.1.4 Read `tml_crash_backtrace_frames` from DLL after recovery
- [x] 3.1.5 Backtrace addresses shown as hex in crash output (DbgHelp SymFromAddr intentionally disabled — hangs in test suites with 300+ DLLs)
- [x] 3.1.6 Backtrace included in test failure error message (hex addresses; full symbol names deferred)
- [x] 3.1.7 Enhanced error format: show fault address, access type, RIP, resolved backtrace

## Phase 4: Code Deduplication

- [x] 4.1.1 Add `STATUS_BAD_STACK`, `HEAP_CORRUPTION`, `STACK_BUFFER_OVERRUN` to `tml_get_exception_name()`
- [x] 4.1.2 Export `tml_get_exception_name` from essential.c DLL
- [x] 4.1.3 Remove `get_exception_name()` from `test_runner_exec.cpp`, call exported function
- [x] 4.1.4 Remove inline switch from `run.cpp:88-113`, call exported function
- [x] 4.1.5 Verify all 3 call sites use the single canonical function

## Phase 5: Verification

- [x] 5.1.1 Verify null deref crash shows fault address 0x0 and "READ" in message
- [x] 5.1.2 Verify STACK_OVERFLOW recovery — VEH catches, `_resetstkoflw()` called, RIP/RSP/RBP printed
- [x] 5.1.3 Verify dangerous crash (write AV) aborts suite — "Aborting suite after WRITE_VIOLATION crash"
- [x] 5.1.4 Verify backtrace frames appear in test failure output
- [x] 5.1.5 Run full test suite — no regressions (only pre-existing crypto/dns crashes)
