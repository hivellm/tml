# Atomic Operations

Atomic operations are indivisible memory operations — from the perspective of
any other thread, an atomic operation either has not started or has completely
finished. There is no intermediate state that another thread can observe.

This property makes atomic types the foundation of lock-free concurrency.
A counter incremented atomically from ten threads will always reach exactly
ten; a counter incremented with plain load/store operations can silently drop
increments due to data races.

Atomic types live in `std::sync::atomic`.

## Atomic Types

| Type | Equivalent C type |
|------|-------------------|
| `AtomicBool` | `_Atomic bool` |
| `AtomicI8` | `_Atomic int8_t` |
| `AtomicI16` | `_Atomic int16_t` |
| `AtomicI32` | `_Atomic int32_t` |
| `AtomicI64` | `_Atomic int64_t` |
| `AtomicU8` | `_Atomic uint8_t` |
| `AtomicU16` | `_Atomic uint16_t` |
| `AtomicU32` | `_Atomic uint32_t` |
| `AtomicU64` | `_Atomic uint64_t` |
| `AtomicIsize` | `_Atomic intptr_t` |
| `AtomicUsize` | `_Atomic uintptr_t` |
| `AtomicPtr[T]` | `_Atomic T*` |

All atomic types are `Send` and `Sync`, so they can be freely shared across
threads without wrapping in `Arc` or `Mutex`.

## Memory Ordering

Every atomic operation requires a **memory ordering** argument that controls
what guarantees the operation carries about the visibility of other memory
operations. Choosing the right ordering is the primary challenge of lock-free
programming.

```tml
use std::sync::atomic::{AtomicI32, Ordering}

let counter: AtomicI32 = AtomicI32::new(0)
counter.store(42, Ordering::Release)
let value: I32 = counter.load(Ordering::Acquire)
```

### Ordering Options

| Ordering | Allowed for | Guarantees |
|----------|-------------|-----------|
| `Relaxed` | Load, Store, RMW | Atomicity only; no ordering with other operations |
| `Acquire` | Load, RMW | All subsequent reads/writes in this thread happen after this load |
| `Release` | Store, RMW | All prior reads/writes in this thread are visible before this store |
| `AcqRel` | RMW only | Combines Acquire (on the load) and Release (on the store) |
| `SeqCst` | Load, Store, RMW | Globally consistent ordering across all threads and all atomic operations |

*RMW = read-modify-write operations (fetch_add, swap, compare_exchange, etc.)*

**When in doubt, use `SeqCst`.** It is the safest and most conservative
option. It may be slightly slower on ARM, but correctness is more important
than micro-optimization. Reach for weaker orderings only after you understand
exactly what synchronization guarantees you need.

## Load and Store

`load` reads the current value. `store` writes a new value. Neither returns
the old value; for that, use `swap`.

```tml
use std::sync::atomic::{AtomicI32, Ordering}

func main() {
    let counter: AtomicI32 = AtomicI32::new(0)

    counter.store(100, Ordering::SeqCst)

    let value: I32 = counter.load(Ordering::SeqCst)
    assert_eq(value, 100)
}
```

## Swap

`swap` atomically replaces the stored value and returns the previous one.

```tml
use std::sync::atomic::{AtomicI32, Ordering}

func main() {
    let counter: AtomicI32 = AtomicI32::new(7)

    let old: I32 = counter.swap(99, Ordering::SeqCst)
    assert_eq(old, 7)
    assert_eq(counter.load(Ordering::SeqCst), 99)
}
```

## Arithmetic Operations

All fetch operations perform a read-modify-write in one atomic step and return
the value that was in the cell *before* the operation.

```tml
use std::sync::atomic::{AtomicI32, Ordering}

func main() {
    let n: AtomicI32 = AtomicI32::new(10)

    let prev: I32 = n.fetch_add(5, Ordering::SeqCst)
    assert_eq(prev, 10)
    assert_eq(n.load(Ordering::SeqCst), 15)

    let prev: I32 = n.fetch_sub(3, Ordering::SeqCst)
    assert_eq(prev, 15)
    assert_eq(n.load(Ordering::SeqCst), 12)
}
```

| Method | Description |
|--------|-------------|
| `fetch_add(val, ord)` | Adds `val`, returns previous value |
| `fetch_sub(val, ord)` | Subtracts `val`, returns previous value |
| `fetch_max(val, ord)` | Stores the maximum of current and `val`, returns previous |
| `fetch_min(val, ord)` | Stores the minimum of current and `val`, returns previous |

