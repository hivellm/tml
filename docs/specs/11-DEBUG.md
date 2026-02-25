# TML v1.0 — Debug System

## 1. Error Messages

### 1.1 Standard Format

```
error[E001]: type mismatch
  --> src/main.tml:42:15
   |
42 |     let x: String = 42
   |            ^^^^^^   ^^ expected String, found I32
   |            |
   |            expected due to this type annotation
   |
   = help: use `42.to_string()` to convert I32 to String
```

### 1.2 Elements

| Element | Description |
|---------|-------------|
| `error[E001]` | Level + code |
| `-->` | Location (file:line:column) |
| Snippet | Relevant code with markers |
| `= help:` | Fix suggestion |
| `= note:` | Additional information |

### 1.3 Levels

```
error    Compilation error (blocks build)
warning  Potential problem (doesn't block)
info     Information
note     Additional context
help     Correction suggestion
```

## 2. Format for LLMs

### 2.1 Structured JSON

```bash
tml check --message-format json
```

```json
{
  "reason": "compiler-message",
  "message": {
    "code": "E001",
    "level": "error",
    "message": "type mismatch",
    "file": "src/main.tml",
    "line": 42,
    "column": 15,
    "stable_id": "@let_x",
    "spans": [
      {
        "start": {"line": 42, "column": 15},
        "end": {"line": 42, "column": 21},
        "label": "expected String"
      },
      {
        "start": {"line": 42, "column": 24},
        "end": {"line": 42, "column": 26},
        "label": "found I32"
      }
    ],
    "expected": "String",
    "found": "I32",
    "suggestions": [
      {
        "message": "convert I32 to String",
        "replacement": "42.to_string()",
        "span": {"line": 42, "column": 24, "end_column": 26}
      }
    ]
  }
}
```

### 2.2 Important Fields for LLMs

| Field | Usage |
|-------|-------|
| `stable_id` | Stable reference by @id |
| `suggestions` | Applicable patches |
| `expected`/`found` | Types for analysis |
| `spans` | Precise locations |

## 3. Interactive Debugger

### 3.1 Start Debugger

```bash
tml debug src/main.tml

# Or attach to process
tml debug --pid 12345
```

### 3.2 Commands

```
(tml-debug) help
Commands:
  break <loc>    Set breakpoint at location
  continue       Continue execution
  step           Step over
  stepi          Step into
  stepo          Step out
  print <expr>   Print expression value
  locals         Show local variables
  backtrace      Show call stack
  watch <expr>   Watch expression
  quit           Exit debugger

Locations:
  src/main.tml:42     Line in file
  @a1b2c3d4           Stable ID
  func_name           Function name
```

### 3.3 Session Example

```
(tml-debug) break src/main.tml:42
Breakpoint 1 at src/main.tml:42

(tml-debug) continue
Hit breakpoint 1 at src/main.tml:42

   40 | func process(data: List[I32]) -> I32 {
   41 |     var total: I32 = 0
-> 42 |     loop item in data {
   43 |         total += item
   44 |     }

(tml-debug) print data
List[I32] = [1, 2, 3, 4, 5]

(tml-debug) print data.len()
U64 = 5

(tml-debug) locals
data: List[I32] = [1, 2, 3, 4, 5]
total: I32 = 0

(tml-debug) step
   43 |         total += item

(tml-debug) print item
I32 = 1

(tml-debug) watch total
Watchpoint 1: total

(tml-debug) continue
Watchpoint 1 hit: total changed from 0 to 1
```

## 4. Logging

### 4.1 Log Functions

```tml
use std::log::*

func process() {
    log_trace("entering process")
    log_debug("data: " + data.to_string())
    log_info("processing started")
    log_warn("deprecated API used")
    log_error("failed to connect")
}
```

### 4.2 Levels

```
trace   Most verbose, execution details
debug   Debug information
info    General information
warn    Warnings
error   Errors (non-fatal)
```

### 4.3 Configuration

```bash
TML_LOG=debug tml run

# Filter by module
TML_LOG=myapp=debug,http=warn tml run
```

