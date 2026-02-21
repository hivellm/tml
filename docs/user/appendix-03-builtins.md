# Appendix C - Builtin Functions

TML provides several builtin functions that are always available.

## I/O Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `print(...)` | `(...) -> Unit` | Print values (no newline) |
| `println(...)` | `(...) -> Unit` | Print values with newline |

### Basic Usage

```tml
print("Hello ")
println("World!")  // Hello World!
println(42)        // 42
println(true)      // true
```

### Format Strings

Both `print` and `println` support format strings with placeholders:

```tml
// Basic placeholder {}
let name = "Alice"
println("Hello, {}!", name)  // Hello, Alice!

// Multiple values
let x = 10
let y = 20
println("Values: {} and {}", x, y)  // Values: 10 and 20
```

### String Interpolation

TML also supports direct string interpolation with `{expr}` syntax:

```tml
let name = "Alice"
let age = 30
println("Hello, {name}! You are {age} years old.")
```

### Precision Format Specifiers

Use `{:.N}` to specify decimal precision for floating-point numbers:

```tml
let pi: F64 = 3.14159265359

println("{}", pi)      // 3.14159265359
println("{:.0}", pi)   // 3
println("{:.2}", pi)   // 3.14
println("{:.5}", pi)   // 3.14159

// Works with integers too (converts to double)
let x: I32 = 42
println("{:.2}", x)    // 42.00
```

**Supported Types:**
- `Str` - Direct output
- `I8`, `I16`, `I32`, `I64`, `I128` - Integer formatting
- `U8`, `U16`, `U32`, `U64`, `U128` - Unsigned integer formatting
- `F32`, `F64` - Float formatting (supports precision)
- `Bool` - Prints "true" or "false"

## Memory Functions

Memory operations use `*Unit` as an opaque pointer type (similar to `void*` in C).

> **Tip:** The `core::ptr` module provides `Ptr` as a convenient alias for `*Unit`:
> ```tml
> use core::ptr::Ptr
> let mem: Ptr = mem_alloc(64)
> ```

### Modern Memory Intrinsics (Preferred)

| Function | Signature | Description |
|----------|-----------|-------------|
| `mem_alloc(size)` | `(I64) -> *Unit` | Allocate `size` bytes |
| `mem_free(ptr)` | `(*Unit) -> Unit` | Free allocated memory |
| `ptr_read[T](ptr)` | `(*Unit) -> T` | Read typed value from pointer |
| `ptr_write[T](ptr, val)` | `(*Unit, T) -> Unit` | Write typed value to pointer |
| `ptr_offset(ptr, n)` | `(*Unit, I64) -> *Unit` | Offset pointer by `n` bytes |
| `copy_nonoverlapping(src, dst, n)` | `(*Unit, *Unit, I64) -> Unit` | Copy `n` bytes (memcpy) |

```tml
let mem = mem_alloc(16)
ptr_write[I64](mem, 42)
let val = ptr_read[I64](mem)
println("{val}")  // 42
mem_free(mem)
```

### Legacy Memory Functions (Still Supported)

| Function | Signature | Description |
|----------|-----------|-------------|
| `alloc(size)` | `(I32) -> *Unit` | Allocate bytes (size * 4 bytes) |
| `dealloc(ptr)` | `(*Unit) -> Unit` | Free memory |
| `read_i32(ptr)` | `(*Unit) -> I32` | Read 32-bit int |
| `write_i32(ptr, val)` | `(*Unit, I32) -> Unit` | Write 32-bit int |

## Compile-Time Constants

| Constant | Type | Description |
|----------|------|-------------|
| `__FILE__` | `Str` | Path of the current source file |
| `__DIRNAME__` | `Str` | Directory containing the current source file |
| `__LINE__` | `I64` | Current line number |

```tml
println("File: {__FILE__}")
println("Dir: {__DIRNAME__}")
println("Line: {__LINE__}")
```

## Atomic Functions

Atomic operations for thread-safe memory access. All functions use type-specific variants.

### I32 Atomics

| Function | Signature | Description |
|----------|-----------|-------------|
| `atomic_load_i32(ptr)` | `(*Unit) -> I32` | Atomic read |
| `atomic_store_i32(ptr, val)` | `(*Unit, I32) -> Unit` | Atomic write |
| `atomic_fetch_add_i32(ptr, val)` | `(*Unit, I32) -> I32` | Fetch and add |
| `atomic_fetch_sub_i32(ptr, val)` | `(*Unit, I32) -> I32` | Fetch and subtract |
| `atomic_swap_i32(ptr, val)` | `(*Unit, I32) -> I32` | Swap |
| `atomic_compare_exchange_i32(ptr, exp, des)` | `(*Unit, I32, I32) -> I32` | Compare-and-exchange |
| `atomic_and_i32(ptr, val)` | `(*Unit, I32) -> I32` | Atomic AND |
| `atomic_or_i32(ptr, val)` | `(*Unit, I32) -> I32` | Atomic OR |

### I64 Atomics

| Function | Signature | Description |
|----------|-----------|-------------|
| `atomic_load_i64(ptr)` | `(*Unit) -> I64` | Atomic read |
| `atomic_store_i64(ptr, val)` | `(*Unit, I64) -> Unit` | Atomic write |
| `atomic_fetch_add_i64(ptr, val)` | `(*Unit, I64) -> I64` | Fetch and add |
| `atomic_fetch_sub_i64(ptr, val)` | `(*Unit, I64) -> I64` | Fetch and subtract |
| `atomic_swap_i64(ptr, val)` | `(*Unit, I64) -> I64` | Swap |
| `atomic_compare_exchange_i64(ptr, exp, des)` | `(*Unit, I64, I64) -> I64` | Compare-and-exchange |

