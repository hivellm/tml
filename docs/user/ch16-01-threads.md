# Threads

A thread is an independently executing sequence of instructions within a
process. All threads in a process share the same address space, which is
both their power—shared data is accessible without copying—and their
primary hazard, since unsynchronized access causes data races.

TML exposes thread management through the `std::thread` module. The module's
types and functions map closely to operating-system threads, giving predictable
performance characteristics with compile-time safety guarantees.

## Spawning a Thread

`thread::spawn` creates a new thread and runs the closure you provide. It
returns a `JoinHandle[T]`, where `T` is the type the closure returns.

```tml
use std::thread

func main() {
    let handle: JoinHandle[Unit] = thread::spawn(do() {
        println("Hello from a worker thread!")
    })

    // The main thread and the worker thread run concurrently here.

    handle.join()  // Wait for the worker to finish.
    println("Worker finished.")
}
```

The closure passed to `spawn` must satisfy `Send` — all values it captures must
be safe to transfer across thread boundaries. The compiler enforces this.

### Returning a Value

Threads can compute and return values. `join()` blocks the calling thread until
the spawned thread exits and returns an `Outcome` containing the result.

```tml
use std::thread

func main() {
    let handle: JoinHandle[I32] = thread::spawn(do() -> I32 {
        let result: I32 = heavy_computation()
        return result
    })

    when handle.join() {
        Ok(value) => println("Computed: ", value),
        Err(_)    => println("Thread panicked")
    }
}
```

### Parallel Computation

Spawn multiple threads to divide a workload across CPU cores:

```tml
use std::thread

func sum_range(start: I32, end: I32) -> I32 {
    var total: I32 = 0
    loop i in start to end {
        total = total + i
    }
    return total
}

func main() {
    // Split 0..1000 into two halves, compute in parallel.
    let h1: JoinHandle[I32] = thread::spawn(do() -> I32 {
        return sum_range(0, 500)
    })
    let h2: JoinHandle[I32] = thread::spawn(do() -> I32 {
        return sum_range(500, 1000)
    })

    let part1: I32 = h1.join().unwrap()
    let part2: I32 = h2.join().unwrap()
    println("Total: ", part1 + part2)
}
```

## Thread Builder

For more control over thread creation, use `Thread::Builder`. The builder lets
you assign a name (useful for debugging and profiling) and set the stack size.

```tml
use std::thread::Thread

func main() {
    let handle: JoinHandle[I32] = Thread::Builder()
        .name("number-cruncher")
        .stack_size(2 * 1024 * 1024)  // 2 MB stack
        .spawn(do() -> I32 {
            println("Running on a named thread")
            return 42
        })

    let result: I32 = handle.join().unwrap()
    println("Result: ", result)
}
```

`Thread::Builder::spawn` returns an `Outcome` in case thread creation fails
(for example, when the OS exhausts thread handles). The plain `thread::spawn`
panics on failure instead.

## JoinHandle

`JoinHandle[T]` is the handle to a spawned thread. Keep the handle if you need
to wait for the result or check the thread's status; let it drop if you want
the thread to run independently until the process exits.

### join

`join()` blocks the calling thread until the target thread finishes. It returns
`Ok(T)` with the thread's return value, or `Err` if the thread panicked.

```tml
let handle: JoinHandle[I32] = thread::spawn(do() -> I32 { return 7 })
let value: I32 = handle.join().unwrap()
```

### is_finished

`is_finished()` checks whether the thread has exited without blocking. Use it
in polling loops where you want to do other work while waiting.

```tml
use std::thread

func main() {
    let handle: JoinHandle[Unit] = thread::spawn(do() {
        thread::sleep_ms(500)
    })

    loop {
        if handle.is_finished() {
            println("Thread done")
            break
        }
        println("Still running...")
        thread::sleep_ms(100)
    }

    handle.join()
}
```

### thread

`handle.thread()` returns a `Thread` value representing the spawned thread.
Use it to obtain the thread ID or to call `unpark` on it.

```tml
let handle: JoinHandle[Unit] = thread::spawn(do() { /* ... */ })
let t: Thread = handle.thread()
println("Spawned thread ID: ", t.id().as_u64())
```

## Thread Information and Utilities

### Current Thread

`thread::current()` returns a `Thread` representing the calling thread.

```tml
use std::thread

func main() {
    let me: Thread = thread::current()
    println("Main thread ID: ", me.id().as_u64())

    when me.name() {
        Just(name) => println("Name: ", name),
        Nothing    => println("Unnamed thread")
    }
}
```

### Sleep

`thread::sleep_ms` pauses the current thread for the given number of
milliseconds. The thread will not consume CPU while sleeping.

```tml
use std::thread

func main() {
    println("Starting")
    thread::sleep_ms(1000)  // Sleep 1 second
    println("One second later")
}
```

### Yield

`thread::yield_now` hints to the OS scheduler that the current thread is
willing to give up its time slice. Use this in busy-wait loops to avoid
starving other threads.

```tml
use std::thread

loop {
    if work_available() {
        do_work()
    } else {
        thread::yield_now()
    }
}
```

### Available Parallelism

`thread::available_parallelism()` returns the number of logical CPUs the
process can use. Use this to size thread pools.

```tml
use std::thread

let num_workers: U32 = thread::available_parallelism()
println("Spawning ", num_workers, " workers")
```

