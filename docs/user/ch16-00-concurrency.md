# Concurrency

Concurrent programming lets a program run multiple tasks at the same time,
either by executing them in parallel on multiple CPU cores or by interleaving
them cooperatively. Modern software—web servers, data pipelines, desktop
applications—relies on concurrency to stay responsive and make full use of
available hardware.

TML provides a set of composable, type-safe concurrency primitives modeled
closely on Rust's approach. The compiler enforces thread safety at compile time
using the `Send` and `Sync` marker behaviors, catching entire classes of bugs
before a program ever runs.

## The Concurrency Problem

Without coordination, concurrent access to shared data produces unpredictable
results. Consider a counter incremented from two threads simultaneously:

```
Thread A: read counter (5)
Thread B: read counter (5)
Thread A: write counter (6)
Thread B: write counter (6)   // Lost increment! Should be 7.
```

This is a *data race*. Languages that allow data races make programs that appear
correct in testing but silently corrupt data in production. TML's type system
makes data races a compile-time error.

## TML's Concurrency Primitives

TML organizes its concurrency tools into three conceptual layers:

| Layer | Primitives | Purpose |
|-------|-----------|---------|
| Shared memory | `Mutex[T]`, `RwLock[T]`, `Arc[T]` | Protect and share mutable state |
| Message passing | `Sender[T]`, `Receiver[T]` | Transfer data between threads |
| Low-level | `AtomicI32`, `AtomicBool`, `fence` | Lock-free operations, building blocks |
| Threading | `thread::spawn`, `thread::scope`, `JoinHandle[T]` | Create and manage threads |
| Once-init | `Once`, `OnceLock[T]` | Safe global initialization |
| Coordination | `Barrier`, `Condvar` | Synchronize thread phases |

All of these live in the `std::sync` and `std::thread` modules.

## Thread Safety: Send and Sync

TML enforces thread safety through two marker behaviors that the compiler
checks automatically:

- **`Send`** — a type whose ownership can be transferred to another thread.
  Moving an `Arc[T]` into a spawned thread is only legal because `Arc[T]`
  implements `Send`.

- **`Sync`** — a type whose values can be accessed concurrently through shared
  references. `AtomicI32` is `Sync` because its operations are safe from
  multiple threads at once.

The compiler derives these behaviors automatically for most types. A struct is
`Send` if all its fields are `Send`. A struct is `Sync` if all its fields are
`Sync`. This means that wrapping a non-thread-safe type inside a `Mutex` is
enough to make it shareable: `Mutex[T]` is `Sync` even when `T` is only `Send`.

```tml
use std::sync::Mutex

// Vec[I32] is Send (can be moved to a thread)
// but not Sync (concurrent access is not safe)
// Wrapping it in Mutex makes it Sync
type SharedLog {
    entries: Mutex[Vec[Str]]
}
// SharedLog is now Sync — multiple threads can hold a reference to it
```

When you write code that violates these rules, the compiler tells you:

```
error: type `Ptr[I32]` does not implement `Send`
  --> main.tml:12:5
   |
12 |     thread::spawn(do() { ... use p ... })
   |     ^^^^^^^^^^^^^ closure is not Send
```

## Choosing the Right Tool

The right concurrency primitive depends on the communication pattern:

**Share mutable state** — wrap the value in `Mutex[T]` and then in `Arc[T]`
so multiple threads each hold a counted reference to the same lock:

```tml
let counter: Arc[Mutex[I32]] = Arc::new(Mutex::new(0))
```

**Share read-heavy state** — use `RwLock[T]` instead of `Mutex[T]` so
multiple threads can read simultaneously without blocking each other:

```tml
let config: Arc[RwLock[Config]] = Arc::new(RwLock::new(load()))
```

**Pass data between threads** — use a channel so threads communicate through
message transfer rather than shared memory:

```tml
let (tx, rx): (Sender[Work], Receiver[Work]) = channel[Work]()
```

**Count or flag across threads** — use an atomic type to avoid locking
altogether for simple scalar operations:

```tml
let requests: AtomicI64 = AtomicI64::new(0)
requests.fetch_add(1, Ordering::Relaxed)
```

## A First Example

The following program spawns ten worker threads, each incrementing a shared
counter, then waits for all of them to finish and prints the result.

```tml
use std::sync::{Arc, Mutex}
use std::thread

func main() {
    let counter: Arc[Mutex[I32]] = Arc::new(Mutex::new(0))
    var handles: Vec[JoinHandle[Unit]] = Vec::new()

    loop i in 0 to 10 {
        let c: Arc[Mutex[I32]] = counter.duplicate()
        let handle: JoinHandle[Unit] = thread::spawn(do() {
            let guard: MutexGuard[I32] = c.lock()
            *guard.get_mut() = *guard.get_mut() + 1
            // Guard is dropped here; lock releases automatically.
        })
        handles.push(handle)
    }

    for handle in handles {
        handle.join()
    }

    let guard: MutexGuard[I32] = counter.lock()
    println("Final count: ", *guard.get())  // Final count: 10
}
```

Key points in this example:

- `Arc::new` puts the mutex on the heap with shared ownership.
- `counter.duplicate()` increments the reference count; each thread gets its
  own `Arc` handle to the same underlying `Mutex`.
- `c.lock()` blocks until the mutex is available and returns a `MutexGuard`.
- The guard releases the lock automatically when it goes out of scope (RAII).
- `handle.join()` blocks the main thread until the worker thread finishes.

## What This Chapter Covers

The following sections examine each primitive in depth:

1. **Threads** — spawning threads, named threads, scoped threads, thread pools,
   parallel computation patterns.

2. **Synchronization Primitives** — `Mutex`, `RwLock`, `Condvar`, `Barrier`,
   `Once`, and `OnceLock` for protecting and coordinating shared state.

3. **Channels** — MPSC (multi-producer, single-consumer) channels for
   structured message passing and producer-consumer pipelines.

4. **Atomic Operations** — `AtomicI32`, `AtomicBool`, memory orderings,
   compare-and-exchange, and lock-free patterns.
