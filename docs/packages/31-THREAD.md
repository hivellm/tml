# TML Standard Library: Thread

> `std::thread` — Native threads for parallel execution (1:1 threading model).

## Overview

The `std::thread` module provides OS-native threads with a 1:1 threading model. Each TML thread maps directly to an OS thread. Threads can be spawned with closures, joined for their return values, and configured with custom names and stack sizes via `Builder`.

## Import

```tml
use std::thread
use std::thread::{spawn, sleep_ms, current, yield_now, JoinHandle, Builder}
```

---

## Types

### ThreadId

```tml
pub type ThreadId {
    id: U64,
}
```

A unique identifier for a running thread. No two active threads share the same `ThreadId`.

### Thread

```tml
pub type Thread {
    id: ThreadId,
    name: Str,
}
```

A handle to a thread's metadata.

#### Methods

```tml
extend Thread {
    /// Returns the thread's unique identifier.
    pub func id(this) -> ThreadId

    /// Returns the thread's name, if one was assigned.
    pub func name(this) -> Maybe[Str]

    /// Unparks the thread, waking it if it was parked.
    pub func unpark(this)
}
```

### JoinHandle[T]

A handle to a spawned thread, used to join it and retrieve its return value.

```tml
extend JoinHandle[T] {
    /// Blocks until the thread finishes and returns its result.
    pub func join(this) -> Outcome[T, JoinError]

    /// Returns true if the thread has finished executing.
    pub func is_finished(this) -> Bool

    /// Returns a reference to the underlying Thread.
    pub func thread(this) -> Thread
}
```

### JoinError

```tml
pub enum JoinError {
    AlreadyJoined,
    OsError(I32),
    NoResult,
    Panicked,
}
```

### SpawnError

```tml
pub enum SpawnError {
    Unsupported,
}
```

Returned when thread spawning fails. Currently `Builder::spawn` returns `Err(SpawnError::Unsupported)` as thread spawning is work-in-progress.

### Builder

Configures and spawns threads with custom settings.

```tml
extend Builder {
    /// Creates a new Builder with default settings.
    pub func new() -> Builder

    /// Sets the name of the thread.
    pub func name(this, name: Str) -> Builder

    /// Sets the stack size in bytes.
    pub func stack_size(this, size: U64) -> Builder

    /// Spawns a new thread with the configured settings.
    /// NOTE: Currently returns Err(SpawnError::Unsupported).
    pub func spawn[T](this, f: func() -> T) -> Outcome[JoinHandle[T], SpawnError]
}
```

---

## Free Functions

```tml
/// Spawns a new thread running the given closure, returning a JoinHandle.
pub func spawn[T](f: func() -> T) -> JoinHandle[T]

/// Returns a handle to the current thread.
pub func current() -> Thread

/// Yields execution to the OS scheduler.
pub func yield_now()

/// Sleeps the current thread for the given number of milliseconds.
pub func sleep_ms(ms: U64)

/// Parks the current thread until unparked by another thread.
pub func park()

/// Returns the number of hardware threads available.
pub func available_parallelism() -> U32
```

---

## Example

```tml
use std::thread::{spawn, sleep_ms, current}

func main() {
    let name = current().name()
    print("Main thread: {name}\n")

    let handle = spawn[I32](func() -> I32 {
        sleep_ms(100)
        return 42
    })

    let result = handle.join()
    when result {
        Ok(value) -> print("Thread returned: {value}\n"),
        Err(e) -> print("Thread failed\n"),
    }

    let cores = std::thread::available_parallelism()
    print("Available parallelism: {cores}\n")
}
```

---

## See Also

- [std::sync](./14-ASYNC.md) — Synchronization primitives
- [std::collections](./10-COLLECTIONS.md) — Thread-safe collection wrappers
