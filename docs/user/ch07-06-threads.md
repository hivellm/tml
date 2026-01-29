# Thread Management

TML provides comprehensive thread management through the `std::thread`
module. This chapter covers spawning threads, joining, and thread utilities.

## Spawning Threads

### Basic Spawn

Use `thread::spawn` to create a new thread:

```tml
use std::thread

func main() {
    // Spawn a thread that prints a message
    let handle: JoinHandle[Unit] = thread::spawn(do() {
        println("Hello from a thread!")
    })

    // Wait for the thread to finish
    handle.join()
    println("Thread finished")
}
```

### Returning Values

Threads can return values:

```tml
use std::thread

func main() {
    let handle: JoinHandle[I32] = thread::spawn(do() -> I32 {
        // Do some computation
        let result: I32 = expensive_computation()
        return result
    })

    // Get the result
    when handle.join() {
        Ok(value) => println("Result: ", value),
        Err(e) => println("Thread panicked")
    }
}
```

### Thread Builder

Use `Builder` for more control over thread creation:

```tml
use std::thread::Builder

func main() {
    let builder: Builder = Builder::new()
        .name("worker-1")
        .stack_size(1024 * 1024)  // 1MB stack

    when builder.spawn(do() -> I32 {
        println("Named thread running")
        return 42
    }) {
        Ok(handle) => {
            let result: I32 = handle.join().unwrap()
            println("Result: ", result)
        },
        Err(e) => println("Failed to spawn thread")
    }
}
```

## JoinHandle

`JoinHandle[T]` represents a handle to a spawned thread.

### Joining

`join()` waits for the thread to finish and returns its result:

```tml
use std::thread

func main() {
    let handle: JoinHandle[I32] = thread::spawn(do() -> I32 {
        return 42
    })

    // Blocks until thread completes
    when handle.join() {
        Ok(value) => println("Got: ", value),
        Err(_) => println("Thread panicked")
    }
}
```

### Checking Completion

Check if a thread has finished without blocking:

```tml
use std::thread

func main() {
    let handle: JoinHandle[Unit] = thread::spawn(do() {
        thread::sleep_ms(1000)
    })

    // Non-blocking check
    if handle.is_finished() {
        println("Thread done")
    } else {
        println("Thread still running")
    }

    handle.join()
}
```

### Getting Thread Information

```tml
use std::thread

func main() {
    let handle: JoinHandle[Unit] = thread::spawn(do() {
        // ...
    })

    // Get the Thread from the handle
    let t: Thread = handle.thread()
    println("Thread ID: ", t.id().as_u64())
}
```

## Thread Utilities

### Current Thread

Get information about the current thread:

```tml
use std::thread

func main() {
    let current: Thread = thread::current()

    // Thread ID
    let id: ThreadId = current.id()
    println("My thread ID: ", id.as_u64())

    // Thread name (if set)
    when current.name() {
        Just(name) => println("My name: ", name),
        Nothing => println("No name set")
    }
}
```

### Sleeping

Pause the current thread:

```tml
use std::thread

func main() {
    println("Starting...")

    // Sleep for 1 second
    thread::sleep_ms(1000)

    println("1 second later...")

    // Sleep for 500 milliseconds
    thread::sleep_ms(500)

    println("Done")
}
```

### Yielding

Hint to the scheduler to run other threads:

```tml
use std::thread

func main() {
    // In a busy loop, yield to let other threads run
    for i in 0 to 1000 {
        do_some_work(i)
        thread::yield_now()
    }
}
```

### Available Parallelism

Query the number of available CPU cores:

```tml
use std::thread

func main() {
    let cores: U32 = thread::available_parallelism()
    println("Available cores: ", cores)

    // Use this to size thread pools
    let pool_size: U32 = cores
}
```

## Scoped Threads

Scoped threads can borrow data from the parent scope without requiring
`'static` lifetimes:

```tml
use std::thread::scope

func main() {
    let data: I32 = 42
    let results: Vec[I32] = Vec::new()

    scope(do(s: mut ref Scope) {
        // Spawn threads that borrow from parent scope
        s.spawn(do() {
            println("Data: ", data)  // Can borrow data!
        })

        s.spawn(do() {
            println("Also see data: ", data)
        })

        // All scoped threads join here automatically
    })

    println("All threads finished")
}
```

### Scoped Thread with Return Value

