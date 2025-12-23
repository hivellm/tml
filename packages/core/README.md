# TML Core Library

**Low-level system primitives with C runtime implementations**

The `core` package provides the foundation for all TML applications. These are the **only** modules that directly call system APIs (malloc, pthread, gettimeofday, etc.). All other TML code should be built using these core primitives.

## Architecture

Similar to Rust's organization:
- **Rust**: `core` (no_std) → `alloc` (heap) → `std` (OS features)
- **TML**: `core` (system calls) → `std` (pure TML)

All core modules have **lowlevel** C implementations in `runtime/`.

## Modules

### `core::mem` - Memory Allocation
Wrappers around `malloc`/`free` system calls.

**Functions:**
- `alloc(count: I64) -> mut ref I32` - Allocate memory for N elements
- `dealloc(ptr: mut ref I32) -> Unit` - Free allocated memory
- `read_i32(ptr: ref I32) -> I32` - Read value from pointer
- `write_i32(ptr: mut ref I32, value: I32) -> Unit` - Write value to pointer
- `ptr_offset(ptr: mut ref I32, offset: I64) -> mut ref I32` - Pointer arithmetic

**Runtime:** `runtime/mem.c` (733 bytes)

### `core::thread` - Threading Primitives
Wrappers around `pthread` API.

**Functions:**
- `thread_spawn(func_ptr: I64) -> I64` - Create new thread
- `thread_join(thread_id: I64) -> I32` - Wait for thread completion
- `thread_sleep(ms: I32) -> Unit` - Sleep for N milliseconds
- `thread_yield() -> Unit` - Yield CPU to other threads

**Channels:**
- `channel_create(capacity: I32) -> I64` - Create buffered channel
- `channel_send(chan: I64, value: I32) -> Unit` - Send value
- `channel_recv(chan: I64) -> I32` - Receive value (blocking)
- `channel_close(chan: I64) -> Unit` - Close channel

**Runtime:** `runtime/thread.c` (9.7 KB)

### `core::sync` - Synchronization Primitives
Atomic operations and synchronization primitives. Shares runtime with `core::thread`.

**Atomic Operations:**
- `atomic_load(ptr: ref I32) -> I32`
- `atomic_store(ptr: mut ref I32, value: I32) -> Unit`
- `atomic_add(ptr: mut ref I32, value: I32) -> I32`
- `atomic_sub(ptr: mut ref I32, value: I32) -> I32`
- `atomic_exchange(ptr: mut ref I32, value: I32) -> I32`
- `atomic_cas(ptr: mut ref I32, expected: I32, desired: I32) -> Bool`
- `atomic_cas_val(ptr: mut ref I32, expected: I32, desired: I32) -> I32`
- `atomic_and(ptr: mut ref I32, value: I32) -> I32`
- `atomic_or(ptr: mut ref I32, value: I32) -> I32`

**Memory Fences:**
- `fence() -> Unit` - Full memory fence
- `fence_acquire() -> Unit` - Acquire fence
- `fence_release() -> Unit` - Release fence

**Spinlocks:**
- `spin_lock(lock: mut ref I32) -> Unit`
- `spin_unlock(lock: mut ref I32) -> Unit`
- `spin_trylock(lock: mut ref I32) -> Bool`

**Runtime:** `runtime/thread.c` (shared with core::thread)

### `core::time` - Time Functions
Wrappers around `gettimeofday` system call.

**Functions:**
- `time_ms() -> I32` - Current time in milliseconds (for benchmarking)
- `time_us() -> I64` - Current time in microseconds
- `time_ns() -> I64` - Current time in nanoseconds

**Instant API:**
- `instant_now() -> I64` - Capture current instant
- `instant_elapsed_ms(start: I64) -> I32` - Milliseconds since instant
- `instant_elapsed_us(start: I64) -> I64` - Microseconds since instant

**Duration API:**
- `duration_from_secs(secs: I32) -> I64` - Create duration from seconds
- `duration_from_millis(ms: I32) -> I64` - Create duration from milliseconds

**Formatting:**
- `format_f32(value: F32, precision: I32) -> Str` - Format float32
- `format_f64(value: F64, precision: I32) -> Str` - Format float64

**Runtime:** `runtime/time.c` (3.4 KB)

## Usage

```tml
use core::mem
use core::thread
use core::sync
use core::time

func example() -> Unit {
    // Memory allocation
    let ptr: mut ref I32 = alloc(10)
    write_i32(ptr, 42)
    let value: I32 = read_i32(ptr)
    dealloc(ptr)

    // Threading
    let thread_id: I64 = thread_spawn(worker_func_ptr)
    thread_join(thread_id)

    // Atomic operations
    let counter: mut ref I32 = alloc(1)
    atomic_store(counter, 0)
    atomic_add(counter, 1)
    let count: I32 = atomic_load(counter)
    dealloc(counter)

    // Timing
    let start: I64 = instant_now()
    // ... do work ...
    let elapsed: I32 = instant_elapsed_ms(start)
}
```

## Build Process

Core modules are compiled to native code during `tml build-std`:

1. Parse `core/modules.toml` configuration
2. Compile each `.tml` source file → extract type signatures → save to `.tml.meta`
3. Compile each `.c` runtime file → generate `.o` or `.obj` object file
4. Save compiled artifacts to `core/compiled/`

When building a TML application:
- Type checker loads `.tml.meta` files for fast module resolution
- Linker includes `.o`/`.obj` files for modules that were imported

## File Structure

```
packages/core/
├── modules.toml           # Module configuration
├── README.md              # This file
├── src/                   # TML source files
│   ├── mem.tml           # (to be created)
│   ├── thread.tml        # (to be created)
│   ├── sync.tml          # (to be created)
│   └── time.tml          # (to be created)
└── runtime/              # C implementations
    ├── mem.c             # 733 bytes
    ├── thread.c          # 9.7 KB
    └── time.c            # 3.4 KB
```

## Dependencies

**Zero dependencies** - Core modules only depend on standard C library system calls:
- `stdlib.h` - malloc, free
- `pthread.h` - threading
- `time.h` - gettimeofday
- `stdatomic.h` - atomic operations

## Implementation Status

| Module | TML Source | C Runtime | Status |
|--------|-----------|-----------|--------|
| `core::mem` | ❌ Pending | ✅ Complete | Runtime ready |
| `core::thread` | ❌ Pending | ✅ Complete | Runtime ready |
| `core::sync` | ❌ Pending | ✅ Complete | Runtime ready |
| `core::time` | ❌ Pending | ✅ Complete | Runtime ready |

**Next steps:**
1. Write `.tml` source files with function signatures
2. Mark all functions as `lowlevel`
3. Implement `build-std` command to compile core modules
4. Update linker to automatically include compiled `.o` files

## Design Philosophy

**Core should be minimal and stable:**
- Only include functionality that **must** call system APIs directly
- No pure TML implementations (those go in `std`)
- No convenience wrappers (those go in `std`)
- Stable API - breaking changes require major version bump

**Everything else goes in `std`:**
- Collections (List, HashMap, Buffer) → use `core::mem`
- String utilities → pure TML
- Math functions → pure TML
- I/O, filesystem, networking → separate modules

## See Also

- [TML Standard Library](../std/README.md) - Pure TML modules built on core
- [TML Test Package](../test/README.md) - Testing framework
- [TML Compiler](../compiler/README.md) - Compiler implementation
