# TML Standard Library

**Pure TML modules built on core primitives**

The `std` package provides high-level utilities implemented in **pure TML** using `core` primitives. Unlike `core` modules which have C runtime implementations, std modules compile directly to LLVM IR without needing lowlevel code.

## Architecture

```
TML Application
       ↓
   std (pure TML)
       ↓
  core (system calls)
       ↓
   OS / libc
```

**Separation of concerns:**
- `core` = Low-level system primitives (mem, thread, sync, time) with C runtime
- `std` = High-level utilities in pure TML (collections, str, math, io, fs, net)

Similar to:
- **Rust**: `core` → `alloc` → `std`
- **Go**: `runtime` → standard library packages

## Planned Modules

### `std::collections` - Data Structures
Built using `core::mem` primitives.

**Planned types:**
- `List[T]` - Dynamic array (uses core::mem for allocation)
- `HashMap[K, V]` - Hash table
- `Buffer` - Byte buffer for I/O operations

**Status:** ❌ Not implemented (will be pure TML)

### `std::str` - String Utilities
Pure TML string operations.

**Planned functions:**
- `str_len(s: Str) -> I32` - String length
- `str_hash(s: Str) -> I64` - Hash string
- `str_eq(a: Str, b: Str) -> Bool` - String equality
- `str_concat(a: Str, b: Str) -> Str` - Concatenate strings
- `str_split(s: Str, delimiter: Str) -> List[Str]` - Split string

**Status:** ❌ Not implemented (will be pure TML)

### `std::math` - Math Functions
Pure TML math implementations.

**Planned functions:**
- `abs(x: I32) -> I32` - Absolute value
- `min(a: I32, b: I32) -> I32` - Minimum
- `max(a: I32, b: I32) -> I32` - Maximum
- `sqrt(x: F64) -> F64` - Square root (Newton's method)
- `pow(base: F64, exp: I32) -> F64` - Power
- `floor(x: F64) -> F64` - Floor function
- `ceil(x: F64) -> F64` - Ceiling function
- `round(x: F64) -> F64` - Round to nearest integer

**Status:** ❌ Not implemented (will be pure TML)

### `std::fmt` - Formatting
Pure TML formatting utilities.

**Planned functions:**
- `toString(value: I32) -> Str` - Convert integer to string
- `toFloat(s: Str) -> F64` - Parse float from string
- `toInt(s: Str) -> I32` - Parse integer from string

**Status:** ❌ Not implemented (will be pure TML)

### Future Modules

The following modules are planned for future releases:

- `std::io` - I/O traits and functions
- `std::fs` - File system operations
- `std::net` - TCP/UDP networking
- `std::process` - Process management
- `std::env` - Environment variables
- `std::path` - Path manipulation

## What's NOT in std

The following functionality is in **`core`** package (lowlevel with C runtime):

- ❌ `mem` - Memory allocation → use `core::mem`
- ❌ `thread` - Threading → use `core::thread`
- ❌ `sync` - Atomics, mutex → use `core::sync`
- ❌ `time` - Time functions → use `core::time`

## Usage

```tml
use core::mem      // For low-level memory operations
use std::collections  // For high-level data structures

func example() -> Unit {
    // Use core::mem for raw allocation
    let ptr: mut ref I32 = alloc(10)
    dealloc(ptr)

    // Use std::collections for high-level abstractions
    let list: List[I32] = List::new()
    list.push(42)
    let value: I32 = list.get(0)
}
```

## Build Process

Unlike `core` modules, `std` modules:
1. **No C runtime** - All implementations in pure TML
2. **Compile to LLVM IR** - No external object files needed
3. **Use core primitives** - Built on `core::mem`, `core::thread`, etc.

When building:
```bash
tml build-std      # Compiles both core (to .o) and std (to .tml.meta)
tml build app.tml  # Links only needed core .o files, inlines std code
```

## Dependencies

**All std modules depend on core:**
- `std::collections` → `core::mem` (for allocation)
- `std::str` → Pure TML (no dependencies)
- `std::math` → Pure TML (no dependencies)

## Implementation Status

| Module | Dependencies | Status | Notes |
|--------|-------------|--------|-------|
| `std::collections` | `core::mem` | ❌ Pending | List, HashMap, Buffer |
| `std::str` | None | ❌ Pending | Pure TML string ops |
| `std::math` | None | ❌ Pending | Pure TML math |
| `std::fmt` | None | ❌ Pending | Pure TML formatting |
| `std::io` | TBD | 🔮 Future | I/O traits |
| `std::fs` | TBD | 🔮 Future | File operations |
| `std::net` | TBD | 🔮 Future | Networking |

## Configuration

See `modules.toml` for module configuration. Currently all modules are commented out (pending implementation).

## Design Philosophy

**std should be:**
- **Pure TML** - No C code, compile to LLVM IR
- **High-level** - User-friendly abstractions over core primitives
- **Composable** - Build complex functionality from simple core primitives
- **Fast** - Inline everything, zero-cost abstractions

**Example - List implementation strategy:**
```tml
// std::collections::list.tml
use core::mem

type List[T] = struct {
    data: mut ref I32,
    length: I32,
    capacity: I32
}

func List::new() -> List[T] {
    List {
        data: alloc(0),  // core::mem::alloc
        length: 0,
        capacity: 0
    }
}

func List::push(this: mut ref List[T], value: T) -> Unit {
    // ... resize logic using core::mem primitives ...
}
```

## Testing

```bash
# Test std modules (when implemented)
tml test packages/std/tests/

# Test core dependencies
tml test packages/core/tests/
```

## See Also

- [TML Core Library](../core/README.md) - Low-level system primitives
- [TML Test Package](../test/README.md) - Testing framework
- [TML Compiler](../compiler/README.md) - Compiler implementation