```tml
use std::thread::scope

func main() {
    let numbers: Vec[I32] = vec![1, 2, 3, 4, 5]

    let sum: I32 = scope(do(s: mut ref Scope) -> I32 {
        let handle: ScopedJoinHandle[I32] = s.spawn(do() -> I32 {
            var total: I32 = 0
            for n in numbers {  // Borrows numbers
                total = total + n
            }
            return total
        })

        return handle.join()
    })

    println("Sum: ", sum)  // Sum: 15
}
```

### Why Scoped Threads?

Regular threads require `'static` data—the data must live forever or be
moved into the thread. Scoped threads allow borrowing because:

1. The `scope()` function doesn't return until all spawned threads finish
2. This guarantees borrowed data outlives the threads

```tml
// Regular spawn: must move data
let data: Vec[I32] = vec![1, 2, 3]
thread::spawn(do() {
    // data must be moved or 'static
    println(data.len())
})

// Scoped spawn: can borrow
let data: Vec[I32] = vec![1, 2, 3]
scope(do(s: mut ref Scope) {
    s.spawn(do() {
        // data is borrowed, not moved
        println(data.len())
    })
})  // data still available here
```

## Thread Parking

Park and unpark threads for custom synchronization:

```tml
use std::thread

func main() {
    let t: Thread = thread::current()

    let handle: JoinHandle[Unit] = thread::spawn(do() {
        // Do some work...

        // Unpark the main thread
        t.unpark()
    })

    // Park until unparked
    thread::park()

    println("Was unparked!")
    handle.join()
}
```

### Park with Timeout

```tml
use std::thread

func main() {
    // Park for at most 1 second
    thread::park_timeout_ms(1000)

    println("Woke up (either unparked or timed out)")
}
```

## Common Patterns

### Worker Pool

Create a pool of worker threads:

```tml
use std::sync::{Arc, Mutex}
use std::sync::mpsc::{channel, Sender, Receiver}
use std::thread

type Job = do()

func main() {
    let (tx, rx): (Sender[Job], Receiver[Job]) = channel[Job]()
    let rx: Arc[Mutex[Receiver[Job]]] = Arc::new(Mutex::new(rx))

    let num_workers: U32 = thread::available_parallelism()
    let workers: Vec[JoinHandle[Unit]] = Vec::new()

    for i in 0 to num_workers {
        let rx: Arc[Mutex[Receiver[Job]]] = rx.duplicate()
        let handle: JoinHandle[Unit] = thread::spawn(do() {
            loop {
                let guard: MutexGuard[Receiver[Job]] = rx.lock()
                when guard.get().recv() {
                    Ok(job) => {
                        drop(guard)  // Release lock before running job
                        job()
                    },
                    Err(_) => break  // Channel closed
                }
            }
        })
        workers.push(handle)
    }

    // Submit jobs
    for i in 0 to 100 {
        tx.send(do() {
            println("Job ", i)
        })
    }

    // Close channel and wait for workers
    drop(tx)
    for worker in workers {
        worker.join()
    }
}
```

### Parallel Map

Process items in parallel:

```tml
use std::thread::scope

func parallel_map[T: Send, R: Send](items: ref Vec[T], f: func(ref T) -> R) -> Vec[R] {
    let results: Vec[R] = Vec::with_capacity(items.len())

    scope(do(s: mut ref Scope) {
        for item in items {
            let handle: ScopedJoinHandle[R] = s.spawn(do() -> R {
                return f(item)
            })
            results.push(handle.join())
        }
    })

    return results
}

func main() {
    let numbers: Vec[I32] = vec![1, 2, 3, 4, 5]
    let squares: Vec[I32] = parallel_map(ref numbers, do(n: ref I32) -> I32 {
        return (*n) * (*n)
    })
    // squares = [1, 4, 9, 16, 25]
}
```

## Best Practices

1. **Always join threads** - Detached threads can cause problems if the
   main thread exits first

2. **Use scoped threads when possible** - They're safer and allow borrowing

3. **Size thread pools to available cores** - Use `available_parallelism()`

4. **Yield in busy loops** - Prevent starving other threads

5. **Handle join errors** - Threads can panic; handle it gracefully

6. **Prefer channels over shared state** - Easier to reason about

## Thread Safety Reminder

- Closures passed to `spawn` must be `Send`
- Return values must be `Send`
- Use `Arc[Mutex[T]]` for shared mutable state
- Use `Arc[T]` for shared read-only state
- See the Send/Sync chapter for details