```toml
# tml.toml
[log]
level = "info"
format = "pretty"  # or "json"
```

## 5. Memory Leak Detection

### 5.1 Runtime Tracking

TML includes built-in memory leak detection for debug builds:

```bash
# Leak checking is enabled by default in debug builds
tml build myapp.tml
./myapp  # Reports leaks at exit

# Disable leak checking
tml build myapp.tml --no-check-leaks

# Release builds have leak checking disabled automatically
tml build myapp.tml --release
```

### 5.2 Leak Report Format

When a program exits with unfreed allocations:

```
================================================================================
                         TML MEMORY LEAK REPORT
================================================================================

Detected 2 unfreed allocation(s) totaling 256 bytes:

  Leak #1:
    Address:  0x7f8a12340000
    Size:     128 bytes
    Alloc ID: 1
    Tag:      mem_alloc

  Leak #2:
    Address:  0x7f8a12340100
    Size:     128 bytes
    Alloc ID: 3
    Tag:      mem_alloc

================================================================================
Summary: 2 leak(s), 256 bytes lost
================================================================================

[TML Memory] Program exited with 2 memory leak(s)
```

### 5.3 Tracking API

For advanced use cases, the tracking API is available in C:

```c
#include "mem_track.h"

// Get current statistics
TmlMemStats stats;
tml_mem_get_stats(&stats);
printf("Current allocations: %llu\n", stats.current_allocations);
printf("Peak allocations: %llu\n", stats.peak_allocations);
printf("Peak bytes: %llu\n", stats.peak_bytes);

// Print full statistics
tml_mem_print_stats();

// Manual leak check (returns count)
int leaks = tml_mem_check_leaks();

// Disable automatic check at exit
tml_mem_set_check_at_exit(0);
```

### 5.4 Test Runner Integration

```bash
# Run tests with leak checking (default)
tml test

# Disable leak checking for tests
tml test --no-check-leaks
```

## 6. Profiling

### 6.1 CPU Profiler

```bash
tml run --profile cpu

# Generates: target/profile/cpu.json
```

### 6.2 Memory Profiler

```bash
tml run --profile memory

# Tracks allocations
```

### 6.3 Visualization

```bash
tml profile view target/profile/cpu.json
# Opens visualization in browser

tml profile flamegraph target/profile/cpu.json
# Generates flamegraph
```

### 6.4 Inline Profiling

```tml
use std::time::Instant

func benchmark_operation() {
    let start: Instant = Instant.now()

    expensive_operation()

    let elapsed: Duration = start.elapsed()
    log_info("operation took: " + elapsed.to_string())
}

// Or with directive helper
func with_timing() {
    @profile("my_operation")
    {
        expensive_operation()
    }
}
```

## 7. Runtime Assertions

### 7.1 Debug Assertions

```tml
func process(x: I32) {
    debug_assert(x >= 0, "x must be non-negative")
    // removed in release builds
}
```

### 7.2 Runtime Checks

```tml
func divide(a: I32, b: I32) -> I32 {
    if b == 0 then panic("division by zero")
    return a / b
}

// Overflow checking (debug mode)
let x: I32 = I32.MAX
let y: I32 = x + 1  // panic in debug, wraps in release
```

## 8. Panic Handling

### 8.1 Panic with Message

```tml
panic("something went wrong")
panic("invalid state: " + state.to_string())
```

### 8.2 Panic Hook

```tml
use std::panic

func main() {
    panic.set_hook(do(info) {
        log_error("PANIC: " + info.message)
        log_error("Location: " + info.location.to_string())
        log_error("Backtrace:\n" + info.backtrace.to_string())
    })
}
```

### 8.3 Catch Panic (Low-Level)

```tml
use std::panic

let result: Outcome[T, PanicInfo] = panic.catch(do() {
    risky_operation()
})

when result {
    Ok(value) -> use(value),
    Err(panic_info) -> recover(panic_info),
}
```

## 9. Backtraces

### 9.1 Enabling

```bash
TML_BACKTRACE=1 tml run
TML_BACKTRACE=full tml run  # with all frames
```

