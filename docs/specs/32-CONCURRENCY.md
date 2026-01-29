# TML Concurrency Specification

This document specifies TML's memory model, atomic operations, synchronization primitives, and thread-safety mechanisms.

## 1. Overview

TML provides a Rust-inspired concurrency model with:

- **Atomic types** for lock-free programming
- **Memory orderings** for fine-grained synchronization control
- **Send/Sync behaviors** for compile-time thread-safety verification
- **Synchronization primitives** (Mutex, RwLock, Condvar, Barrier)
- **MPSC channels** for message passing
- **Scoped threads** for borrowing from parent scope

## 2. Memory Model

TML follows the C++11/Rust memory model with sequentially consistent semantics by default. The memory model defines how reads and writes to shared memory are ordered across threads.

### 2.1 Sequential Consistency

By default, TML enforces sequential consistency for data race-free programs:

- All threads observe memory operations in a single, consistent total order
- Programs without data races behave as if executed on a sequentially consistent machine
- Data races are undefined behavior

### 2.2 Data Races

A **data race** occurs when:
1. Two or more threads access the same memory location concurrently
2. At least one access is a write
3. The accesses are not synchronized (not using atomics or locks)

TML's type system (Send/Sync) helps prevent data races at compile time.

### 2.3 Atomic Memory Operations

Atomic operations provide guaranteed ordering and visibility across threads. All atomic operations are indivisible - they complete entirely or not at all.

## 3. Memory Ordering

TML provides five memory orderings, from weakest to strongest:

### 3.1 Ordering::Relaxed

**Guarantees**: Atomicity only. No synchronization or ordering constraints.

**Use cases**:
- Counters where exact order doesn't matter
- Statistics gathering
- Flags where only eventual visibility is needed

```tml
use std::sync::atomic::{AtomicI32, Ordering}

// Counter incremented by multiple threads
let counter: AtomicI32 = AtomicI32::new(0)

// Order doesn't matter, just need accurate final count
counter.fetch_add(1, Ordering::Relaxed)
```

**Guarantees NOT provided**:
- No ordering with other memory operations
- Writes may be reordered before or after by compiler/CPU
- Other threads may observe operations in different orders

### 3.2 Ordering::Acquire

**Guarantees**: All subsequent reads and writes in the current thread cannot be reordered before this load.

**Use cases**:
- Loading a flag to check if data is ready
- Reading a "published" indicator
- Mutex lock acquisition (paired with Release store)

```tml
use std::sync::atomic::{AtomicBool, Ordering}

let ready: AtomicBool = AtomicBool::new(false)
var data: I32 = 0

// Thread 2: Wait for data to be ready
loop (not ready.load(Ordering::Acquire)) {
    // spin
}
// After Acquire load returns true, we're guaranteed to see
// all writes that happened before the Release store
let value: I32 = data  // Safe: sees the write from Thread 1
```

**Key property**: Forms a **synchronizes-with** relationship with a Release store to the same location.

### 3.3 Ordering::Release

**Guarantees**: All preceding reads and writes in the current thread cannot be reordered after this store.

**Use cases**:
- Setting a flag to indicate data is ready
- Publishing data for other threads
- Mutex unlock (paired with Acquire load)

```tml
use std::sync::atomic::{AtomicBool, Ordering}

let ready: AtomicBool = AtomicBool::new(false)
var data: I32 = 0

// Thread 1: Prepare data and publish
data = 42
ready.store(true, Ordering::Release)  // All writes before this are visible
                                       // to threads that Acquire-load `true`
```

**Key property**: All writes before the Release store become visible to any thread that performs an Acquire load and sees this store's value.

### 3.4 Ordering::AcqRel (Acquire-Release)

**Guarantees**: Combines Acquire and Release semantics. Used for read-modify-write operations.

**Use cases**:
- Atomic swap operations
- Compare-and-exchange
- Any RMW operation that both reads and writes

```tml
use std::sync::atomic::{AtomicI32, Ordering}

let value: AtomicI32 = AtomicI32::new(0)

// Atomically increment and get the old value
// - Acquire: see writes before other threads' stores
// - Release: make our writes visible to other threads
let old: I32 = value.fetch_add(1, Ordering::AcqRel)
```

