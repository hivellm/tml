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
   41 |     var total = 0
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
import std.log.*

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

## 5. Profiling

### 5.1 CPU Profiler

```bash
tml run --profile cpu

# Generates: target/profile/cpu.json
```

### 5.2 Memory Profiler

```bash
tml run --profile memory

# Tracks allocations
```

### 5.3 Visualization

```bash
tml profile view target/profile/cpu.json
# Opens visualization in browser

tml profile flamegraph target/profile/cpu.json
# Generates flamegraph
```

### 5.4 Inline Profiling

```tml
import std.time.Instant

func benchmark_operation() {
    let start = Instant.now()

    expensive_operation()

    let elapsed = start.elapsed()
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

## 6. Runtime Assertions

### 6.1 Debug Assertions

```tml
func process(x: I32) {
    debug_assert(x >= 0, "x must be non-negative")
    // removed in release builds
}
```

### 6.2 Runtime Checks

```tml
func divide(a: I32, b: I32) -> I32 {
    if b == 0 then panic("division by zero")
    return a / b
}

// Overflow checking (debug mode)
let x: I32 = I32.MAX
let y = x + 1  // panic in debug, wraps in release
```

## 7. Panic Handling

### 7.1 Panic with Message

```tml
panic("something went wrong")
panic("invalid state: " + state.to_string())
```

### 7.2 Panic Hook

```tml
import std.panic

func main() {
    panic.set_hook(do(info) {
        log_error("PANIC: " + info.message)
        log_error("Location: " + info.location.to_string())
        log_error("Backtrace:\n" + info.backtrace.to_string())
    })
}
```

### 7.3 Catch Panic (Low-Level)

```tml
import std.panic

let result = panic.catch(do() {
    risky_operation()
})

when result {
    Success(value) -> use(value),
    Failure(panic_info) -> recover(panic_info),
}
```

## 8. Backtraces

### 8.1 Enabling

```bash
TML_BACKTRACE=1 tml run
TML_BACKTRACE=full tml run  # with all frames
```

### 8.2 Format

```
thread 'main' panicked at 'index out of bounds'
stack backtrace:
   0: myapp::process@a1b2c3d4
             at src/lib.tml:42
   1: myapp::main@b2c3d4e5
             at src/main.tml:15
   2: std::rt::lang_start
```

## 9. Inspect Tools

### 9.1 Type Info

```tml
import std.debug

let x = get_value()
print(debug.type_name(x))  // "Maybe[I32]"
print(debug.size_of(x))    // 8
```

### 9.2 Debug Behavior

```tml
behavior Debug {
    func debug(this) -> String
}

@auto(debug)
type Point { x: F64, y: F64 }

let p = Point { x: 1.0, y: 2.0 }
print(p.debug())  // "Point { x: 1.0, y: 2.0 }"
```

### 9.3 Dbg Helper

```tml
let x = dbg(expensive_computation())
// Prints: [src/main.tml:42] expensive_computation() = 42
// Returns the value to continue pipeline
```

## 10. IDE Integration

### 10.1 Inline Diagnostics

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

### 10.2 Code Actions

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

---

*Previous: [10-TESTING.md](./10-TESTING.md)*
*Next: [12-ERRORS.md](./12-ERRORS.md) — Error Catalog*
