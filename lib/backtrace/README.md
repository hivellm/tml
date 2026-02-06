# TML Backtrace Library

Runtime stack trace capture and symbol resolution for debugging TML applications.

**Status**: Complete (Production ready)

## Features

- **Stack capture** - Capture call stack at any point in execution
- **Symbol resolution** - Convert instruction pointers to function names, files, and line numbers
- **Cross-platform** - Windows (DbgHelp) and Unix (dladdr) support
- **Lazy resolution** - Symbols resolved on demand for performance
- **Formatted output** - Pretty-printed stack traces
- **Frame skipping** - Skip internal frames for cleaner output

## Quick Start

Capture and print a backtrace:

```tml
use backtrace::Backtrace

func debug_point() {
    let mut bt: Backtrace = Backtrace::capture()
    bt.resolve()
    print("Stack trace:\n")
    print(bt.to_string())
}
```

Or use the convenience function:

```tml
use backtrace::print_backtrace

func some_function() {
    // ... code ...
    print_backtrace()  // Prints current stack trace
}
```

## Output Format

```
   0: main::inner_function
             at src/main.tml:42
   1: main::outer_function
             at src/main.tml:23
   2: main::main
             at src/main.tml:10
```

## API Reference

### Backtrace

The main type for capturing and formatting stack traces.

```tml
use backtrace::Backtrace

// Capture current stack
let bt: Backtrace = Backtrace::capture()

// Capture, skipping N frames from top
let bt: Backtrace = Backtrace::capture_from(2)

// Get number of frames
let count: I32 = bt.frame_count()

// Resolve symbols (converts IPs to names/files/lines)
bt.resolve()

// Get frames list
let frames: ref List[BacktraceFrame] = bt.frames()

// Format as string
let s: Str = bt.to_string()

// Print to stdout
bt.print()
```

### BacktraceFrame

Represents a single stack frame.

```tml
use backtrace::BacktraceFrame

// Create from instruction pointer
let frame: BacktraceFrame = BacktraceFrame::new(ip)

// Create with symbol info
let frame: BacktraceFrame = BacktraceFrame::with_symbol(ip, symbol)

// Access properties
let ip: *Unit = frame.instruction_pointer()
let resolved: Bool = frame.is_resolved()
let sym: Maybe[BacktraceSymbol] = frame.symbol()

// Format with index
let s: Str = frame.format(0)  // "  0: function_name\n             at file:line"
```

### BacktraceSymbol

Resolved symbol information for a frame.

```tml
use backtrace::BacktraceSymbol

// Create empty symbol
let sym: BacktraceSymbol = BacktraceSymbol::empty()

// Create with name only
let sym: BacktraceSymbol = BacktraceSymbol::with_name("my_function")

// Create with full info
let sym: BacktraceSymbol = BacktraceSymbol::new(
    Just("function_name"),
    Just("src/file.tml"),
    42,  // line number
    0    // column number
)

// Check properties
let has_name: Bool = sym.has_name()
let has_loc: Bool = sym.has_location()

// Format
let s: Str = sym.to_string()  // "function_name at src/file.tml:42"
```

### Convenience Functions

```tml
use backtrace::{print_backtrace, capture_backtrace}

// Capture and print immediately
print_backtrace()

// Capture with skip
let bt: Backtrace = capture_backtrace(2)
```

## Platform Support

### Windows

Uses Windows Debug Help Library (DbgHelp):
- `RtlCaptureStackBackTrace` for stack capture
- `SymFromAddr` for function names
- `SymGetLineFromAddr64` for file/line info
- Thread safety via CriticalSection around symbol operations

**Symbol Requirements**:
- Debug builds (`-g` flag): Full function names, file paths, line numbers
- Release builds: Function names if not stripped
- PDB files: Must be in same directory as executable or in symbol path

**Known Limitations**:
- Maximum 128 frames per capture
- Inline functions may not show separate entries
- Optimized code may have approximate line numbers

### Linux/macOS

Uses standard Unix facilities:
- `backtrace()` from `<execinfo.h>` for stack capture
- `dladdr()` for symbol resolution

**Symbol Requirements**:
- Compile with `-g` for debug info
- DWARF debug info in `.debug_info` sections
- Use `-rdynamic` for exported symbols

**Known Limitations**:
- `dladdr()` only provides function names, not line numbers
- Full resolution requires libbacktrace (future enhancement)

### Frame Filtering

The following frames are automatically filtered from backtrace output:
- Runtime: `panic`, `assert_tml`, `assert_tml_loc`
- Backtrace internals: `backtrace_capture`, `backtrace_resolve`
- System: `longjmp`, `setjmp`, `RaiseException`
- CRT: `__scrt_common_main_seh`, `invoke_main`, `_start`

## Use Cases

### Debugging

```tml
use backtrace::print_backtrace

func complex_algorithm(data: ref List[I32]) {
    // Print stack when entering complex code
    print("Entering complex_algorithm:\n")
    print_backtrace()

    // ... algorithm ...
}
```

### Error Context

```tml
use backtrace::Backtrace

func process_file(path: Str) -> Outcome[Data, Error] {
    let result: Outcome[Data, Error] = parse_file(path)

    when result {
        Err(e) => {
            print("Error parsing file: {e}\n")
            let mut bt: Backtrace = Backtrace::capture()
            bt.resolve()
            print("Stack trace:\n{bt.to_string()}")
            return Err(e)
        },
        Ok(data) => return Ok(data)
    }
}
```

### Test Framework Integration

Backtraces are automatically captured on test assertion failures:

```tml
@test
func test_my_function() -> I32 {
    assert_eq(result, expected, "values should match")
    // On failure, the test runner displays:
    // Error: Test panicked: assertion failed at tests/my_test.tml:15: values should match
    //
    // Backtrace:
    //    0: test_my_function
    //              at tests/my_test.tml:15
    return 0
}
```

Use `--no-backtrace` to disable backtrace capture in tests:

```bash
tml test --no-backtrace
```

## Module Structure

```
lib/backtrace/
├── src/
│   ├── mod.tml           # Module exports
│   ├── backtrace.tml     # Main Backtrace type
│   ├── frame.tml         # BacktraceFrame type
│   └── symbol.tml        # BacktraceSymbol type
├── tests/
│   └── capture.test.tml  # Unit tests
└── README.md             # This file

compiler/runtime/
├── backtrace.h           # C header
└── backtrace.c           # Platform-specific implementation
```

## Implementation Status

### Complete

- [x] Stack frame capture (Windows/Unix)
- [x] Symbol resolution (function names)
- [x] File and line number resolution
- [x] Lazy symbol resolution
- [x] Frame skipping
- [x] Formatted output
- [x] TML type wrappers
- [x] FFI bindings
- [x] Basic test suite
- [x] Compiler `--backtrace` flag (for `tml run`)
- [x] Panic handler integration
- [x] Test framework integration (auto-capture on test failures)
- [x] Internal frame filtering

### Planned

- [ ] Multiple symbols per frame (inlined functions)
- [ ] Symbol caching for performance
- [ ] Cross-library symbol resolution tests

## Performance Notes

- **Capture is fast** - Just copies stack pointers (~1μs)
- **Resolution is slow** - Loads debug symbols (~100μs per frame)
- **Resolve lazily** - Only call `resolve()` when you need symbol info
- **Cache results** - Frames store resolved symbols internally

## Building

The backtrace runtime is automatically compiled with the TML compiler:

```bash
scripts/build.bat   # Windows
scripts/build.sh    # Linux/macOS
```

The runtime links `dbghelp.lib` on Windows automatically via `#pragma comment`.
