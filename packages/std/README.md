# TML Standard Library

The TML standard library provides essential functionality for all TML programs.

## Modules

### Core Types
- **`prelude`** - Automatically imported types and functions
- **`option`** - `Maybe[T]` type for optional values
- **`result`** - `Outcome[T, E]` type for error handling
- **`error`** - Error traits and types

### Collections
- **`vec`** - Dynamic arrays
- **`map`** - Hash maps
- **`set`** - Hash sets

### I/O
- **`io`** - Input/output traits and functions
- **`fs`** - File system operations
- **`path`** - File path manipulation

### Concurrency
- **`sync`** - Synchronization primitives (Mutex, RwLock, etc.)
- **`thread`** - Thread creation and management
- **`channel`** - Message passing

### Networking
- **`net`** - TCP, UDP networking

### Utilities
- **`fmt`** - Formatting and printing
- **`time`** - Time and duration
- **`env`** - Environment variables
- **`process`** - Process management

## Usage

The standard library is automatically available:

```tml
// prelude is automatically imported
func main() {
    let x: Maybe[I32] = Just(42)
    println("Value: " + x.unwrap().to_string())
}
```

Explicit imports:

```tml
use std::collections::Vec
use std::io::{Read, Write}
use std::fs::File

func main() {
    let file: Outcome[File, Error] = File::open("data.txt")
    // ...
}
```

## Features

The standard library can be compiled with different feature sets:

- **`std`** (default) - Full standard library
- **`alloc`** - Only allocation, no I/O or OS dependencies
- **`io`** - I/O operations
- **`fs`** - File system operations
- **`net`** - Networking operations
- **`sync`** - Synchronization primitives

For embedded or no_std environments:

```toml
[dependencies]
std = { version = "0.1.0", default-features = false, features = ["alloc"] }
```

## Testing

The standard library includes comprehensive tests for all builtin functions:

```bash
# Run all stdlib tests
tml test packages/std/tests/stdlib/

# Run specific category
tml test packages/std/tests/stdlib/io.test.tml
tml test packages/std/tests/stdlib/time.test.tml
tml test packages/std/tests/stdlib/strings.test.tml
```

**Test Categories (stdlib only):**
- `io.test.tml` - I/O functions (print, println, print_i32, print_bool) ✅
- `time.test.tml` - Time functions (time_ms, elapsed_ms) ✅
- `math.test.tml` - Math functions (abs, sqrt, pow, round, floor, ceil) ❌

**Core/Runtime tests** (strings, memory, atomics) are in `packages/compiler/tests/runtime/`

See test READMEs for detailed results:
- `packages/std/tests/stdlib/README.md` - Stdlib test results
- `packages/compiler/tests/runtime/README.md` - Runtime test results

## Documentation

Full documentation is available at [docs.tml-lang.org/std](https://docs.tml-lang.org/std).

## License

MIT