## Scoped Threads

Regular threads spawned with `thread::spawn` must satisfy `'static` — they
cannot borrow data from the calling stack because the calling function might
return before the thread finishes. Scoped threads lift this restriction.

`thread::scope` creates a scope from which threads can safely borrow the
calling stack. All threads spawned within the scope are guaranteed to finish
before `scope` returns, so the borrow checker can allow non-`'static`
borrows.

```tml
use std::thread::scope

func main() {
    let data: Vec[I32] = vec![10, 20, 30, 40, 50]

    scope(do(s: mut ref Scope) {
        // This thread borrows `data` from the parent stack.
        s.spawn(do() {
            var sum: I32 = 0
            for v in ref data {
                sum = sum + *v
            }
            println("Sum: ", sum)
        })
        // More threads can also borrow data here.
    })
    // All spawned threads have finished by this point.
    println("data still accessible: ", data.len(), " items")
}
```

Scoped threads return `ScopedJoinHandle[T]` instead of `JoinHandle[T]`. You
can join them explicitly or let the scope join them automatically.

```tml
use std::thread::scope

func parallel_sum(numbers: ref Vec[I32]) -> I32 {
    let mid: I32 = (numbers.len() / 2) as I32

    let result: I32 = scope(do(s: mut ref Scope) -> I32 {
        let left: ScopedJoinHandle[I32] = s.spawn(do() -> I32 {
            var sum: I32 = 0
            loop i in 0 to mid {
                sum = sum + numbers.get(i)
            }
            return sum
        })

        let right: ScopedJoinHandle[I32] = s.spawn(do() -> I32 {
            var sum: I32 = 0
            loop i in mid to numbers.len() as I32 {
                sum = sum + numbers.get(i)
            }
            return sum
        })

        return left.join() + right.join()
    })

    return result
}
```

Use scoped threads whenever your worker threads are short-lived and you want
to share existing stack data without cloning it into `Arc`.

## Thread Parking

Thread parking is a low-overhead mechanism for blocking a thread until
another thread explicitly wakes it. It is useful when channels or mutexes
would be overly heavy.

```tml
use std::thread

func main() {
    let main_thread: Thread = thread::current()

    let handle: JoinHandle[Unit] = thread::spawn(do() {
        thread::sleep_ms(200)
        main_thread.unpark()  // Wake the main thread.
    })

    thread::park()            // Block until unparked.
    println("Woken up!")
    handle.join()
}
```

`thread::park_timeout_ms` parks with an upper bound so the thread does not
wait forever if the wake signal is lost:

```tml
thread::park_timeout_ms(1000)  // Wait at most 1 second.
println("Continuing (unparked or timed out)")
```

> **Note:** A call to `unpark` before `park` is not lost—it acts as a
> token that `park` will consume immediately. However, park tokens are not
> additive: multiple `unpark` calls still only satisfy one `park`.

## Worker Pool Pattern

A worker pool keeps a fixed number of threads alive and distributes work to
them through a channel. This avoids the overhead of spawning a new thread for
every task.

```tml
use std::sync::{Arc, Mutex}
use std::sync::mpsc::{channel, Sender, Receiver}
use std::thread

type Job = do()

func main() {
    let num_workers: U32 = thread::available_parallelism()
    let (tx, rx): (Sender[Job], Receiver[Job]) = channel[Job]()
    let rx: Arc[Mutex[Receiver[Job]]] = Arc::new(Mutex::new(rx))

    // Spawn workers.
    var workers: Vec[JoinHandle[Unit]] = Vec::new()
    loop i in 0 to num_workers as I32 {
        let rx: Arc[Mutex[Receiver[Job]]] = rx.duplicate()
        let handle: JoinHandle[Unit] = thread::spawn(do() {
            loop {
                let guard: MutexGuard[Receiver[Job]] = rx.lock()
                when guard.get().recv() {
                    Ok(job) => {
                        drop(guard)  // Release lock before running the job.
                        job()
                    },
                    Err(_) => break  // Channel closed; exit the worker.
                }
            }
        })
        workers.push(handle)
    }

    // Submit 100 tasks.
    loop i in 0 to 100 {
        let task_id: I32 = i
        tx.send(do() {
            println("Task ", task_id, " running on a worker thread")
        })
    }

    // Drop the sender to close the channel, then wait for workers.
    drop(tx)
    for w in workers {
        w.join()
    }
}
```

## Best Practices

1. **Always join threads you spawn.** If the main thread exits before workers
   finish, the OS terminates the process and workers are killed mid-task.
   Use scoped threads when lifetime allows; otherwise collect handles and
   join them in order.

2. **Prefer scoped threads for short-lived borrowing.** They eliminate the
   need for `Arc` when the threads do not outlive the calling function.

3. **Size pools to available parallelism.** Spawning far more threads than
   CPU cores leads to excessive context switching. Query
   `thread::available_parallelism()` and use that as the pool size.

4. **Call `yield_now` in busy loops.** A thread that loops without blocking
   starves other threads on the same core.

5. **Handle join errors.** A thread can panic for any reason. Calling
   `join()` on a panicked thread returns `Err`. Decide per-application
   whether to propagate, log, or retry.

6. **Do not hold mutex guards across blocking calls.** Holding a lock while
   waiting on a channel or sleeping prevents other threads from making
   progress and can cause deadlocks.
