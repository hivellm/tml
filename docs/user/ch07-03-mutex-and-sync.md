# Mutex and Synchronization Primitives

When you need multiple threads to access shared mutable data, TML provides
synchronization primitives that ensure thread safety.

## Mutex[T]

A `Mutex[T]` (mutual exclusion) ensures only one thread can access the
protected data at a time.

### Basic Usage

```tml
use std::sync::Mutex

func main() {
    // Create a mutex protecting an I32
    let counter: Mutex[I32] = Mutex::new(0)

    // Lock the mutex to access the data
    let guard: MutexGuard[I32] = counter.lock()

    // Access the data through the guard
    let value: I32 = *guard.get()
    println("Value: ", value)

    // Modify the data
    *guard.get_mut() = 42

    // Lock is automatically released when guard goes out of scope
}
```

### MutexGuard

`MutexGuard[T]` is an RAII type that:
- Holds the lock while it exists
- Provides access to the protected data
- Automatically releases the lock when dropped

```tml
use std::sync::Mutex

func main() {
    let data: Mutex[I32] = Mutex::new(100)

    {
        let guard: MutexGuard[I32] = data.lock()
        *guard.get_mut() = 200
        // guard dropped here, lock released
    }

    // Can lock again
    let guard2: MutexGuard[I32] = data.lock()
    assert_eq(*guard2.get(), 200)
}
```

### try_lock

Non-blocking attempt to acquire the lock:

```tml
use std::sync::Mutex

func main() {
    let m: Mutex[I32] = Mutex::new(42)

    // First lock succeeds
    let guard: MutexGuard[I32] = m.lock()

    // try_lock returns Nothing because lock is held
    when m.try_lock() {
        Just(g) => println("Got lock"),
        Nothing => println("Lock busy")  // This runs
    }

    // Drop first guard
    drop(guard)

    // Now try_lock succeeds
    when m.try_lock() {
        Just(g) => println("Got lock: ", *g.get()),  // Got lock: 42
        Nothing => println("Lock busy")
    }
}
```

### into_inner

Consume the mutex and extract the value (only works when no locks are held):

```tml
use std::sync::Mutex

func main() {
    let m: Mutex[I32] = Mutex::new(42)
    let value: I32 = m.into_inner()
    assert_eq(value, 42)
}
```

## RwLock[T]

A reader-writer lock allows multiple concurrent readers OR one exclusive
writer. Use this when reads are much more common than writes.

### Multiple Readers

```tml
use std::sync::RwLock

func main() {
    let data: RwLock[I32] = RwLock::new(42)

    // Multiple readers can hold the lock simultaneously
    let r1: RwLockReadGuard[I32] = data.read()
    let r2: RwLockReadGuard[I32] = data.read()

    assert_eq(*r1.get(), 42)
    assert_eq(*r2.get(), 42)

    // Both reading the same data concurrently
}
```

### Exclusive Writer

```tml
use std::sync::RwLock

func main() {
    let data: RwLock[I32] = RwLock::new(0)

    // Write lock is exclusive
    let w: RwLockWriteGuard[I32] = data.write()
    *w.get_mut() = 42

    // Cannot get another read or write lock while w exists
    // w is dropped here

    // Now can read
    let r: RwLockReadGuard[I32] = data.read()
    assert_eq(*r.get(), 42)
}
```

### try_read and try_write

Non-blocking attempts:

```tml
use std::sync::RwLock

func main() {
    let data: RwLock[I32] = RwLock::new(42)

    let w: RwLockWriteGuard[I32] = data.write()

    // try_read fails because write lock is held
    when data.try_read() {
        Just(r) => println("Got read lock"),
        Nothing => println("Blocked by writer")  // This runs
    }

    drop(w)

    // Now try_read succeeds
    when data.try_read() {
        Just(r) => println("Value: ", *r.get()),  // Value: 42
        Nothing => println("Blocked")
    }
}
```

## Condvar (Condition Variable)

Condition variables let threads wait for a condition to become true,
while releasing their lock.

### Basic Pattern

```tml
use std::sync::{Mutex, Condvar}

func main() {
    let ready: Mutex[Bool] = Mutex::new(false)
    let cv: Condvar = Condvar::new()

    // Thread 1: Wait for condition
    let guard: MutexGuard[Bool] = ready.lock()
    loop (not *guard.get()) {
        guard = cv.wait(guard)  // Releases lock while waiting
    }
    println("Condition met!")

    // Thread 2: Signal condition
    let guard: MutexGuard[Bool] = ready.lock()
    *guard.get_mut() = true
    cv.notify_one()  // Wake up one waiter
}
```

### wait_while

Wait while a condition is true:

```tml
use std::sync::{Mutex, Condvar}

func main() {
    let count: Mutex[I32] = Mutex::new(0)
    let cv: Condvar = Condvar::new()

    // Wait while count < 5
    let guard: MutexGuard[I32] = count.lock()
    let guard: MutexGuard[I32] = cv.wait_while(guard, do(value: ref I32) -> Bool {
        return *value < 5
    })
    println("Count reached 5!")
}
```

