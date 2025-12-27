# Atomic Operations

Atomic operations are the foundation of thread-safe programming. They
guarantee that operations complete without interruption, even when
multiple threads access the same memory.

## Why Atomics?

Consider incrementing a counter from two threads:

```
Thread 1: read counter (0)
Thread 2: read counter (0)
Thread 1: write counter (1)
Thread 2: write counter (1)  // Lost update!
```

Without atomics, both threads read 0 and write 1, losing an increment.

## Atomic Functions

TML provides these atomic operations:

| Function | Description |
|----------|-------------|
| `atomic_load(ptr)` | Read value atomically |
| `atomic_store(ptr, value)` | Write value atomically |
| `atomic_add(ptr, value)` | Add and return old value |
| `atomic_sub(ptr, value)` | Subtract and return old value |
| `atomic_exchange(ptr, value)` | Swap and return old value |
| `atomic_cas(ptr, expected, desired)` | Compare-and-swap, returns bool |
| `atomic_and(ptr, value)` | Bitwise AND, returns old value |
| `atomic_or(ptr, value)` | Bitwise OR, returns old value |

## Basic Example

```tml
func main() {
    let counter = alloc(4)
    write_i32(counter, 0)

    // Atomic store
    atomic_store(counter, 42)
    println(atomic_load(counter))  // 42

    // Atomic add (returns old value)
    let old = atomic_add(counter, 10)
    println(old)                    // 42 (old value)
    println(atomic_load(counter))   // 52 (new value)

    dealloc(counter)
}
```

## Compare-and-Swap (CAS)

CAS is the most powerful atomic operation. It only updates if the current
value matches the expected value:

```tml
func main() {
    let counter = alloc(4)
    atomic_store(counter, 100)

    // Try to swap 100 -> 200 (should succeed)
    let success1 = atomic_cas(counter, 100, 200)
    println(success1)               // true
    println(atomic_load(counter))   // 200

    // Try to swap 100 -> 300 (should fail, current is 200)
    let success2 = atomic_cas(counter, 100, 300)
    println(success2)               // false
    println(atomic_load(counter))   // 200 (unchanged)

    dealloc(counter)
}
```

## Spinlocks

Build a simple lock using atomics:

```tml
func main() {
    let lock = alloc(4)
    write_i32(lock, 0)  // 0 = unlocked

    // Acquire lock
    let acquired = spin_trylock(lock)
    println(acquired)  // true

    // Try again (should fail)
    let acquired2 = spin_trylock(lock)
    println(acquired2)  // false

    // Release lock
    spin_unlock(lock)

    // Now can acquire again
    let acquired3 = spin_trylock(lock)
    println(acquired3)  // true

    spin_unlock(lock)
    dealloc(lock)
}
```

## Memory Fences

Control memory ordering with fences:

```tml
func main() {
    fence()          // Full memory barrier
    fence_acquire()  // Acquire fence
    fence_release()  // Release fence
}
```

Fences ensure that memory operations before/after the fence are properly
ordered with respect to other threads.

## Atomic Bitwise Operations

Use atomic AND/OR for flag manipulation:

```tml
func main() {
    let flags = alloc(4)
    atomic_store(flags, 0xFF)  // 255

    // Clear lower 4 bits
    let old1 = atomic_and(flags, 0xF0)
    println(old1)                   // 255 (old)
    println(atomic_load(flags))     // 240 (new: 255 & 0xF0)

    // Set lower 4 bits
    let old2 = atomic_or(flags, 0x0F)
    println(old2)                   // 240 (old)
    println(atomic_load(flags))     // 255 (new: 240 | 0x0F)

    dealloc(flags)
}
```

## When to Use Atomics

Use atomics for:

1. **Simple counters**: Increment/decrement across threads
2. **Flags**: Boolean state shared between threads
3. **Lock-free data structures**: Building queues, stacks
4. **Building higher-level primitives**: Implementing mutex, semaphore

For complex shared state, consider using channels or mutex instead.
