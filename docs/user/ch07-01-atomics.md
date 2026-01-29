# Atomic Operations

Atomic operations are the foundation of thread-safe programming. They
guarantee that operations complete without interruption, even when
multiple threads access the same memory.

## Why Atomics?

Consider incrementing a counter from two threads without synchronization:

```
Thread 1: read counter (0)
Thread 2: read counter (0)
Thread 1: write counter (1)
Thread 2: write counter (1)  // Lost update!
```

Without atomics, both threads read 0 and write 1, losing an increment.
Atomic operations ensure each increment is indivisible.

## Atomic Types

TML provides atomic types in `std::sync::atomic`:

| Type | Description |
|------|-------------|
| `AtomicBool` | Atomic boolean |
| `AtomicI8`, `AtomicI16`, `AtomicI32`, `AtomicI64` | Atomic signed integers |
| `AtomicU8`, `AtomicU16`, `AtomicU32`, `AtomicU64` | Atomic unsigned integers |
| `AtomicIsize`, `AtomicUsize` | Atomic pointer-sized integers |
| `AtomicPtr[T]` | Atomic raw pointer |

## Memory Ordering

Every atomic operation requires a memory ordering that specifies
synchronization guarantees:

```tml
use std::sync::atomic::{AtomicI32, Ordering}

let counter: AtomicI32 = AtomicI32::new(0)

// Load with Acquire ordering
let value: I32 = counter.load(Ordering::Acquire)

// Store with Release ordering
counter.store(42, Ordering::Release)
```

### Ordering Options

| Ordering | Description |
|----------|-------------|
| `Relaxed` | No synchronization, just atomicity |
| `Acquire` | Synchronizes with Release stores (for loads) |
| `Release` | Synchronizes with Acquire loads (for stores) |
| `AcqRel` | Both Acquire and Release (for read-modify-write) |
| `SeqCst` | Strongest ordering, globally consistent |

**When in doubt, use `SeqCst`**. It's the safest option, though potentially
slower on some architectures.

## Basic Operations

### Creating Atomic Values

```tml
use std::sync::atomic::{AtomicI32, AtomicBool, Ordering}

// Create atomic values
let counter: AtomicI32 = AtomicI32::new(0)
let flag: AtomicBool = AtomicBool::new(false)
```

### Load and Store

```tml
use std::sync::atomic::{AtomicI32, Ordering}

let counter: AtomicI32 = AtomicI32::new(0)

// Atomic store
counter.store(42, Ordering::SeqCst)

// Atomic load
let value: I32 = counter.load(Ordering::SeqCst)
assert_eq(value, 42)
```

### Swap

Exchange the value atomically and return the old value:

```tml
use std::sync::atomic::{AtomicI32, Ordering}

let counter: AtomicI32 = AtomicI32::new(100)

// Swap returns old value
let old: I32 = counter.swap(200, Ordering::SeqCst)
assert_eq(old, 100)
assert_eq(counter.load(Ordering::SeqCst), 200)
```

## Arithmetic Operations

All fetch operations return the **previous** value:

```tml
use std::sync::atomic::{AtomicI32, Ordering}

let counter: AtomicI32 = AtomicI32::new(10)

// fetch_add: add and return old value
let old: I32 = counter.fetch_add(5, Ordering::SeqCst)
assert_eq(old, 10)
assert_eq(counter.load(Ordering::SeqCst), 15)

// fetch_sub: subtract and return old value
let old: I32 = counter.fetch_sub(3, Ordering::SeqCst)
assert_eq(old, 15)
assert_eq(counter.load(Ordering::SeqCst), 12)
```

### All Arithmetic Operations

| Method | Description |
|--------|-------------|
| `fetch_add(val, ord)` | Add and return old value |
| `fetch_sub(val, ord)` | Subtract and return old value |
| `fetch_max(val, ord)` | Maximum of current and val |
| `fetch_min(val, ord)` | Minimum of current and val |

## Bitwise Operations

```tml
use std::sync::atomic::{AtomicU32, Ordering}

let flags: AtomicU32 = AtomicU32::new(0b1111)

// Bitwise AND (clear bits)
let old: U32 = flags.fetch_and(0b1100, Ordering::SeqCst)
assert_eq(old, 0b1111)
assert_eq(flags.load(Ordering::SeqCst), 0b1100)

// Bitwise OR (set bits)
let old: U32 = flags.fetch_or(0b0011, Ordering::SeqCst)
assert_eq(old, 0b1100)
assert_eq(flags.load(Ordering::SeqCst), 0b1111)

// Bitwise XOR (toggle bits)
let old: U32 = flags.fetch_xor(0b1010, Ordering::SeqCst)
assert_eq(old, 0b1111)
assert_eq(flags.load(Ordering::SeqCst), 0b0101)
```

## Compare-and-Exchange (CAS)

CAS is the most powerful atomic operation. It only updates if the current
value matches the expected value:

