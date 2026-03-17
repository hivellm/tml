# Appendix C - Builtin Functions

Builtin functions are available in every TML program without any `use`
declaration. They are injected by the compiler rather than defined in a
library module.

This appendix lists all builtins, grouped by purpose. Functions that require
a `lowlevel` block are noted explicitly.

---

## I/O Functions

These functions write to standard output and standard error, and read from
standard input.

| Function | Returns | Description |
|----------|---------|-------------|
| `print(value)` | `Unit` | Writes `value` to stdout without a trailing newline. |
| `println(value)` | `Unit` | Writes `value` to stdout followed by a newline. |
| `eprint(value)` | `Unit` | Writes `value` to stderr without a trailing newline. |
| `eprintln(value)` | `Unit` | Writes `value` to stderr followed by a newline. |
| `read_line()` | `Str` | Reads one line from stdin and returns it without the trailing newline. |

`print` and `println` accept any type that implements the `Display` behavior.
All primitive types (`I32`, `F64`, `Bool`, `Str`, etc.) implement `Display`.

```tml
print("Loading")
print(".")
println(" done")      // Loading. done

let name = read_line()
println("Hello, ${name}!")
```

### String Interpolation in I/O

The `${}` syntax inside string literals evaluates an expression and formats it
inline. This works everywhere a string literal is valid, not only inside print
calls.

```tml
let count = 42
let unit = count == 1 ? "item" : "items"
println("Found ${count} ${unit}.")

// Method calls and expressions are valid inside ${}
let values = [1, 2, 3]
println("Length: ${values.len()}, sum: ${values.iter().sum()}")
```

---

## Compile-Time Constants

These identifiers are resolved by the compiler at each point of use and produce
a `Str` or `I64` value. They cannot be assigned or shadowed.

| Constant | Type | Description |
|----------|------|-------------|
| `__FILE__` | `Str` | Absolute path of the current source file at compile time. |
| `__LINE__` | `I64` | Line number of the current source location. |
| `__DIRNAME__` | `Str` | Directory containing the current source file. |
| `__FUNC__` | `Str` | Name of the immediately enclosing function. |

```tml
func greet(name: Str) {
    println("[${__FUNC__}] called from ${__FILE__}:${__LINE__}")
    println("Hello, ${name}!")
}
```

---

## Assertion Functions

Assertions verify program invariants. A failed assertion calls `panic` with a
descriptive message that includes the source location.

### General Assertions

| Function | Description |
|----------|-------------|
| `assert(condition: Bool, message: Str)` | Panics with `message` if `condition` is `false`. |
| `assert_true(value: Bool)` | Panics if `value` is not `true`. |
| `assert_false(value: Bool)` | Panics if `value` is not `false`. |
| `debug_assert(condition: Bool)` | Like `assert`, but compiled out in release builds. |

### Equality Assertions

| Function | Description |
|----------|-------------|
| `assert_eq(actual, expected)` | Panics if `actual != expected`. Prints both values on failure. |
| `assert_ne(actual, expected)` | Panics if `actual == expected`. Prints both values on failure. |

### Ordering Assertions

| Function | Description |
|----------|-------------|
| `assert_lt(a, b)` | Panics if `a` is not less than `b`. |
| `assert_le(a, b)` | Panics if `a` is not less than or equal to `b`. |
| `assert_gt(a, b)` | Panics if `a` is not greater than `b`. |
| `assert_ge(a, b)` | Panics if `a` is not greater than or equal to `b`. |
| `assert_in_range(value, min, max)` | Panics if `value` is not in `[min, max]`. |

### String Assertions

| Function | Description |
|----------|-------------|
| `assert_str_len(s: Str, len: I64)` | Panics if `s.len() != len`. |
| `assert_str_empty(s: Str)` | Panics if `s` is not empty. |
| `assert_str_not_empty(s: Str)` | Panics if `s` is empty. |

```tml
let xs = [1, 2, 3]
assert_eq(xs.len(), 3)
assert_gt(xs[0], 0)
assert_in_range(xs[1], 1, 5)

let msg = "hello"
assert_str_not_empty(msg)
assert_str_len(msg, 5)
```

---

## Control Functions

These functions terminate the current execution path. Their return type is
`!` (the never type), meaning they never return normally.

| Function | Returns | Description |
|----------|---------|-------------|
| `panic(message: Str)` | `!` | Terminates the program immediately with an error message and stack information. |
| `unreachable(message: Str)` | `!` | Signals that the program reached a code path that should be impossible. Use when the compiler cannot prove the path is dead. |
| `todo(message: Str)` | `!` | Marks an unimplemented code path. The program panics if the path is reached at runtime. |

```tml
func describe(n: I32) -> Str {
    when n {
        0 -> "zero"
        1 -> "one"
        else -> todo("extend for larger numbers")
    }
}

func direction(deg: I32) -> Str {
    when deg {
        0   -> "North"
        90  -> "East"
        180 -> "South"
        270 -> "West"
        else -> unreachable("caller must normalize degrees to 0/90/180/270")
    }
}
```