```tml
let counter = mem_alloc(4)
atomic_store_i32(counter, 0)

let old = atomic_fetch_add_i32(counter, 1)
println("{old}")  // 0

let value = atomic_load_i32(counter)
println("{value}")  // 1

mem_free(counter)
```

## Memory Fences

| Function | Signature | Description |
|----------|-----------|-------------|
| `atomic_fence()` | `() -> Unit` | Full memory barrier (SeqCst) |
| `atomic_fence_acquire()` | `() -> Unit` | Acquire fence |
| `atomic_fence_release()` | `() -> Unit` | Release fence |

## Synchronization Functions

### Spinlock

| Function | Signature | Description |
|----------|-----------|-------------|
| `spin_lock(ptr)` | `(*Unit) -> Unit` | Acquire spinlock |
| `spin_unlock(ptr)` | `(*Unit) -> Unit` | Release spinlock |
| `spin_trylock(ptr)` | `(*Unit) -> Bool` | Try acquire |

### Mutex

| Function | Signature | Description |
|----------|-----------|-------------|
| `mutex_create()` | `() -> ptr` | Create mutex |
| `mutex_lock(m)` | `(ptr) -> Unit` | Acquire lock |
| `mutex_unlock(m)` | `(ptr) -> Unit` | Release lock |
| `mutex_try_lock(m)` | `(ptr) -> Bool` | Try acquire |
| `mutex_destroy(m)` | `(ptr) -> Unit` | Free mutex |

## Channel Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `channel_create()` | `() -> ptr` | Create channel |
| `channel_send(ch, val)` | `(ptr, I32) -> Bool` | Blocking send |
| `channel_recv(ch)` | `(ptr) -> I32` | Blocking receive |
| `channel_try_send(ch, val)` | `(ptr, I32) -> Bool` | Non-blocking send |
| `channel_try_recv(ch, out)` | `(ptr, ptr) -> Bool` | Non-blocking recv |
| `channel_len(ch)` | `(ptr) -> I32` | Get length |
| `channel_close(ch)` | `(ptr) -> Unit` | Close channel |
| `channel_destroy(ch)` | `(ptr) -> Unit` | Free channel |

## WaitGroup Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `waitgroup_create()` | `() -> ptr` | Create WaitGroup |
| `waitgroup_add(wg, n)` | `(ptr, I32) -> Unit` | Add to counter |
| `waitgroup_done(wg)` | `(ptr) -> Unit` | Decrement counter |
| `waitgroup_wait(wg)` | `(ptr) -> Unit` | Wait for zero |
| `waitgroup_destroy(wg)` | `(ptr) -> Unit` | Free WaitGroup |

## Thread Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `thread_spawn(fn, arg)` | `(ptr, ptr) -> ptr` | Start thread |
| `thread_join(handle)` | `(ptr) -> Unit` | Wait for thread |
| `thread_yield()` | `() -> Unit` | Yield CPU |
| `thread_sleep(ms)` | `(I32) -> Unit` | Sleep milliseconds |
| `thread_id()` | `() -> I32` | Get current thread ID |

## Time and Benchmarking

### Deprecated Time Functions

These functions are deprecated. Use the `Instant` API instead.

| Function | Signature | Status |
|----------|-----------|--------|
| `time_ms()` | `() -> I32` | Deprecated (use `Instant::now()`) |
| `time_us()` | `() -> I64` | Deprecated (use `Instant::now()`) |
| `time_ns()` | `() -> I64` | Deprecated (use `Instant::now()`) |

### Instant API (Recommended)

| Function | Signature | Description |
|----------|-----------|-------------|
| `Instant::now()` | `() -> I64` | High-resolution timestamp (microseconds) |
| `Instant::elapsed(start)` | `(I64) -> I64` | Duration since start (microseconds) |
| `Duration::as_secs_f64(us)` | `(I64) -> F64` | Duration in seconds as float |
| `Duration::as_millis_f64(us)` | `(I64) -> F64` | Duration in milliseconds as float |

```tml
let start = Instant::now()
expensive_computation()
let elapsed = Instant::elapsed(start)
let ms = Duration::as_millis_f64(elapsed)
println("Time: {:.3} ms", ms)
```

## Debug / Test Assertions

| Function | Signature | Description |
|----------|-----------|-------------|
| `assert(cond)` | `(Bool) -> Unit` | Assert condition is true |
| `assert_eq(a, b)` | `(T, T) -> Unit` | Assert values are equal |
| `assert_ne(a, b)` | `(T, T) -> Unit` | Assert values are not equal |
| `assert_true(a)` | `(Bool) -> Unit` | Assert value is true |
| `assert_false(a)` | `(Bool) -> Unit` | Assert value is false |
| `assert_lt(a, b)` | `(T, T) -> Unit` | Assert a < b |
| `assert_gt(a, b)` | `(T, T) -> Unit` | Assert a > b |
| `assert_lte(a, b)` | `(T, T) -> Unit` | Assert a <= b |
| `assert_gte(a, b)` | `(T, T) -> Unit` | Assert a >= b |
| `assert_in_range(val, min, max)` | `(T, T, T) -> Unit` | Assert min <= val <= max |
| `assert_str_len(s, len)` | `(Str, I64) -> Unit` | Assert string length |
| `assert_str_empty(s)` | `(Str) -> Unit` | Assert string is empty |
| `assert_str_not_empty(s)` | `(Str) -> Unit` | Assert string is not empty |
