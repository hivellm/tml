# Synchronization Primitives

When multiple threads need to read or modify shared data, they must coordinate
so that only one thread performs a potentially destructive operation at a time.
TML's synchronization primitives provide this coordination using RAII guards
that automatically release their locks when they go out of scope, making it
difficult to accidentally leave a lock held.

All synchronization primitives live in `std::sync`.

## Mutex[T]

`Mutex[T]` (mutual exclusion) protects a value of type `T` behind a lock.
Only the thread that successfully acquires the lock may access the value.
All other threads block at `lock()` until the lock is released.

### Basic Usage

```tml
use std::sync::Mutex

func main() {
    let counter: Mutex[I32] = Mutex::new(0)

    // Acquire the lock. Blocks if another thread holds it.
    let guard: MutexGuard[I32] = counter.lock()

    // Read and modify through the guard.
    let current: I32 = *guard.get()
    *guard.get_mut() = current + 1

    // The lock releases here when guard goes out of scope.
}
```

### MutexGuard

`MutexGuard[T]` is the RAII wrapper returned by `lock()`. It provides two
accessors:

| Method | Returns | Use |
|--------|---------|-----|
| `get()` | `ref T` | Read the protected value |
| `get_mut()` | `mut ref T` | Modify the protected value |

The lock is held for the entire lifetime of the guard. Dropping the guard—or
calling `drop(guard)` explicitly—releases the lock immediately.

```tml
use std::sync::Mutex

func main() {
    let data: Mutex[I32] = Mutex::new(100)

    {
        let guard: MutexGuard[I32] = data.lock()
        *guard.get_mut() = 200
        // Guard dropped here; lock released.
    }

    // Lock is available again.
    let guard2: MutexGuard[I32] = data.lock()
    assert_eq(*guard2.get(), 200)
}
```

### try_lock

`try_lock()` attempts to acquire the lock without blocking. It returns
`Just(MutexGuard[T])` on success or `Nothing` if the lock is already held.

```tml
use std::sync::Mutex

func main() {
    let m: Mutex[I32] = Mutex::new(42)
    let guard: MutexGuard[I32] = m.lock()

    // The lock is currently held, so try_lock returns Nothing.
    when m.try_lock() {
        Just(g) => println("Acquired: ", *g.get()),
        Nothing => println("Lock is busy")  // This branch runs.
    }

    drop(guard)

    when m.try_lock() {
        Just(g) => println("Acquired: ", *g.get()),  // Acquired: 42
        Nothing => println("Lock is busy")
    }
}
```

### Sharing Across Threads with Arc

A `Mutex` on its own is not useful across threads because it cannot be moved
into multiple threads simultaneously. Wrap it in `Arc` to give each thread
its own counted reference to the same mutex:

```tml
use std::sync::{Arc, Mutex}
use std::thread

func main() {
    let counter: Arc[Mutex[I32]] = Arc::new(Mutex::new(0))
    var handles: Vec[JoinHandle[Unit]] = Vec::new()

    loop i in 0 to 5 {
        let c: Arc[Mutex[I32]] = counter.duplicate()
        handles.push(thread::spawn(do() {
            let guard: MutexGuard[I32] = c.lock()
            *guard.get_mut() = *guard.get_mut() + 1
        }))
    }

    for h in handles { h.join() }

    let guard: MutexGuard[I32] = counter.lock()
    assert_eq(*guard.get(), 5)
}
```

### into_inner

Consume the mutex and extract the protected value when you have sole ownership
and no locks are held:

```tml
use std::sync::Mutex

func main() {
    let m: Mutex[I32] = Mutex::new(42)
    let value: I32 = m.into_inner()
    println(value)  // 42
}
```

## RwLock[T]

`RwLock[T]` (reader-writer lock) allows concurrent reads but exclusive writes.
Use it when your workload reads far more often than it writes, and when holding
all readers out during every write operation would create unnecessary contention.

The contract is:
- Any number of threads may hold a **read lock** simultaneously.
- Only one thread may hold a **write lock**, and only when no read locks exist.

### Multiple Readers

```tml
use std::sync::RwLock

func main() {
    let config: RwLock[I32] = RwLock::new(42)

    // Two read locks coexist; neither blocks the other.
    let r1: RwLockReadGuard[I32] = config.read()
    let r2: RwLockReadGuard[I32] = config.read()

    assert_eq(*r1.get(), 42)
    assert_eq(*r2.get(), 42)
}
```