```tml
use std::sync::atomic::{AtomicI32, Ordering}

let counter: AtomicI32 = AtomicI32::new(100)

// Try to change 100 -> 200
when counter.compare_exchange(100, 200, Ordering::SeqCst, Ordering::SeqCst) {
    Ok(old) => {
        // Success! old == 100
        assert_eq(old, 100)
        assert_eq(counter.load(Ordering::SeqCst), 200)
    },
    Err(current) => {
        // Failed - someone changed it first
        // current contains the actual value
    }
}
```

### CAS Variants

| Method | Description |
|--------|-------------|
| `compare_exchange(cur, new, succ_ord, fail_ord)` | Strong CAS |
| `compare_exchange_weak(cur, new, succ_ord, fail_ord)` | Weak CAS (may fail spuriously) |

Use `compare_exchange_weak` in loops for potentially better performance:

```tml
use std::sync::atomic::{AtomicI32, Ordering}

let counter: AtomicI32 = AtomicI32::new(0)

// Lock-free increment using CAS loop
var current: I32 = counter.load(Ordering::Relaxed)
loop {
    when counter.compare_exchange_weak(current, current + 1, Ordering::SeqCst, Ordering::Relaxed) {
        Ok(_) => break,
        Err(actual) => current = actual  // Retry with actual value
    }
}
```

## AtomicBool

AtomicBool is useful for flags and simple synchronization:

```tml
use std::sync::atomic::{AtomicBool, Ordering}

let ready: AtomicBool = AtomicBool::new(false)

// Check and set
let was_ready: Bool = ready.swap(true, Ordering::SeqCst)

// Common pattern: publish data
// Thread 1: prepare data, then set flag
ready.store(true, Ordering::Release)

// Thread 2: wait for flag, then read data
loop (not ready.load(Ordering::Acquire)) {
    // spin or yield
}
// Now safe to read the data
```

## Memory Ordering Examples

### Producer-Consumer Pattern

Use Release/Acquire for producer-consumer synchronization:

```tml
use std::sync::atomic::{AtomicBool, Ordering}

let ready: AtomicBool = AtomicBool::new(false)
var data: I32 = 0

// Producer thread
data = 42                              // Write data
ready.store(true, Ordering::Release)   // Publish: all writes before are visible

// Consumer thread
loop (not ready.load(Ordering::Acquire)) {
    // spin
}
// Acquire guarantees we see the write to `data`
assert_eq(data, 42)
```

### Simple Counter (Relaxed is OK)

When you just need a counter and don't care about ordering with other data:

```tml
use std::sync::atomic::{AtomicI64, Ordering}

let requests: AtomicI64 = AtomicI64::new(0)

// Multiple threads can increment - order doesn't matter
requests.fetch_add(1, Ordering::Relaxed)

// Read the total later
let total: I64 = requests.load(Ordering::Relaxed)
```

## Fence Operations

Fences provide ordering constraints without accessing specific variables:

```tml
use std::sync::atomic::{fence, Ordering}

// All writes before this fence are visible to threads
// that see writes after a corresponding Acquire fence
fence(Ordering::Release)

// All reads after this fence see writes that happened
// before a corresponding Release fence
fence(Ordering::Acquire)
```

### Spin Loop Hint

Optimize spin loops with a CPU hint:

```tml
use std::sync::atomic::{AtomicBool, spin_loop_hint, Ordering}

let flag: AtomicBool = AtomicBool::new(false)

// Efficient spin loop
loop (not flag.load(Ordering::Acquire)) {
    spin_loop_hint()  // Hint to CPU: we're spinning
}
```

## When to Use Atomics

Use atomics for:

1. **Simple counters**: Increment/decrement across threads
2. **Flags**: Boolean state shared between threads
3. **Lock-free data structures**: Building queues, stacks
4. **Statistics gathering**: When exact order doesn't matter

For complex shared state, prefer `Mutex[T]` or channels—they're easier
to use correctly.

## Common Mistakes

### Wrong: Using Relaxed for Synchronization

```tml
// WRONG: Relaxed doesn't synchronize!
ready.store(true, Ordering::Relaxed)  // Producer
// ...
loop (not ready.load(Ordering::Relaxed)) {}  // Consumer
// Consumer may not see writes before the store!
```

### Right: Using Release/Acquire

```tml
// RIGHT: Release/Acquire synchronize
ready.store(true, Ordering::Release)  // Producer
// ...
loop (not ready.load(Ordering::Acquire)) {}  // Consumer
// Consumer sees all writes before the Release store
```

## Lock-Free Check

All atomic types provide `is_lock_free()` to check if operations are truly
lock-free on the current platform:

```tml
use std::sync::atomic::AtomicI64

let counter: AtomicI64 = AtomicI64::new(0)
if counter.is_lock_free() {
    println("64-bit atomics are lock-free on this platform")
}
```
