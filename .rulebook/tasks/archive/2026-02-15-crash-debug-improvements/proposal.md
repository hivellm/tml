# Proposal: Crash Debug Improvements

## Status: APPROVED

## Why

The TML test runner currently catches hardware exceptions (ACCESS_VIOLATION, STACK_OVERFLOW, etc.) via a Vectored Exception Handler (VEH) in `essential.c` and recovers via `longjmp`. This was implemented to prevent silent test crashes from killing the entire test suite without diagnostics.

However, the current crash reporting is **minimal** — it prints the exception name and test context, but omits critical debugging information:

- **No fault address**: An ACCESS_VIOLATION doesn't tell you *what* address was accessed (null? freed memory? random garbage?)
- **No read/write distinction**: Was it a null pointer *read* (usually safe to recover from) or a *write* (likely memory corruption)?
- **No instruction pointer (RIP)**: You can't tell *where* in the code the crash occurred
- **No register dump**: RSP/RBP values help identify stack corruption instantly
- **No stack frames**: No backtrace means you're debugging blind
- **STACK_OVERFLOW is unsafe**: After stack overflow, the guard page is consumed. Without `_resetstkoflw()`, the next test can crash again without any exception. This is a ticking time bomb.
- **No crash severity policy**: All crashes are treated equally — a null deref (usually harmless state-wise) is recovered from the same way as a heap corruption or write-to-freed-memory (definitely corrupted state)

### Real-World Impact

In a recent debugging session, a test crash produced only:
```
CRASH: ACCESS_VIOLATION (0xC0000005)
  Test:  test_deflate_inflate_roundtrip
  File:  lib/std/tests/zlib/zlib_stream.test.tml
```