### 9.2 Format

```
thread 'main' panicked at 'index out of bounds'
stack backtrace:
   0: myapp::process@a1b2c3d4
             at src/lib.tml:42
   1: myapp::main@b2c3d4e5
             at src/main.tml:15
   2: std::rt::lang_start
```

## 10. Inspect Tools

### 10.1 Type Info

```tml
use std::debug

let x: I32 = get_value()
print(debug.type_name(x))  // "Maybe[I32]"
print(debug.size_of(x))    // 8
```

### 10.2 Debug Behavior

```tml
behavior Debug {
    func debug(this) -> String
}

@auto(debug)
type Point { x: F64, y: F64 }

let p: Point = Point { x: 1.0, y: 2.0 }
print(p.debug())  // "Point { x: 1.0, y: 2.0 }"
```

### 10.3 Dbg Helper

```tml
let x: T = dbg(expensive_computation())
// Prints: [src/main.tml:42] expensive_computation() = 42
// Returns the value to continue pipeline
```

## 11. IDE Integration

### 11.1 Inline Diagnostics

The LSP sends diagnostics in real-time:

```json
{
  "uri": "file:///src/main.tml",
  "diagnostics": [
    {
      "range": {"start": {"line": 41, "character": 14}, "end": {"line": 41, "character": 20}},
      "severity": 1,
      "code": "E001",
      "source": "tml",
      "message": "type mismatch: expected String, found I32",
      "relatedInformation": [...],
      "data": {"stable_id": "@let_x", "suggestions": [...]}
    }
  ]
}
```

### 11.2 Code Actions

```json
{
  "title": "Convert to String",
  "kind": "quickfix",
  "edit": {
    "changes": {
      "file:///src/main.tml": [
        {
          "range": {"start": {"line": 41, "character": 24}},
          "newText": "42.to_string()"
        }
      ]
    }
  }
}
```

## 12. External Memory Tools

### 12.1 AddressSanitizer (Recommended)

AddressSanitizer (ASan) is the recommended memory debugging tool on all platforms:

```bash
# Build with AddressSanitizer
tml build myapp.tml --sanitize=address

# Run - crashes on memory errors with detailed stack traces
./myapp
```

ASan detects:
- Buffer overflows (stack, heap, global)
- Use-after-free
- Double-free
- Memory leaks (with LeakSanitizer)

### 12.2 Valgrind (Linux)

Valgrind provides comprehensive memory debugging on Linux:

```bash
# Install Valgrind (Ubuntu/Debian)
sudo apt install valgrind

# Run with memcheck (memory errors)
valgrind --leak-check=full ./myapp

# Run with detailed leak origins
valgrind --leak-check=full --track-origins=yes ./myapp

# Generate suppression file for false positives
valgrind --gen-suppressions=all ./myapp 2>&1 | grep -A4 "^{"
```

Common Valgrind options:
| Option | Description |
|--------|-------------|
| `--leak-check=full` | Full leak checking |
| `--show-leak-kinds=all` | Show all leak types |
| `--track-origins=yes` | Track uninitialized values |
| `--suppressions=file.supp` | Use suppression file |

### 12.3 Windows Memory Tools

On Windows, use Visual Studio's diagnostics or Dr. Memory:

```bash
# Dr. Memory (open source)
drmemory -- ./myapp.exe

# Visual Studio: Debug → Windows → Memory Diagnostic
```

### 12.4 Sanitizer Build Options

The TML compiler supports sanitizer flags:

```bash
# AddressSanitizer (memory errors + leaks)
tml build --sanitize=address

# UndefinedBehaviorSanitizer (UB detection)
tml build --sanitize=undefined

# MemorySanitizer (uninitialized reads, Linux only)
tml build --sanitize=memory

# ThreadSanitizer (data races)
tml build --sanitize=thread

# Combine multiple sanitizers
tml build --sanitize=address,undefined
```

---

*Previous: [10-TESTING.md](./10-TESTING.md)*
*Next: [12-ERRORS.md](./12-ERRORS.md) — Error Catalog*