---

## Memory Intrinsics

> These functions require a `lowlevel` block. Using them outside a `lowlevel`
> block is a compile error. They correspond directly to LLVM intrinsics and
> carry no safety guarantees.

Raw memory operations use `*Unit` as an opaque pointer (equivalent to `void*`
in C). The `core::ptr` module provides a `Ptr` type alias for convenience.

### Allocation

| Function | Signature | Description |
|----------|-----------|-------------|
| `mem_alloc(size: I64)` | `-> *Unit` | Allocates at least `size` bytes and returns a pointer to the first byte. The returned memory is uninitialized. |
| `mem_free(ptr: *Unit)` | `-> Unit` | Frees memory previously returned by `mem_alloc`. Calling with any other pointer is undefined behavior. |

### Typed Read / Write

| Function | Signature | Description |
|----------|-----------|-------------|
| `ptr_read[T](ptr: *Unit, offset: I64)` | `-> T` | Reads a `T`-sized value from `ptr + offset` bytes. The memory must be initialized and properly aligned for `T`. |
| `ptr_write[T](ptr: *Unit, offset: I64, value: T)` | `-> Unit` | Writes `value` to `ptr + offset` bytes. The offset must be aligned for `T`. |

### Pointer Arithmetic

| Function | Signature | Description |
|----------|-----------|-------------|
| `ptr_offset(ptr: *Unit, bytes: I64)` | `-> *Unit` | Returns a new pointer displaced by `bytes` from `ptr`. Equivalent to `(char*)ptr + bytes` in C. |
| `copy_nonoverlapping(src: *Unit, dst: *Unit, bytes: I64)` | `-> Unit` | Copies `bytes` bytes from `src` to `dst`. The regions must not overlap (use `ptr_offset` to verify). |

### Layout Queries

| Function | Signature | Description |
|----------|-----------|-------------|
| `size_of[T]()` | `-> I64` | Returns the size in bytes of type `T` as the compiler would lay it out in memory. |
| `align_of[T]()` | `-> I64` | Returns the required alignment in bytes of type `T`. |

```tml
lowlevel {
    let stride = size_of[I64]()           // 8
    let buf = mem_alloc(stride * 4)       // room for 4 I64 values

    ptr_write[I64](buf, 0, 10)
    ptr_write[I64](buf, stride, 20)
    ptr_write[I64](buf, stride * 2, 30)
    ptr_write[I64](buf, stride * 3, 40)

    let third = ptr_read[I64](buf, stride * 2)  // 30
    println("Third element: ${third}")

    mem_free(buf)
}
```

---

## Atomic Operations

> Atomic functions require a `lowlevel` block.

Atomic operations guarantee visibility and ordering across threads without
a mutex. All atomics operate on raw memory pointers; the caller is responsible
for ensuring the pointed-to memory has the correct size and alignment.

### I32 Atomics

| Function | Signature | Description |
|----------|-----------|-------------|
| `atomic_load_i32(ptr: *Unit)` | `-> I32` | Reads an I32 atomically. |
| `atomic_store_i32(ptr: *Unit, val: I32)` | `-> Unit` | Writes an I32 atomically. |
| `atomic_add_i32(ptr: *Unit, val: I32)` | `-> I32` | Adds `val`, returns the previous value. |
| `atomic_sub_i32(ptr: *Unit, val: I32)` | `-> I32` | Subtracts `val`, returns the previous value. |
| `atomic_and_i32(ptr: *Unit, val: I32)` | `-> I32` | Bitwise AND, returns the previous value. |
| `atomic_or_i32(ptr: *Unit, val: I32)` | `-> I32` | Bitwise OR, returns the previous value. |
| `atomic_xor_i32(ptr: *Unit, val: I32)` | `-> I32` | Bitwise XOR, returns the previous value. |
| `atomic_cas_i32(ptr: *Unit, expected: I32, desired: I32)` | `-> I32` | Compare-and-swap. If `*ptr == expected`, writes `desired`. Returns the value that was in `*ptr` before the operation. |

### I64 Atomics

| Function | Signature | Description |
|----------|-----------|-------------|
| `atomic_load_i64(ptr: *Unit)` | `-> I64` | Reads an I64 atomically. |
| `atomic_store_i64(ptr: *Unit, val: I64)` | `-> Unit` | Writes an I64 atomically. |
| `atomic_add_i64(ptr: *Unit, val: I64)` | `-> I64` | Adds `val`, returns the previous value. |
| `atomic_sub_i64(ptr: *Unit, val: I64)` | `-> I64` | Subtracts `val`, returns the previous value. |
| `atomic_cas_i64(ptr: *Unit, expected: I64, desired: I64)` | `-> I64` | Compare-and-swap, returns the previous value. |

### Memory Fences