This tells you *which* test crashed, but not:
- Was it a null pointer? (addr 0x0 = easy fix, probably a missing null check)
- Was it use-after-free? (addr 0x7FF... = harder, need to trace lifetime)
- Was it a write? (state may be corrupted, remaining tests unreliable)
- Where in the function did it crash? (without RIP, you're guessing)

The lack of this information cost ~30 minutes of manual debugging that could have been resolved in seconds with proper crash diagnostics.

### Current Architecture

| Layer | File | Role |
|-------|------|------|
| VEH handler | `essential.c:662` | First responder — catches exception, formats message, does `longjmp` |
| SEH fallback | `test_runner_exec.cpp:98-122` | Belt-and-suspenders `__try/__except` wrapper |
| Global filter | `run.cpp:84-163` | `SetUnhandledExceptionFilter` for unrecoverable crashes, writes `crash_report.txt` |
| Crash context | `essential.c:155-181` | `tml_set_test_crash_context()` / `tml_clear_test_crash_context()` — sets test/file/suite before each test |
| Recovery | `essential.c:765-822` | `tml_run_test_with_catch()` — `setjmp`/`longjmp` with VEH for crash catching |

### Code Duplication

The exception name lookup is duplicated 3 times:
- `essential.c:611` — `tml_get_exception_name()`
- `test_runner_exec.cpp:43` — `get_exception_name()`
- `run.cpp:88-113` — inline `switch(code)`

## What Changes

### Phase 1: Rich Crash Diagnostics (essential.c VEH handler)

Enhance the VEH handler to extract and report all available information from the `EXCEPTION_POINTERS` struct. This data is freely available in the exception record and context record but currently ignored.

**1.1 Fault address and access type for ACCESS_VIOLATION**

`ExceptionRecord->ExceptionInformation[0]` = 0 (read), 1 (write), 8 (DEP/execute violation).
`ExceptionRecord->ExceptionInformation[1]` = the faulting address.

Output becomes:
```
CRASH: ACCESS_VIOLATION (0xC0000005)
  Test:    test_my_function
  File:    lib/std/tests/foo.test.tml
  Fault:   0x0000000000000000 (null pointer READ)
  RIP:     0x00007FF6A1234567
```

vs today:
```
CRASH: ACCESS_VIOLATION (0xC0000005)
  Test:  test_my_function
  File:  lib/std/tests/foo.test.tml
```

**1.2 Register dump (RIP, RSP, RBP)**

`info->ContextRecord` has all x64 registers. Printing RIP + RSP + RBP is enough for 90% of crash diagnosis. RIP tells you *where*; RSP/RBP tell you if the stack is sane.

**1.3 Static buffer for STACK_OVERFLOW**

Current handler uses `char msg[1024]` on the stack. After STACK_OVERFLOW, the stack is at its limit — allocating 1KB on it may cause a double fault. Use a pre-allocated static global buffer instead (only for the STACK_OVERFLOW case).

**1.4 `CaptureStackBackTrace` raw frames**

`CaptureStackBackTrace()` is safe to call from VEH (no heap allocation, no symbol loading). Capture up to 32 raw return addresses. These can be resolved to symbols post-mortem using PDB + `addr2line` or by the C++ test runner after recovery.

Do NOT use `SymFromAddr` or `SymGetLineFromAddr64` in the handler — those allocate from the heap and can deadlock if the crash occurred during a heap operation.

### Phase 2: Stack Overflow Safety (essential.c)

**2.1 `_resetstkoflw()` after STACK_OVERFLOW**

After a STACK_OVERFLOW exception, the stack guard page has been consumed. Without restoring it, the next stack overflow will cause an immediate process termination without any exception. `_resetstkoflw()` restores the guard page.

Must be called before `longjmp` since `longjmp` itself needs stack space.

**2.2 Abort-suite flag for dangerous crashes**

Introduce a `tml_crash_severity` global that the VEH handler sets based on crash type:

| Exception | Severity | Recovery |
|-----------|----------|----------|
| ACCESS_VIOLATION read, addr < 0xFFFF | `CRASH_NULL_DEREF` | `longjmp` (safe — no corruption) |
| ACCESS_VIOLATION read, addr >= 0xFFFF | `CRASH_USE_AFTER_FREE` | `longjmp` + set abort-suite flag |
| ACCESS_VIOLATION write | `CRASH_WRITE_VIOLATION` | `longjmp` + set abort-suite flag |
| ACCESS_VIOLATION execute (DEP) | `CRASH_DEP_VIOLATION` | `longjmp` + set abort-suite flag |
| STACK_OVERFLOW | `CRASH_STACK_OVERFLOW` | `_resetstkoflw()` + `longjmp` + set abort-suite flag |
| HEAP_CORRUPTION (0xC0000374) | `CRASH_HEAP_CORRUPTION` | `longjmp` + set abort-suite flag |
| INT_DIVIDE_BY_ZERO | `CRASH_ARITHMETIC` | `longjmp` (safe) |
| Everything else | `CRASH_UNKNOWN` | `longjmp` + set abort-suite flag |

The C++ test runner reads `tml_crash_severity` after recovery and decides whether to continue the remaining tests in the suite or skip them:
- `CRASH_NULL_DEREF` / `CRASH_ARITHMETIC` → continue (low risk)
- Everything else → skip remaining tests in this DLL, report as "skipped due to prior crash"

### Phase 3: C++ Test Runner Integration (test_runner_exec.cpp)

**3.1 Read crash severity from DLL**

After a crash recovery (exit_code == -2), look up `tml_crash_severity` from the DLL and include it in the test result. The suite executor can then decide to abort.

**3.2 Post-mortem symbol resolution**

After crash recovery, the C++ test runner is in a safe context (heap is usable, no signal handler active). It can:
- Look up `tml_crash_backtrace_frames` from the DLL
- Use `SymFromAddr` / `SymGetLineFromAddr64` to resolve addresses to function names + line numbers
- Include resolved backtrace in the test failure output

This is the correct split: raw capture in the handler (no alloc), symbol resolution after recovery (safe context).

**3.3 Enhanced error message format**

Current: `Test crashed: CRASH: ACCESS_VIOLATION (0xC0000005) in test "test_foo" [file.tml]`

Proposed:
```
Test crashed: ACCESS_VIOLATION (null pointer READ at 0x0000000000000000)
  Location: 0x00007FF6A1234567 (RIP)
  Stack:    0x000000D830BFEF00 (RSP)
  Backtrace:
    [0] 0x00007FF6A1234567  test_foo + 0x42  (file.test.tml)
    [1] 0x00007FF6A1230000  tml_test_0 + 0x10
    [2] 0x00007FF6A1001234  tml_run_test_with_catch + 0x8A
```

### Phase 4: Code Deduplication

**4.1 Single exception name function**

Remove the duplicated `get_exception_name` from `test_runner_exec.cpp` and the inline switch from `run.cpp`. Both should call the exported `tml_get_exception_name` from `essential.c` (already exists, just not used by the C++ side).

Add new exception codes that `run.cpp` already handles but `essential.c` doesn't:
- `0xC0000028` — `STATUS_BAD_STACK`
- `0xC0000374` — `HEAP_CORRUPTION`
- `0xC0000409` — `STACK_BUFFER_OVERRUN`

## Impact

- Affected code:
  - `compiler/runtime/core/essential.c` — VEH handler enhancement, static buffers, `_resetstkoflw()`, crash severity
  - `compiler/src/cli/tester/test_runner_exec.cpp` — read crash severity, post-mortem symbol resolution, enhanced error format
  - `compiler/src/cli/tester/suite_execution.cpp` — abort-suite logic on dangerous crashes
  - `compiler/src/cli/tester/run.cpp` — remove duplicated exception name lookup
- Breaking change: NO
- User benefit: Crash messages go from "test X crashed with ACCESS_VIOLATION" to a full diagnostic with fault address, access type, instruction pointer, registers, and backtrace

## Dependencies

- None. All changes are within existing files and use existing Windows APIs.
- `CaptureStackBackTrace` — available since Windows XP, no additional libraries needed.
- `_resetstkoflw` — available in MSVC CRT (`<malloc.h>`), no additional libraries needed.
- `SymFromAddr` / `SymGetLineFromAddr64` — available via `DbgHelp.h`, already used by `backtrace.c`.

## Success Criteria

1. ACCESS_VIOLATION crash message includes fault address (read/write/execute) and RIP
2. STACK_OVERFLOW crash properly resets guard page via `_resetstkoflw()` and test suite continues safely
3. Dangerous crashes (write AV, use-after-free, stack overflow) abort the current suite but allow other suites to run
4. Raw stack frames captured in VEH handler, resolved to symbols by C++ test runner after recovery
5. Exception name lookup is defined in exactly one place
6. Existing 7791 tests continue to pass (no regression)

## Out of Scope

- **Process-per-suite isolation**: Correct solution for full crash isolation, but requires test runner architecture redesign. Tracked separately.
- **TLS for crash context**: Needed for parallel test execution, not needed until that feature is implemented.
- **MiniDump generation**: Overkill for test crashes; raw backtrace is sufficient.
- **Symbol server integration**: Manual PDB lookup is sufficient for now.
- **Unix signal handler improvements**: This task focuses on Windows VEH. Unix signal handlers can be improved in a follow-up task.