## Bitwise Operations

```tml
use std::sync::atomic::{AtomicU32, Ordering}

func main() {
    let flags: AtomicU32 = AtomicU32::new(0b1111)

    // Clear bits 0 and 1.
    let prev: U32 = flags.fetch_and(0b1100, Ordering::SeqCst)
    assert_eq(flags.load(Ordering::SeqCst), 0b1100)

    // Set bits 0 and 1.
    let prev: U32 = flags.fetch_or(0b0011, Ordering::SeqCst)
    assert_eq(flags.load(Ordering::SeqCst), 0b1111)

    // Toggle bits 1 and 3.
    let prev: U32 = flags.fetch_xor(0b1010, Ordering::SeqCst)
    assert_eq(flags.load(Ordering::SeqCst), 0b0101)
}
```

| Method | Description |
|--------|-------------|
| `fetch_and(val, ord)` | Bitwise AND, returns previous value |
| `fetch_or(val, ord)` | Bitwise OR, returns previous value |
| `fetch_xor(val, ord)` | Bitwise XOR, returns previous value |
| `fetch_nand(val, ord)` | Bitwise NAND, returns previous value |

## AtomicBool

`AtomicBool` is useful for flags and simple signaling between threads.

```tml
use std::sync::atomic::{AtomicBool, Ordering}
use std::thread

func main() {
    let shutdown: AtomicBool = AtomicBool::new(false)

    // Worker thread: runs until shutdown is set.
    thread::spawn(do() {
        loop {
            if shutdown.load(Ordering::Acquire) {
                println("Shutting down")
                break
            }
            do_work()
        }
    })

    thread::sleep_ms(500)
    shutdown.store(true, Ordering::Release)
}
```

`AtomicBool::swap` is useful for claiming exclusive one-shot access — only the
first thread that sets the flag from `false` to `true` will see `false`
returned:

```tml
let already_started: AtomicBool = AtomicBool::new(false)

if not already_started.swap(true, Ordering::SeqCst) {
    // This branch runs exactly once across all threads.
    run_initialization()
}
```

## Compare-and-Exchange

Compare-and-exchange (CAS) is the fundamental building block of lock-free
data structures. It compares the current value with an expected value; if they
match, it atomically stores a new value and returns `Ok(old)`. If they do not
match, it returns `Err(current)` without changing anything.

```tml
use std::sync::atomic::{AtomicI32, Ordering}

func main() {
    let counter: AtomicI32 = AtomicI32::new(100)

    // Attempt to change 100 → 200.
    when counter.compare_exchange(100, 200, Ordering::SeqCst, Ordering::SeqCst) {
        Ok(old)     => println("Changed from ", old, " to 200"),
        Err(actual) => println("Expected 100 but found ", actual)
    }

    assert_eq(counter.load(Ordering::SeqCst), 200)
}
```

The two ordering arguments are:
1. The ordering to use if the exchange *succeeds* (at least `Release` for
   publishing updates).
2. The ordering to use if the exchange *fails* (typically `Relaxed` or
   `Acquire` for reading the current value).

### compare_exchange_weak

`compare_exchange_weak` may fail spuriously on some platforms (returning
`Err` even when the value matched) in exchange for potentially better
performance inside retry loops:

```tml
use std::sync::atomic::{AtomicI32, Ordering}

func atomic_increment(counter: ref AtomicI32) {
    var current: I32 = counter.load(Ordering::Relaxed)
    loop {
        when counter.compare_exchange_weak(
            current,
            current + 1,
            Ordering::SeqCst,
            Ordering::Relaxed
        ) {
            Ok(_)       => break,
            Err(actual) => current = actual  // Retry with the real value.
        }
    }
}
```

Use `compare_exchange` when a single attempt must succeed or report a definitive
failure. Use `compare_exchange_weak` inside retry loops for lock-free updates.

## AtomicPtr[T]

`AtomicPtr[T]` stores a raw pointer and allows it to be atomically swapped.
It is the primitive used to implement lock-free linked lists and queues.

```tml
use std::sync::atomic::{AtomicPtr, Ordering}
use core::ptr::Ptr

func main() {
    var value: I32 = 42
    let p: AtomicPtr[I32] = AtomicPtr::new(ref mut value)

    let loaded: Ptr[I32] = p.load(Ordering::SeqCst)
    // loaded points to value.

    let old: Ptr[I32] = p.swap(null, Ordering::SeqCst)
    // old is the previous pointer; p now holds null.
}
```