| Function | Signature | Description |
|----------|-----------|-------------|
| `atomic_fence()` | `-> Unit` | Full sequential-consistency memory barrier. Prevents all reordering across the fence. |
| `atomic_fence_acquire()` | `-> Unit` | Acquire fence. Prevents loads and stores after this point from being reordered before it. |
| `atomic_fence_release()` | `-> Unit` | Release fence. Prevents loads and stores before this point from being reordered after it. |

```tml
lowlevel {
    let counter = mem_alloc(size_of[I32]())
    atomic_store_i32(counter, 0)

    // Increment and read
    let prev = atomic_add_i32(counter, 1)   // returns 0
    let now  = atomic_load_i32(counter)      // 1

    // Compare-and-swap loop (set to 10 only if currently 1)
    let old = atomic_cas_i32(counter, 1, 10)
    // old == 1 means the swap succeeded; counter is now 10

    mem_free(counter)
}
```

---

## Thread Functions

> Thread functions require a `lowlevel` block. For higher-level concurrency,
> prefer the `thread` module in the standard library.

| Function | Signature | Description |
|----------|-----------|-------------|
| `thread_spawn(fn: *Unit, arg: *Unit)` | `-> *Unit` | Starts a new OS thread. `fn` is a function pointer; `arg` is passed as its single argument. Returns an opaque thread handle. |
| `thread_join(handle: *Unit)` | `-> Unit` | Waits for the thread identified by `handle` to finish. |
| `thread_sleep(ms: I32)` | `-> Unit` | Suspends the current thread for at least `ms` milliseconds. |
| `thread_yield()` | `-> Unit` | Voluntarily yields the CPU to other threads. |
| `thread_id()` | `-> I32` | Returns a numeric identifier for the current thread. |

---

## Time

The recommended approach is to use `Instant` and `Duration` from the standard
library. The legacy functions listed below still work but are deprecated.

### Recommended API

```tml
use std::time::{Instant, Duration}

let start = Instant::now()
expensive_computation()
let elapsed: Duration = start.elapsed()
println("Finished in ${elapsed.as_millis()} ms")
```

### Deprecated Builtin Functions

These functions are available without an import but will be removed in a future
version.

| Function | Returns | Description |
|----------|---------|-------------|
| `time_now()` | `I64` | Current time in microseconds since an arbitrary epoch. Prefer `Instant::now()`. |
| `time_diff(start: I64, end: I64)` | `I64` | Difference in microseconds between two `time_now()` values. |
| `time_elapsed(start: I64)` | `I64` | Microseconds elapsed since `start`. Equivalent to `time_diff(start, time_now())`. |

---

## Synchronization Primitives

> These functions require a `lowlevel` block. For safe concurrency, prefer
> `Mutex`, `Channel`, and `WaitGroup` from the `std::sync` module.

### Spinlock

A spinlock is a busy-wait lock implemented with a single `I32` in memory.
Suitable for very short critical sections.

| Function | Signature | Description |
|----------|-----------|-------------|
| `spin_lock(ptr: *Unit)` | `-> Unit` | Busy-waits until the lock at `ptr` is acquired. |
| `spin_unlock(ptr: *Unit)` | `-> Unit` | Releases the spinlock at `ptr`. |
| `spin_trylock(ptr: *Unit)` | `-> Bool` | Attempts to acquire the lock once. Returns `true` if acquired. |

### WaitGroup

A WaitGroup coordinates a fixed number of concurrent operations.

| Function | Signature | Description |
|----------|-----------|-------------|
| `waitgroup_create()` | `-> *Unit` | Allocates and returns a new WaitGroup. |
| `waitgroup_add(wg: *Unit, n: I32)` | `-> Unit` | Adds `n` to the counter. Call before spawning work. |
| `waitgroup_done(wg: *Unit)` | `-> Unit` | Decrements the counter by one. Call when a unit of work completes. |
| `waitgroup_wait(wg: *Unit)` | `-> Unit` | Blocks until the counter reaches zero. |
| `waitgroup_destroy(wg: *Unit)` | `-> Unit` | Frees the WaitGroup. Call only after `waitgroup_wait` returns. |

### Channel

Channels provide first-in, first-out message passing between threads.

| Function | Signature | Description |
|----------|-----------|-------------|
| `channel_create()` | `-> *Unit` | Creates a new unbounded channel. |
| `channel_send(ch: *Unit, val: I32)` | `-> Bool` | Sends `val`. Returns `false` if the channel is closed. |
| `channel_recv(ch: *Unit)` | `-> I32` | Blocks until a value is available and returns it. |
| `channel_try_send(ch: *Unit, val: I32)` | `-> Bool` | Non-blocking send. Returns `false` if full or closed. |
| `channel_try_recv(ch: *Unit, out: *Unit)` | `-> Bool` | Non-blocking receive. Writes to `out` and returns `true` if a value was available. |
| `channel_len(ch: *Unit)` | `-> I32` | Returns the number of values currently queued. |
| `channel_close(ch: *Unit)` | `-> Unit` | Closes the channel. Subsequent sends return `false`. |
| `channel_destroy(ch: *Unit)` | `-> Unit` | Frees the channel. Call only after `channel_close`. |