### wait_timeout_ms

Wait with a timeout:

```tml
use std::sync::{Mutex, Condvar}

func main() {
    let flag: Mutex[Bool] = Mutex::new(false)
    let cv: Condvar = Condvar::new()

    let guard: MutexGuard[Bool] = flag.lock()
    let (new_guard, timed_out): (MutexGuard[Bool], Bool) = cv.wait_timeout_ms(guard, 1000)

    if timed_out {
        println("Timed out after 1 second")
    } else {
        println("Condition was signaled")
    }
}
```

### notify_one vs notify_all

```tml
use std::sync::Condvar

let cv: Condvar = Condvar::new()

cv.notify_one()  // Wake up one waiting thread
cv.notify_all()  // Wake up all waiting threads
```

## Barrier

A barrier synchronizes multiple threads at a specific point:

```tml
use std::sync::Barrier

func main() {
    // Barrier for 3 threads
    let barrier: Barrier = Barrier::new(3)

    // Each thread calls wait()
    // All threads block until all 3 have called wait()
    let result: BarrierWaitResult = barrier.wait()

    if result.is_leader() {
        // One thread is designated the "leader"
        println("I'm the leader!")
    }
}
```

### Use Case: Parallel Computation Phases

```tml
use std::sync::Barrier
use std::thread

func main() {
    let barrier: Barrier = Barrier::new(3)

    for i in 0 to 3 {
        let b: Barrier = barrier.duplicate()  // Clone for each thread
        thread::spawn(do() {
            // Phase 1: All threads do work
            println("Thread ", i, " phase 1")

            // Wait for all threads to finish phase 1
            b.wait()

            // Phase 2: Continue together
            println("Thread ", i, " phase 2")
        })
    }
}
```

## Once and OnceLock

### Once

Execute code exactly once, even across multiple threads:

```tml
use std::sync::Once

func main() {
    let init: Once = Once::new()

    // Multiple threads can call this, but the closure runs only once
    init.call_once(do() {
        println("Initializing...")  // Prints exactly once
    })

    // Check if initialization completed
    if init.is_completed() {
        println("Already initialized")
    }
}
```

### OnceLock[T]

Lazy initialization of a value:

```tml
use std::sync::OnceLock

func main() {
    let config: OnceLock[I32] = OnceLock::new[I32]()

    // First call initializes
    let value: ref I32 = config.get_or_init(do() -> I32 {
        println("Computing...")  // Runs once
        return 42
    })
    assert_eq(*value, 42)

    // Subsequent calls return cached value
    let value2: ref I32 = config.get_or_init(do() -> I32 {
        println("This never runs")
        return 0
    })
    assert_eq(*value2, 42)
}
```

### OnceLock::set

Explicitly set the value (fails if already set):

```tml
use std::sync::OnceLock

func main() {
    let cell: OnceLock[I32] = OnceLock::new[I32]()

    // First set succeeds
    when cell.set(42) {
        Ok(_) => println("Set to 42"),
        Err(v) => println("Already set")
    }

    // Second set fails
    when cell.set(100) {
        Ok(_) => println("Set to 100"),
        Err(v) => println("Already set, tried to set: ", v)  // This runs
    }

    assert_eq(*cell.get().unwrap(), 42)
}
```

## Choosing the Right Primitive

| Scenario | Use |
|----------|-----|
| Single shared mutable value | `Mutex[T]` |
| Many readers, few writers | `RwLock[T]` |
| Wait for condition | `Condvar` |
| Synchronize at a point | `Barrier` |
| One-time initialization | `Once` or `OnceLock[T]` |
| Pass data between threads | Channels (see previous chapter) |

## Thread Safety Rules

Guard types cannot be sent between threads:

- `MutexGuard[T]` is NOT `Send` - cannot cross thread boundaries
- `RwLockReadGuard[T]` is NOT `Send`
- `RwLockWriteGuard[T]` is NOT `Send`

This prevents accidentally unlocking from a different thread.

## Avoiding Deadlocks

### Lock in Consistent Order

```tml
// BAD: Thread 1 locks A then B, Thread 2 locks B then A
// Thread 1: lock(a), lock(b)
// Thread 2: lock(b), lock(a)  // Deadlock!

// GOOD: Always lock in same order
// Thread 1: lock(a), lock(b)
// Thread 2: lock(a), lock(b)  // No deadlock
```

### Don't Hold Locks While Blocking

```tml
// BAD: Holding lock while waiting on channel
let guard = mutex.lock()
let value = channel.recv()  // Blocks! Other threads can't get lock

// GOOD: Release lock before blocking
drop(guard)
let value = channel.recv()
let guard = mutex.lock()
```

### Use Timeouts

```tml
// Avoid infinite wait
when mutex.try_lock() {
    Just(guard) => {
        // Got the lock
    },
    Nothing => {
        // Lock busy, try alternative approach
    }
}
```
