# Fearless Concurrency

Concurrent programming—running multiple tasks at the same time—is
increasingly important. TML provides powerful, type-safe primitives for
concurrent programming, inspired by Rust's approach.

## Why Concurrency?

Modern computers have multiple cores. Concurrent programming lets you:

- **Use all cores**: Run computations in parallel
- **Stay responsive**: Handle I/O without blocking
- **Scale better**: Handle more work with the same resources

## TML's Concurrency Model

TML provides several concurrency primitives:

| Feature | Description |
|---------|-------------|
| **Atomic Types** | `AtomicI32`, `AtomicBool`, etc. - Lock-free thread-safe operations |
| **Memory Ordering** | `Ordering` enum for fine-grained synchronization control |
| **Mutex** | `Mutex[T]` - Mutual exclusion locks with RAII guards |
| **RwLock** | `RwLock[T]` - Reader-writer locks for read-heavy workloads |
| **Condvar** | Condition variables for waiting on state changes |
| **Barrier** | Synchronization points for multiple threads |
| **Once/OnceLock** | One-time initialization primitives |
| **Arc** | `Arc[T]` - Atomic reference counting for shared ownership |
| **MPSC Channels** | `Sender[T]`/`Receiver[T]` - Multi-producer, single-consumer message passing |
| **Threads** | `thread::spawn`, `JoinHandle[T]` - Thread management |
| **Scoped Threads** | `thread::scope` - Threads that can borrow from parent scope |

## Thread Safety with Send and Sync

TML's type system helps prevent common concurrency bugs at compile time using
two marker behaviors:

- **`Send`**: Types that can be safely transferred to another thread
- **`Sync`**: Types that can be safely shared between threads (via references)

```tml
use core::marker::{Send, Sync}

// Point is automatically Send + Sync (all fields are primitives)
type Point { x: I32, y: I32 }

// Mutex[T] is Sync even if T is only Send
// This allows shared mutable state
type SharedCounter { count: Mutex[I32] }
```

The compiler automatically derives Send and Sync for types where all fields
implement those behaviors.

## What You'll Learn

In this chapter, you'll learn:

1. **Atomic Operations**: Lock-free primitives with memory ordering
2. **Channels**: Type-safe message passing between threads
3. **Mutex and Synchronization**: Protecting shared mutable data
4. **Arc and Shared Ownership**: Sharing data across threads
5. **Send and Sync**: Understanding thread-safety markers
6. **Thread Management**: Spawning and managing threads

## Quick Example

Here's a taste of TML's concurrency in action:

```tml
use std::sync::{Arc, Mutex}
use std::thread

func main() {
    // Shared counter protected by a mutex, wrapped in Arc for sharing
    let counter: Arc[Mutex[I32]] = Arc::new(Mutex::new(0))

    // Spawn threads to increment the counter
    let handles: Vec[JoinHandle[Unit]] = Vec::new()

    for i in 0 to 10 {
        let counter_clone: Arc[Mutex[I32]] = counter.duplicate()
        let handle: JoinHandle[Unit] = thread::spawn(do() {
            let guard: MutexGuard[I32] = counter_clone.lock()
            *guard.get_mut() = *guard.get_mut() + 1
            // guard automatically released when it goes out of scope
        })
        handles.push(handle)
    }

    // Wait for all threads to finish
    for handle in handles {
        handle.join()
    }

    // Print final count
    let guard: MutexGuard[I32] = counter.lock()
    println(*guard.get())  // 10
}
```
