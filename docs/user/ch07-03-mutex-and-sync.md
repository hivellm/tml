# Mutex and Synchronization

When you need to share data between threads and can't use channels,
mutex (mutual exclusion) provides a way to protect shared resources.

## What is a Mutex?

A mutex is a lock that ensures only one thread can access protected
data at a time. Other threads must wait until the lock is released.

## Mutex Functions

| Function | Description |
|----------|-------------|
| `mutex_create()` | Create a new mutex |
| `mutex_lock(m)` | Acquire lock (blocks if held) |
| `mutex_unlock(m)` | Release lock |
| `mutex_try_lock(m)` | Non-blocking lock attempt |
| `mutex_destroy(m)` | Free mutex memory |

## Basic Usage

```tml
func main() {
    let mtx = mutex_create()

    // Acquire the lock
    mutex_lock(mtx)
    println("Lock acquired")

    // Critical section - safe to access shared data here

    // Release the lock
    mutex_unlock(mtx)
    println("Lock released")

    mutex_destroy(mtx)
}
```

## Try Lock

Use `mutex_try_lock` when you don't want to block:

```tml
func main() {
    let mtx = mutex_create()

    // First lock succeeds
    mutex_lock(mtx)

    // Try to lock again - fails because we already hold it
    if mutex_try_lock(mtx) {
        println("Got lock")
    } else {
        println("Lock busy")  // This runs
    }

    mutex_unlock(mtx)

    // Now try_lock succeeds
    if mutex_try_lock(mtx) {
        println("Got lock")  // This runs
        mutex_unlock(mtx)
    }

    mutex_destroy(mtx)
}
```

## Protecting Shared Data

Use a mutex to protect shared data:

```tml
func main() {
    let counter = alloc(4)
    write_i32(counter, 0)
    let mtx = mutex_create()

    // Simulate multiple "threads" incrementing counter
    for i in 0 to 10 {
        mutex_lock(mtx)

        // Critical section
        let value = read_i32(counter)
        write_i32(counter, value + 1)

        mutex_unlock(mtx)
    }

    println("Counter: ", read_i32(counter))  // Counter: 10

    dealloc(counter)
    mutex_destroy(mtx)
}
```

## WaitGroup

WaitGroup helps wait for multiple tasks to complete:

| Function | Description |
|----------|-------------|
| `waitgroup_create()` | Create a new WaitGroup |
| `waitgroup_add(wg, n)` | Add n to the counter |
| `waitgroup_done(wg)` | Decrement counter by 1 |
| `waitgroup_wait(wg)` | Block until counter is 0 |
| `waitgroup_destroy(wg)` | Free WaitGroup memory |

```tml
func main() {
    let wg = waitgroup_create()

    // Say we're waiting for 3 tasks
    waitgroup_add(wg, 3)

    // Simulate tasks completing
    println("Task 1 done")
    waitgroup_done(wg)

    println("Task 2 done")
    waitgroup_done(wg)

    println("Task 3 done")
    waitgroup_done(wg)

    // Wait for all tasks (returns immediately since count is 0)
    waitgroup_wait(wg)
    println("All tasks complete!")

    waitgroup_destroy(wg)
}
```

## Common Patterns

### Lock-Protect-Unlock

Always follow this pattern:

```tml
mutex_lock(mtx)
// Do work with shared data
mutex_unlock(mtx)
```

### Try-Lock with Fallback

```tml
if mutex_try_lock(mtx) {
    // Got the lock, do fast-path work
    mutex_unlock(mtx)
} else {
    // Lock busy, do alternative work or wait
    mutex_lock(mtx)
    // Now we have it
    mutex_unlock(mtx)
}
```

## Avoiding Deadlocks

Deadlocks occur when threads wait for each other indefinitely.

### Rules to Prevent Deadlocks

1. **Lock in consistent order**: Always acquire locks in the same order
2. **Don't hold locks while waiting**: Release before blocking operations
3. **Use timeouts**: Don't wait forever
4. **Avoid nested locks**: One lock at a time when possible

### Bad Example (Potential Deadlock)

```tml
// Thread 1: lock A, then B
// Thread 2: lock B, then A
// -> Deadlock if they interleave!
```

### Good Example (Consistent Order)

```tml
// Both threads: lock A first, then B
// -> No deadlock possible
```

## When to Use What

| Need | Use |
|------|-----|
| Pass data between tasks | Channel |
| Protect shared state | Mutex |
| Simple counter | Atomic |
| Wait for tasks | WaitGroup |
| Lock-free access | Atomic operations |
