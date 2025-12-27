# Fearless Concurrency

Concurrent programming—running multiple tasks at the same time—is
increasingly important. TML provides powerful primitives for safe
concurrent programming, inspired by Go's approach.

## Why Concurrency?

Modern computers have multiple cores. Concurrent programming lets you:

- **Use all cores**: Run computations in parallel
- **Stay responsive**: Handle I/O without blocking
- **Scale better**: Handle more work with the same resources

## TML's Concurrency Model

TML provides several concurrency primitives:

| Feature | Description |
|---------|-------------|
| Atomic Operations | Lock-free thread-safe operations |
| Channels | Go-style message passing |
| Mutex | Mutual exclusion locks |
| WaitGroup | Synchronization for multiple tasks |

## Thread Safety

TML's type system helps prevent common concurrency bugs:

- **Data races**: Caught at compile time
- **Deadlocks**: Reduced through careful API design
- **Race conditions**: Atomic operations ensure consistency

## What You'll Learn

In this chapter, you'll learn:

1. **Atomic Operations**: Lock-free primitives for simple synchronization
2. **Channels**: Message passing for communication between tasks
3. **Mutex and Synchronization**: Protecting shared data