> **Caution:** `AtomicPtr` bypasses borrow checking. It is a building block
> for advanced data structures; prefer higher-level primitives for
> application code.

## Memory Ordering Patterns

### Relaxed Counter

When you only need the final total and do not care about ordering relative to
any other data, `Relaxed` is sufficient:

```tml
use std::sync::atomic::{AtomicU64, Ordering}

let requests_served: AtomicU64 = AtomicU64::new(0)

// Many threads call this concurrently.
func record_request() {
    requests_served.fetch_add(1, Ordering::Relaxed)
}

// Periodic reporter reads the approximate total.
func log_stats() {
    let total: U64 = requests_served.load(Ordering::Relaxed)
    println("Requests so far: ", total)
}
```

### Release-Acquire Handoff

Use `Release` on the producer store and `Acquire` on the consumer load to
guarantee that all writes made before the release are visible to the consumer
after the acquire:

```tml
use std::sync::atomic::{AtomicBool, Ordering}
use std::thread

func main() {
    let ready: AtomicBool = AtomicBool::new(false)
    var data: I32 = 0

    let producer: JoinHandle[Unit] = thread::spawn(do() {
        data = 42                               // Write the data.
        ready.store(true, Ordering::Release)    // Publish that it is ready.
    })

    // Consumer: spin until ready, then read data.
    loop (not ready.load(Ordering::Acquire)) {
        thread::yield_now()
    }
    // The Acquire load synchronizes with the Release store, so data == 42.
    assert_eq(data, 42)

    producer.join()
}
```

If you used `Relaxed` on both sides here, the consumer could observe the
flag as `true` while still seeing the old value of `data`. This is not a
hypothetical concern — it occurs on weakly-ordered architectures like ARM.

## Fence Operations

A fence provides ordering constraints without accessing a specific atomic
variable. It is useful when you have multiple atomic variables and want to
ensure all writes before the fence are visible to a corresponding acquire
fence elsewhere.

```tml
use std::sync::atomic::{fence, Ordering}

// After all writes, publish them.
fence(Ordering::Release)

// Before reading, ensure we see everything that was released.
fence(Ordering::Acquire)
```

## Spin Loop Hint

When busy-waiting on an atomic flag, tell the CPU that the thread is in a
spin loop. On x86 this emits a `PAUSE` instruction; on ARM it emits `YIELD`.
These hints reduce power consumption and improve performance of the cores
that the spinning thread shares with others.

```tml
use std::sync::atomic::{AtomicBool, spin_loop_hint, Ordering}

let flag: AtomicBool = AtomicBool::new(false)

loop (not flag.load(Ordering::Acquire)) {
    spin_loop_hint()
}
```

## is_lock_free

Not all atomic types use hardware atomic instructions on all platforms.
`is_lock_free()` reports whether the operations are truly lock-free on the
current platform:

```tml
use std::sync::atomic::AtomicI64

let x: AtomicI64 = AtomicI64::new(0)
if x.is_lock_free() {
    println("64-bit atomics are hardware-atomic on this platform")
} else {
    println("64-bit atomics use a fallback mutex on this platform")
}
```

## When to Use Atomics

Atomics are appropriate for:

- **Simple counters**: Incrementing request counts, tracking progress.
- **Flags**: Signaling shutdown, marking initialization complete.
- **Lock-free data structures**: Building queues and stacks as building
  blocks for more complex primitives.
- **Statistics**: Metrics that do not require exact ordering guarantees.

For complex shared state, prefer `Mutex[T]` or channels. They are easier to
reason about, and the compiler prevents entire classes of misuse automatically.

## Common Mistakes

### Using Relaxed for Synchronization

`Relaxed` provides no ordering guarantees relative to other memory operations.
Using it to publish data to another thread is wrong:

```tml
// WRONG: The consumer may see ready == true but data == 0.
data = 42
ready.store(true, Ordering::Relaxed)  // Does NOT guarantee data is visible.
```

```tml
// RIGHT: Release ensures data is visible before the store is seen.
data = 42
ready.store(true, Ordering::Release)
```

### Assuming CAS Always Succeeds Once

Between loading the current value and completing a CAS, another thread may
have changed the value. Always write CAS loops that handle `Err` by retrying
with the updated current value.
