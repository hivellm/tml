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

### Mixed Format Examples

```tml
// Benchmarking output
let name: Str = "test"
let time: F64 = 0.266
let runs: I64 = 10
println("{}: {:.3} ms (avg of {} runs)", name, time, runs)
// Output: "test: 0.266 ms (avg of 10 runs)"

// Scientific values
let temperature: F64 = 36.847
println("Temperature: {:.1}°C", temperature)  // Temperature: 36.8°C
```

**Supported Types:**
- `Str` - Direct output
- `I8`, `I16`, `I32`, `I64`, `I128` - Integer formatting
- `U8`, `U16`, `U32`, `U64`, `U128` - Unsigned integer formatting
- `F32`, `F64` - Float formatting (supports precision)
- `Bool` - Prints "true" or "false"

## Memory Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `alloc(size)` | `(I32) -> ptr` | Allocate bytes |
| `dealloc(ptr)` | `(ptr) -> Unit` | Free memory |
| `read_i32(ptr)` | `(ptr) -> I32` | Read 32-bit int |
| `write_i32(ptr, val)` | `(ptr, I32) -> Unit` | Write 32-bit int |
| `ptr_offset(ptr, n)` | `(ptr, I32) -> ptr` | Offset pointer |

```tml
let mem = alloc(4)
write_i32(mem, 42)
println(read_i32(mem))  // 42
dealloc(mem)
```

## Atomic Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `atomic_load(ptr)` | `(ptr) -> I32` | Atomic read |
| `atomic_store(ptr, val)` | `(ptr, I32) -> Unit` | Atomic write |
| `atomic_add(ptr, val)` | `(ptr, I32) -> I32` | Fetch and add |
| `atomic_sub(ptr, val)` | `(ptr, I32) -> I32` | Fetch and subtract |
| `atomic_exchange(ptr, val)` | `(ptr, I32) -> I32` | Swap |
| `atomic_cas(ptr, exp, des)` | `(ptr, I32, I32) -> Bool` | Compare-and-swap |
| `atomic_and(ptr, val)` | `(ptr, I32) -> I32` | Atomic AND |
| `atomic_or(ptr, val)` | `(ptr, I32) -> I32` | Atomic OR |

## Synchronization Functions

### Spinlock

| Function | Signature | Description |
|----------|-----------|-------------|
| `spin_lock(ptr)` | `(ptr) -> Unit` | Acquire spinlock |
| `spin_unlock(ptr)` | `(ptr) -> Unit` | Release spinlock |
| `spin_trylock(ptr)` | `(ptr) -> Bool` | Try acquire |

### Memory Fences

| Function | Signature | Description |
|----------|-----------|-------------|
| `fence()` | `() -> Unit` | Full memory barrier |
| `fence_acquire()` | `() -> Unit` | Acquire fence |
| `fence_release()` | `() -> Unit` | Release fence |

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

## Mutex Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `mutex_create()` | `() -> ptr` | Create mutex |
| `mutex_lock(m)` | `(ptr) -> Unit` | Acquire lock |
| `mutex_unlock(m)` | `(ptr) -> Unit` | Release lock |
| `mutex_try_lock(m)` | `(ptr) -> Bool` | Try acquire |
| `mutex_destroy(m)` | `(ptr) -> Unit` | Free mutex |

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

⚠️ **These functions are deprecated. Use the `Instant` API instead.**

| Function | Signature | Status |
|----------|-----------|--------|
| `time_ms()` | `() -> I32` | ⚠️ Deprecated (use `Instant::now()`) |
| `time_us()` | `() -> I64` | ⚠️ Deprecated (use `Instant::now()`) |
| `time_ns()` | `() -> I64` | ⚠️ Deprecated (use `Instant::now()`) |

### Instant API (Recommended)

| Function | Signature | Description |
|----------|-----------|-------------|
| `Instant::now()` | `() -> I64` | High-resolution timestamp (μs) |
| `Instant::elapsed(start)` | `(I64) -> I64` | Duration since start (μs) |
| `Duration::as_secs_f64(us)` | `(I64) -> F64` | Duration in seconds as float |
| `Duration::as_millis_f64(us)` | `(I64) -> F64` | Duration in milliseconds as float |

**Example:**
```tml
// Measure execution time with format specifier
let start: I64 = Instant::now()
expensive_computation()
let elapsed: I64 = Instant::elapsed(start)
let ms: F64 = Duration::as_millis_f64(elapsed)
println("Time: {:.3} ms", ms)  // e.g., "Time: 0.266 ms"
```

**Benchmarking Example:**
```tml
func benchmark(name: Str, iterations: I64, runs: I64) {
    let mut total_us: I64 = 0

    for _ in 0 to runs {
        let start: I64 = Instant::now()
        // Run benchmark iterations times
        for _ in 0 to iterations {
            expensive_operation()
        }
        let elapsed: I64 = Instant::elapsed(start)
        total_us += elapsed
    }

    let avg_ms: F64 = Duration::as_millis_f64(total_us / runs)
    println("{}: {:.3} ms (avg of {} runs)", name, avg_ms, runs)
}
```