### Exclusive Writer

```tml
use std::sync::RwLock

func main() {
    let config: RwLock[I32] = RwLock::new(0)

    {
        let w: RwLockWriteGuard[I32] = config.write()
        *w.get_mut() = 42
        // Write lock released here.
    }

    let r: RwLockReadGuard[I32] = config.read()
    assert_eq(*r.get(), 42)
}
```

### try_read and try_write

Both `try_read()` and `try_write()` are non-blocking variants that return
`Nothing` if the lock cannot be acquired immediately.

```tml
use std::sync::RwLock

func main() {
    let data: RwLock[I32] = RwLock::new(10)

    // Obtain a write lock.
    let w: RwLockWriteGuard[I32] = data.write()

    // A read attempt while writing fails.
    when data.try_read() {
        Just(r) => println(*r.get()),
        Nothing => println("Write lock is held")  // This runs.
    }

    drop(w)

    when data.try_read() {
        Just(r) => println(*r.get()),  // 10
        Nothing => println("Blocked")
    }
}
```

### When to Prefer RwLock over Mutex

Use `RwLock` when:
- Reads dominate writes (10-to-1 or more).
- Read operations take significant time, making readers waiting for each
  other an actual throughput bottleneck.

Prefer `Mutex` when:
- Writes are frequent, so readers would rarely coexist anyway.
- Simplicity is more important than maximum read throughput.

## Condvar

A condition variable (`Condvar`) allows a thread to release a mutex lock
and wait for a signal from another thread, then re-acquire the lock before
proceeding. `Condvar` is always paired with a `Mutex`.

### Basic Pattern

```tml
use std::sync::{Arc, Mutex, Condvar}
use std::thread

func main() {
    let pair: Arc[(Mutex[Bool], Condvar)] =
        Arc::new((Mutex::new(false), Condvar::new()))

    let pair2: Arc[(Mutex[Bool], Condvar)> = pair.duplicate()

    // Waiter thread: blocks until the flag is true.
    thread::spawn(do() {
        let (lock, cv) = pair2.get()
        var guard: MutexGuard[Bool] = lock.lock()
        loop (not *guard.get()) {
            guard = cv.wait(guard)  // Releases lock while waiting.
        }
        println("Condition met!")
    })

    // Notifier: sets the flag and signals the waiter.
    let (lock, cv) = pair.get()
    thread::sleep_ms(100)
    let guard: MutexGuard[Bool] = lock.lock()
    *guard.get_mut() = true
    cv.notify_one()
}
```

### wait_while

`wait_while` accepts a predicate closure and keeps waiting as long as the
predicate returns true. It is cleaner than a manual `loop`:

```tml
use std::sync::{Mutex, Condvar}

func wait_for_threshold(count: ref Mutex[I32], cv: ref Condvar, threshold: I32) {
    let guard: MutexGuard[I32] = count.lock()
    let _guard: MutexGuard[I32] = cv.wait_while(guard, do(v: ref I32) -> Bool {
        return *v < threshold
    })
    println("Threshold reached")
}
```

### wait_timeout_ms

Wait with a maximum duration to avoid blocking forever if a signal is lost:

```tml
use std::sync::{Mutex, Condvar}

func main() {
    let flag: Mutex[Bool] = Mutex::new(false)
    let cv: Condvar = Condvar::new()

    let guard: MutexGuard[Bool] = flag.lock()
    let (_, timed_out): (MutexGuard[Bool], Bool) = cv.wait_timeout_ms(guard, 500)

    if timed_out {
        println("Nobody signaled within 500 ms")
    } else {
        println("Signaled in time")
    }
}
```

### notify_one and notify_all

| Method | Effect |
|--------|--------|
| `cv.notify_one()` | Wakes one waiting thread (if any) |
| `cv.notify_all()` | Wakes all waiting threads |

Use `notify_one` when exactly one thread should respond (e.g., one item was
added to a queue). Use `notify_all` when all waiting threads should re-check
a condition (e.g., a shutdown flag was set).

## Barrier

A `Barrier` is a meeting point that lets a fixed number of threads wait until
all of them have arrived before any of them proceeds. It is useful for
phased parallel algorithms where all threads must finish phase N before any
thread starts phase N+1.

