# Implement Backtrace Library

## Overview

Implement a backtrace library for TML to enable runtime stack trace capture and analysis for debugging. This will be a new library module (`lib/backtrace`) incorporated into the compiler alongside `core`, `std`, and `test`.

Based on: https://github.com/rust-lang/backtrace-rs

## Goals

1. **Stack frame capture** - Programmatically capture call stack at runtime
2. **Symbol resolution** - Convert instruction pointers to function names, file paths, and line numbers
3. **Cross-platform support** - Windows (primary), Linux, macOS
4. **Panic integration** - Automatic backtrace on panic/assert failures
5. **Debug symbol support** - Work with DWARF/PDB debug information
6. **Minimal overhead** - Lazy symbolication, optional caching

## Technical Approach

### Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        lib/backtrace                            │
├─────────────────────────────────────────────────────────────────┤
│  Public API (backtrace.tml)                                     │
│  ├── Backtrace::capture() -> Backtrace                          │
│  ├── Backtrace::frames() -> Slice[Frame]                        │
│  ├── Frame::ip() -> *Unit                                       │
│  ├── Frame::symbol() -> Maybe[Symbol]                           │
│  └── Symbol::{name, filename, lineno, colno}                    │
├─────────────────────────────────────────────────────────────────┤
│  Platform Unwinding Layer                                       │
│  ├── windows/  (RtlCaptureStackBackTrace, StackWalk64)         │
│  ├── unix/     (libunwind, _Unwind_Backtrace)                  │
│  └── noop/     (fallback for unsupported platforms)            │
├─────────────────────────────────────────────────────────────────┤
│  Symbolication Layer                                            │
│  ├── dbghelp/  (Windows PDB symbols)                           │
│  ├── dwarf/    (ELF/Mach-O DWARF debug info)                   │
│  └── gimli/    (Pure implementation, no system deps)           │
├─────────────────────────────────────────────────────────────────┤
│  FFI Runtime (compiler/runtime/backtrace.c)                     │
│  └── Platform-specific C implementations                        │
└─────────────────────────────────────────────────────────────────┘
```

### Core Types

```tml
// lib/backtrace/src/backtrace.tml

/// A captured stack backtrace
pub type Backtrace {
    frames: List[BacktraceFrame]
    resolved: Bool
}

/// A single stack frame
pub type BacktraceFrame {
    ip: *Unit           // Instruction pointer
    sp: *Unit           // Stack pointer (optional)
    symbol_address: *Unit
    symbols: List[BacktraceSymbol]
}

/// Symbolicated information for a frame
pub type BacktraceSymbol {
    name: Maybe[Str]
    filename: Maybe[Str]
    lineno: Maybe[U32]
    colno: Maybe[U32]
}

impl Backtrace {
    /// Capture current stack trace
    pub func capture() -> Backtrace

    /// Capture with specific frame skip count
    pub func capture_from(skip: I32) -> Backtrace

    /// Resolve symbols (lazy, called on demand)
    pub func resolve(mut this)

    /// Get frames
    pub func frames(this) -> Slice[BacktraceFrame]

    /// Format for display
    pub func to_string(this) -> Str
}
```

### FFI Layer

```c
// compiler/runtime/backtrace.c

// Capture raw stack frames
int backtrace_capture(void** frames, int max_frames, int skip);

// Resolve symbol for address
int backtrace_resolve(void* addr, BacktraceSymbol* out);

// Platform-specific implementations
#ifdef _WIN32
    // RtlCaptureStackBackTrace + DbgHelp
#elif defined(__linux__) || defined(__APPLE__)
    // libunwind or _Unwind_Backtrace + DWARF
#endif
```

### Windows Implementation (Primary)

```c
// Using DbgHelp API for symbol resolution
#include <windows.h>
#include <dbghelp.h>

int backtrace_capture_windows(void** frames, int max, int skip) {
    return RtlCaptureStackBackTrace(skip, max, frames, NULL);
}

int backtrace_resolve_windows(void* addr, BacktraceSymbol* out) {
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    if (SymFromAddr(GetCurrentProcess(), (DWORD64)addr, 0, symbol)) {
        out->name = symbol->Name;
    }

    IMAGEHLP_LINE64 line;
    DWORD displacement;
    if (SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)addr,
                             &displacement, &line)) {
        out->filename = line.FileName;
        out->lineno = line.LineNumber;
    }
    return 0;
}
```

### Compiler Integration

#### 1. Panic Handler Enhancement

```tml
// lib/core/src/panic.tml