### 3.5 Ordering::SeqCst (Sequentially Consistent)

**Guarantees**: Strongest ordering. All SeqCst operations across all threads appear in a single, globally consistent total order.

**Use cases**:
- When in doubt, use SeqCst
- Complex synchronization patterns
- When performance is not critical

```tml
use std::sync::atomic::{AtomicBool, Ordering}

let flag1: AtomicBool = AtomicBool::new(false)
let flag2: AtomicBool = AtomicBool::new(false)

// Thread 1
flag1.store(true, Ordering::SeqCst)

// Thread 2
flag2.store(true, Ordering::SeqCst)

// Thread 3
let a: Bool = flag1.load(Ordering::SeqCst)
let b: Bool = flag2.load(Ordering::SeqCst)

// Thread 4
let c: Bool = flag2.load(Ordering::SeqCst)
let d: Bool = flag1.load(Ordering::SeqCst)

// SeqCst guarantees: if Thread 3 sees a=true, b=false,
// then Thread 4 cannot see c=true, d=false
// (there's a consistent global order of all SeqCst ops)
```

**Performance note**: SeqCst may be slower than weaker orderings on some architectures due to memory barriers.

## 4. Happens-Before Relationship

The **happens-before** relationship defines when one operation is guaranteed to be visible to another.

### 4.1 Definition

Operation A **happens-before** operation B if:

1. **Program order**: A and B are in the same thread, and A comes before B in program order
2. **Synchronizes-with**: A is a Release store, B is an Acquire load that reads A's value
3. **Transitivity**: There exists C such that A happens-before C and C happens-before B

### 4.2 Synchronizes-With

A Release store **synchronizes-with** an Acquire load when:
- Both access the same atomic variable
- The load reads the value written by the store

```tml
use std::sync::atomic::{AtomicI32, Ordering}

let sync: AtomicI32 = AtomicI32::new(0)
var data: I32 = 0

// Thread 1
data = 42                              // (1)
sync.store(1, Ordering::Release)       // (2) synchronizes-with (3)

// Thread 2
if sync.load(Ordering::Acquire) == 1 { // (3) synchronizes-with (2)
    assert(data == 42)                  // (4) guaranteed to see (1)
}
```

In this example:
- (1) happens-before (2) by program order
- (2) synchronizes-with (3) by Release/Acquire pairing
- (3) happens-before (4) by program order
- Therefore: (1) happens-before (4) by transitivity

### 4.3 Ordering Table

| Operation Type | Valid Orderings |
|----------------|-----------------|
| `load` | Relaxed, Acquire, SeqCst |
| `store` | Relaxed, Release, SeqCst |
| `swap`, `compare_exchange`, `fetch_*` | All orderings |
| `fence` | Acquire, Release, AcqRel, SeqCst |

## 5. Fence Operations

Fences provide ordering constraints without accessing a specific memory location.

### 5.1 fence()

A **fence** establishes ordering constraints on all memory operations:

```tml
use std::sync::atomic::{fence, Ordering}

// Ensure all previous writes are visible before continuing
fence(Ordering::Release)

// Ensure all subsequent reads see previous writes
fence(Ordering::Acquire)
```

### 5.2 compiler_fence()

A **compiler fence** prevents the compiler from reordering operations across the fence, but does not emit CPU memory barriers:

```tml
use std::sync::atomic::{compiler_fence, Ordering}

// Prevent compiler reordering (no CPU barrier)
compiler_fence(Ordering::SeqCst)
```

Use cases:
- Signal handlers
- Interaction with hardware/MMIO
- When CPU ordering is already guaranteed by architecture

### 5.3 spin_loop_hint()

Hints to the CPU that we're in a spin loop, allowing power savings:

```tml
use std::sync::atomic::spin_loop_hint

loop (not flag.load(Ordering::Acquire)) {
    spin_loop_hint()  // CPU hint: we're spinning
}
```

## 6. Atomic Types

TML provides the following atomic types in `std::sync::atomic`:

| Type | Description |
|------|-------------|
| `AtomicBool` | Atomic boolean |
| `AtomicI8`, `AtomicI16`, `AtomicI32`, `AtomicI64` | Atomic signed integers |
| `AtomicU8`, `AtomicU16`, `AtomicU32`, `AtomicU64` | Atomic unsigned integers |
| `AtomicIsize`, `AtomicUsize` | Atomic pointer-sized integers |
| `AtomicPtr[T]` | Atomic raw pointer |

### 6.1 Common Operations

All atomic types support:

| Method | Description |
|--------|-------------|
| `new(value) -> Atomic` | Create new atomic |
| `load(ordering) -> T` | Atomic load |
| `store(value, ordering)` | Atomic store |
| `swap(value, ordering) -> T` | Atomic swap |
| `compare_exchange(current, new, success_ord, fail_ord) -> Outcome[T, T]` | CAS |
| `compare_exchange_weak(...)` | CAS with spurious failure |

Integer atomics additionally support:

| Method | Description |
|--------|-------------|
| `fetch_add(val, ordering) -> T` | Add and return old value |
| `fetch_sub(val, ordering) -> T` | Subtract and return old value |
| `fetch_and(val, ordering) -> T` | Bitwise AND |
| `fetch_or(val, ordering) -> T` | Bitwise OR |
| `fetch_xor(val, ordering) -> T` | Bitwise XOR |
| `fetch_max(val, ordering) -> T` | Maximum |
| `fetch_min(val, ordering) -> T` | Minimum |

## 7. Send and Sync Behaviors

TML uses two marker behaviors for thread safety:

### 7.1 Send

A type is `Send` if it can be safely transferred to another thread.

```tml
// Most types are automatically Send if all fields are Send
type Point { x: I32, y: I32 }  // Implicitly Send

// Raw pointers are NOT Send by default
type NotSend { ptr: Ptr[I32] }  // Not Send
```

### 7.2 Sync

A type is `Sync` if it can be safely shared between threads (via `ref T`).

```tml
// Immutable data is Sync
let shared: ref I32 = ref value  // Can be shared if I32 is Sync

// Mutex[T] is Sync even if T is not Sync
let mutex: Mutex[Cell[I32]] = Mutex::new(Cell::new(0))
// Cell is not Sync, but Mutex[Cell[I32]] is Sync
```

### 7.3 Rules

| Type | Send | Sync |
|------|------|------|
| Primitive types (I32, Bool, etc.) | Yes | Yes |
| `ref T` | If T is Sync | If T is Sync |
| `mut ref T` | If T is Send | Never |
| `Ptr[T]` / `RawPtr[T]` | Never | Never |
| `AtomicT` | Yes | Yes |
| `Mutex[T]` | If T is Send | If T is Send |
| `RwLock[T]` | If T is Send + Sync | If T is Send + Sync |
| `Arc[T]` | If T is Send + Sync | If T is Send + Sync |
| `Rc[T]` | Never | Never |

## 8. Synchronization Primitives

### 8.1 Mutex[T]

Mutual exclusion lock providing safe interior mutability:

```tml
use std::sync::Mutex

let data: Mutex[I32] = Mutex::new(0)

// Lock and modify
let guard: MutexGuard[I32] = data.lock()
*guard.get_mut() = 42
// Lock released when guard goes out of scope
```

### 8.2 RwLock[T]

Reader-writer lock allowing multiple readers or one writer:

```tml
use std::sync::RwLock

let data: RwLock[I32] = RwLock::new(0)

// Multiple readers
let r1: RwLockReadGuard[I32] = data.read()
let r2: RwLockReadGuard[I32] = data.read()  // OK: multiple readers

// Single writer (blocks until all readers release)
let w: RwLockWriteGuard[I32] = data.write()
```

### 8.3 Condvar

Condition variable for waiting on state changes:

```tml
use std::sync::{Mutex, Condvar}

let pair: (Mutex[Bool], Condvar) = (Mutex::new(false), Condvar::new())

// Thread 1: Wait for condition
let guard: MutexGuard[Bool] = pair.0.lock()
loop (not *guard.get()) {
    guard = pair.1.wait(guard)
}

// Thread 2: Signal condition
let guard: MutexGuard[Bool] = pair.0.lock()
*guard.get_mut() = true
pair.1.notify_one()
```

### 8.4 Barrier

Synchronization point for multiple threads:

```tml
use std::sync::Barrier

let barrier: Barrier = Barrier::new(3)

// In each of 3 threads:
let result: BarrierWaitResult = barrier.wait()
if result.is_leader() {
    // One thread is designated the leader
}
```

### 8.5 Once and OnceLock

One-time initialization primitives:

```tml
use std::sync::{Once, OnceLock}

// Once: run code exactly once
let init: Once = Once::new()
init.call_once(do() {
    // Initialization code runs once
})

// OnceLock: lazy initialization of a value
let config: OnceLock[Config] = OnceLock::new()
let cfg: ref Config = config.get_or_init(do() {
    return load_config()
})
```

## 9. MPSC Channels

Multi-producer, single-consumer channels for message passing:

```tml
use std::sync::mpsc::{channel, Sender, Receiver}

let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

// Clone sender for multiple producers
let tx2: Sender[I32] = tx.duplicate()

// Send from multiple threads
tx.send(1)
tx2.send(2)

// Receive in one thread
let v1: I32 = rx.recv().unwrap()
let v2: I32 = rx.recv().unwrap()
```

## 10. Thread Management

### 10.1 Spawning Threads

```tml
use std::thread

let handle: JoinHandle[I32] = thread::spawn(do() -> I32 {
    return 42
})

let result: I32 = handle.join().unwrap()
```

### 10.2 Thread Functions

| Function | Description |
|----------|-------------|
| `spawn(f)` | Create new thread |
| `current()` | Get current thread handle |
| `yield_now()` | Yield to scheduler |
| `sleep_ms(ms)` | Sleep for milliseconds |
| `available_parallelism()` | Get number of CPUs |

### 10.3 Scoped Threads

Scoped threads can borrow from the parent scope:

```tml
use std::thread::scope

let data: I32 = 42

scope(do(s: mut ref Scope) {
    s.spawn(do() {
        // Can borrow `data` without moving it
        print(data)
    })
})  // All scoped threads join here
```

## 11. Arc and Shared Ownership

`Arc[T]` provides atomic reference counting for shared ownership:

```tml
use std::sync::Arc

let shared: Arc[I32] = Arc::new(42)
let clone: Arc[I32] = shared.duplicate()

// Both `shared` and `clone` point to the same data
assert(shared.strong_count() == 2)
```

### 11.1 Weak References

```tml
use std::sync::{Arc, Weak}

let strong: Arc[I32] = Arc::new(42)
let weak: Weak[I32] = strong.downgrade()

// Weak doesn't keep data alive
when weak.upgrade() {
    Just(arc) => print(*arc.get()),
    Nothing => print("Data was dropped")
}
```

## 12. Best Practices

### 12.1 Choosing Memory Ordering

1. **Start with SeqCst** when prototyping
2. **Use Acquire/Release pairs** for producer-consumer patterns
3. **Use Relaxed** only for independent counters/statistics
4. **Profile** before optimizing away SeqCst

### 12.2 Avoiding Common Pitfalls

1. **Don't use Relaxed for synchronization** - it provides no ordering guarantees
2. **Pair Acquire with Release** - using only one is usually a bug
3. **Prefer higher-level primitives** - Mutex/Channels over raw atomics
4. **Don't hold locks across await points** (future async support)

### 12.3 Performance Considerations

| Ordering | x86_64 Cost | ARM64 Cost |
|----------|-------------|------------|
| Relaxed | Free | Free |
| Acquire | Free (loads) | Barrier needed |
| Release | Free (stores) | Barrier needed |
| AcqRel | Free for RMW | Barrier needed |
| SeqCst | Full barrier | Full barrier |

## 13. LLVM Mapping

TML memory orderings map to LLVM orderings:

| TML Ordering | LLVM Ordering |
|--------------|---------------|
| Relaxed | monotonic |
| Acquire | acquire |
| Release | release |
| AcqRel | acq_rel |
| SeqCst | seq_cst |

## References

- [C++11 Memory Model](https://en.cppreference.com/w/cpp/atomic/memory_order)
- [Rust Atomics and Locks](https://marabos.nl/atomics/)
- [LLVM Atomic Instructions](https://llvm.org/docs/LangRef.html#atomic-memory-ordering-constraints)