```tml
use std::sync::{Arc, Barrier}
use std::thread

func main() {
    let n: I32 = 4
    let barrier: Arc[Barrier] = Arc::new(Barrier::new(n))
    var handles: Vec[JoinHandle[Unit]] = Vec::new()

    loop i in 0 to n {
        let b: Arc[Barrier] = barrier.duplicate()
        handles.push(thread::spawn(do() {
            println("Thread ", i, " — phase 1 complete")

            // All four threads block here until all arrive.
            let result: BarrierWaitResult = b.wait()

            if result.is_leader() {
                // Exactly one thread is designated leader.
                println("Leader starting phase 2 setup")
            }

            println("Thread ", i, " — phase 2 running")
        }))
    }

    for h in handles { h.join() }
}
```

`BarrierWaitResult::is_leader()` returns true for exactly one of the threads
that crossed the barrier. Use it to perform one-time setup work between phases.

## Once and OnceLock

### Once

`Once` guarantees that a given block of code runs exactly once, even if
multiple threads attempt to execute it simultaneously. Subsequent calls do
nothing.

```tml
use std::sync::Once

let INIT: Once = Once::new()

func initialize_globals() {
    INIT.call_once(do() {
        // This closure runs exactly once across all threads.
        println("Global initialization")
        setup_logging()
        load_config()
    })
}
```

`is_completed()` returns `true` after the closure has finished running:

```tml
if INIT.is_completed() {
    println("Already initialized")
}
```

### OnceLock[T]

`OnceLock[T]` is a lazily-initialized cell. The first call to
`get_or_init` runs the initializer and stores the result; every subsequent
call returns the cached value.

```tml
use std::sync::OnceLock

let CONFIG: OnceLock[AppConfig] = OnceLock::new[AppConfig]()

func get_config() -> ref AppConfig {
    return CONFIG.get_or_init(do() -> AppConfig {
        println("Loading config (runs once)")
        return AppConfig::load()
    })
}
```

`OnceLock::set` explicitly stores a value, failing if the cell is already
filled:

```tml
use std::sync::OnceLock

func main() {
    let cell: OnceLock[I32] = OnceLock::new[I32]()

    when cell.set(42) {
        Ok(_) => println("Set successfully"),
        Err(v) => println("Already set; tried to set ", v)
    }

    // cell.get() returns Maybe[ref T]
    when cell.get() {
        Just(v) => println("Value: ", *v),
        Nothing => println("Not yet initialized")
    }
}
```

## Choosing the Right Primitive

| Situation | Best choice |
|-----------|------------|
| One writer, multiple readers infrequent | `Mutex[T]` |
| Many concurrent readers, rare writes | `RwLock[T]` |
| Thread must wait for another thread's action | `Condvar` with `Mutex` |
| All threads must complete a phase before proceeding | `Barrier` |
| One-time global initialization | `Once` or `OnceLock[T]` |
| Passing data between threads | Channels (see chapter 16-03) |

## Avoiding Deadlocks

A deadlock occurs when thread A holds lock X and waits for lock Y, while
thread B holds lock Y and waits for lock X. Both threads block forever.

**Rule 1: Always acquire locks in the same global order.**

```tml
// BAD: Thread 1 acquires A then B; Thread 2 acquires B then A.
// These can deadlock.
let guard_a = mutex_a.lock()
let guard_b = mutex_b.lock()

// GOOD: Both threads acquire A before B.
let guard_a = mutex_a.lock()
let guard_b = mutex_b.lock()
```

**Rule 2: Release locks before blocking.**

```tml
// BAD: Holding a lock while blocking on recv prevents other threads
// from making progress.
let guard = shared.lock()
let msg = channel.recv()   // Can block for a long time.

// GOOD: Release the lock, receive, re-acquire.
drop(guard)
let msg = channel.recv()
let guard = shared.lock()
```

**Rule 3: Use try_lock in time-sensitive code.**

When a full block would be harmful, `try_lock()` lets you fall back to
an alternative path rather than risk a deadlock.

## Thread Safety Rules for Guards

Guard types hold a mutex lock. Sending a guard to another thread would allow
that thread to release a lock it did not acquire, which is incorrect. The
compiler prevents this:

- `MutexGuard[T]` is neither `Send` nor `Sync`.
- `RwLockReadGuard[T]` is neither `Send` nor `Sync`.
- `RwLockWriteGuard[T]` is neither `Send` nor `Sync`.

This means guards must be acquired, used, and released within the same thread.