@panic_handler
pub func default_panic_handler(msg: Str, file: Str, line: I32) {
    print("panic: {msg}\n")
    print("  at {file}:{line}\n")

    #if BACKTRACE
    print("\nBacktrace:\n")
    let bt = Backtrace::capture_from(2) // skip panic frames
    bt.resolve()
    print(bt.to_string())
    #endif

    exit(1)
}
```

#### 2. Test Framework Integration

```tml
// lib/test/src/assert.tml

pub func assert_eq[T: Eq](actual: T, expected: T, msg: Str) {
    if actual != expected {
        print("Assertion failed: {msg}\n")
        print("  expected: {expected}\n")
        print("  actual: {actual}\n")

        #if BACKTRACE
        let bt = Backtrace::capture_from(1)
        bt.resolve()
        print("\nBacktrace:\n{bt}")
        #endif

        panic("assertion failed")
    }
}
```

#### 3. Build System Integration

```cpp
// compiler/src/cli/builder/build.cpp

// Link backtrace runtime when enabled
if (options.backtrace) {
    link_args.push_back("backtrace_runtime.lib");
    #ifdef _WIN32
    link_args.push_back("dbghelp.lib");
    #endif
}
```

### Output Format

```
panic: index out of bounds
  at lib/core/src/slice.tml:42

Backtrace:
   0: core::slice::Slice::get
             at lib/core/src/slice.tml:42
   1: std::collections::List::at
             at lib/std/src/collections/list.tml:156
   2: main::process_items
             at examples/demo.tml:23
   3: main::main
             at examples/demo.tml:45
   4: tml_main
             at <entry>
```

## Project Structure

```
lib/backtrace/
├── src/
│   ├── mod.tml           # Module exports
│   ├── backtrace.tml     # Main Backtrace type
│   ├── frame.tml         # Frame and Symbol types
│   ├── capture.tml       # Stack capture logic
│   ├── symbolize.tml     # Symbol resolution
│   └── format.tml        # Backtrace formatting
└── tests/
    ├── capture.test.tml
    ├── resolve.test.tml
    └── format.test.tml

compiler/runtime/
├── backtrace.c           # Platform-agnostic wrapper
├── backtrace_win.c       # Windows implementation
├── backtrace_unix.c      # Unix implementation
└── backtrace.h           # C header
```

## File Changes

### New Files
- `lib/backtrace/src/*.tml` - TML library implementation
- `lib/backtrace/tests/*.tml` - Library tests
- `compiler/runtime/backtrace.c` - FFI runtime
- `compiler/runtime/backtrace_win.c` - Windows unwinding
- `compiler/runtime/backtrace_unix.c` - Unix unwinding

### Modified Files
- `compiler/src/cli/builder/build.cpp` - Link backtrace runtime
- `compiler/src/cli/builder/compiler_setup.cpp` - Detect DbgHelp
- `lib/core/src/panic.tml` - Add backtrace to panic handler
- `lib/test/src/assert.tml` - Add backtrace to assertions
- `compiler/CMakeLists.txt` - Add backtrace runtime compilation

## Dependencies

### Windows
- `dbghelp.dll` (Windows SDK, included in Windows)
- No external dependencies

### Linux/macOS
- `libunwind` (optional, fallback available)
- DWARF debug info in binaries

## Configuration

```bash
# Enable backtrace support (default: enabled in debug)
tml build file.tml --backtrace

# Disable backtrace (smaller binary, faster)
tml build file.tml --no-backtrace

# Full symbols for production backtrace
tml build file.tml --release --backtrace --debug
```

## Testing

1. Basic capture test - verify frames are captured
2. Symbol resolution test - verify names/files/lines
3. Skip frames test - verify frame skipping works
4. Recursive function test - verify deep stacks
5. Cross-library test - verify symbols from different modules
6. Panic integration test - verify backtrace in panic
7. Performance test - measure capture/resolve overhead

## Future Enhancements

1. **Async backtrace** - Capture across async boundaries
2. **Minidump support** - Generate crash dumps
3. **Source snippets** - Show source code in backtraces
4. **Symbol caching** - Persistent symbol cache for faster resolution
5. **Remote symbolication** - Send addresses to symbol server
