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
- **`string`** - String type and operations
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

## Documentation

Full documentation is available at [docs.tml-lang.org/std](https://docs.tml-lang.org/std).

## License

MIT
