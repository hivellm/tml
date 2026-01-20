# Tasks: Native Thread-Safety Implementation

**Status**: In Progress (55%) - Phase 1, 2, 4, 5, 6 & 7 infrastructure complete

**Note**: This task covers implementing native thread-safety primitives for TML, including atomic types, synchronization primitives, thread management, and memory ordering semantics. The implementation follows Rust's safety model with Send/Sync behaviors for compile-time thread-safety verification.

## Phase 1: Atomic Types

> **Status**: Complete (TML library + MIR codegen infrastructure)

### 1.1 Atomic Integer Types
- [x] 1.1.1 Design `Atomic[T]` generic type in `lib/std/src/sync/atomic.tml`
- [x] 1.1.2 Implement `AtomicI32` with load/store operations
- [x] 1.1.3 Implement `AtomicI64` with load/store operations
- [x] 1.1.4 Implement `AtomicU32` with load/store operations
- [x] 1.1.5 Implement `AtomicU64` with load/store operations
- [x] 1.1.6 Implement `AtomicUsize` for pointer-sized atomics
- [x] 1.1.7 Implement `AtomicIsize` for pointer-sized signed atomics

### 1.2 Atomic Boolean and Pointer
- [x] 1.2.1 Implement `AtomicBool` type
- [x] 1.2.2 Implement `AtomicPtr[T]` for atomic pointer operations
- [x] 1.2.3 Add `is_lock_free()` method for all atomic types
- [ ] 1.2.4 Add static `LOCK_FREE` constant per type

### 1.3 Atomic Operations
- [x] 1.3.1 Implement `load(ordering: Ordering) -> T`
- [x] 1.3.2 Implement `store(val: T, ordering: Ordering)`
- [x] 1.3.3 Implement `swap(val: T, ordering: Ordering) -> T`
- [ ] 1.3.4 Implement `compare_and_swap(current: T, new: T, ordering: Ordering) -> T`
- [x] 1.3.5 Implement `compare_exchange(current: T, new: T, success: Ordering, failure: Ordering) -> Outcome[T, T]`
- [x] 1.3.6 Implement `compare_exchange_weak(...)` for spurious failure
- [x] 1.3.7 Implement `fetch_add(val: T, ordering: Ordering) -> T`
- [x] 1.3.8 Implement `fetch_sub(val: T, ordering: Ordering) -> T`
- [x] 1.3.9 Implement `fetch_and(val: T, ordering: Ordering) -> T`
- [x] 1.3.10 Implement `fetch_or(val: T, ordering: Ordering) -> T`
- [x] 1.3.11 Implement `fetch_xor(val: T, ordering: Ordering) -> T`
- [x] 1.3.12 Implement `fetch_max(val: T, ordering: Ordering) -> T`
- [x] 1.3.13 Implement `fetch_min(val: T, ordering: Ordering) -> T`
- [ ] 1.3.14 Add unit tests for atomic operations

### 1.4 LLVM Codegen for Atomics
- [x] 1.4.1 Add `AtomicLoadInst` to MIR
- [x] 1.4.2 Add `AtomicStoreInst` to MIR
- [x] 1.4.3 Add `AtomicRMWInst` to MIR (read-modify-write)
- [x] 1.4.4 Add `AtomicCmpXchgInst` to MIR
- [x] 1.4.5 Generate LLVM `atomicrmw` instructions
- [x] 1.4.6 Generate LLVM `cmpxchg` instructions
- [x] 1.4.7 Map TML ordering to LLVM ordering

## Phase 2: Memory Ordering

> **Status**: Complete (TML library + MIR codegen infrastructure)

### 2.1 Ordering Enum
- [x] 2.1.1 Define `Ordering` enum in `lib/std/src/sync/ordering.tml`
- [x] 2.1.2 Add `Relaxed` variant (no synchronization)
- [x] 2.1.3 Add `Acquire` variant (load synchronization)
- [x] 2.1.4 Add `Release` variant (store synchronization)
- [x] 2.1.5 Add `AcqRel` variant (both acquire and release)
- [x] 2.1.6 Add `SeqCst` variant (sequentially consistent)

### 2.2 Fence Operations
- [x] 2.2.1 Implement `fence(ordering: Ordering)` function
- [x] 2.2.2 Implement `compiler_fence(ordering: Ordering)` for compiler-only barrier
- [x] 2.2.3 Add `FenceInst` to MIR
- [x] 2.2.4 Generate LLVM `fence` instructions
- [ ] 2.2.5 Add unit tests for fence operations

### 2.3 Memory Model Documentation
- [ ] 2.3.1 Document TML memory model in `docs/specs/21-CONCURRENCY.md`
- [ ] 2.3.2 Document ordering guarantees
- [ ] 2.3.3 Document happens-before relationship
- [ ] 2.3.4 Add examples for each ordering

## Phase 3: Send and Sync Behaviors

> **Status**: Pending

### 3.1 Marker Behaviors
- [ ] 3.1.1 Define `Send` behavior in `lib/std/src/marker.tml`
- [ ] 3.1.2 Define `Sync` behavior in `lib/std/src/marker.tml`
- [ ] 3.1.3 Auto-implement `Send` for types with all `Send` fields
- [ ] 3.1.4 Auto-implement `Sync` for types with all `Sync` fields
- [ ] 3.1.5 Mark raw pointers as `not Send` and `not Sync` by default

### 3.2 Type Checking
- [ ] 3.2.1 Add `Send` bound checking to thread spawn
- [ ] 3.2.2 Add `Sync` bound checking for shared references
- [ ] 3.2.3 Implement `impl not Send for T` syntax
- [ ] 3.2.4 Implement `impl not Sync for T` syntax
- [ ] 3.2.5 Add lowlevel `impl Send for T` for FFI types
- [ ] 3.2.6 Add lowlevel `impl Sync for T` for FFI types

### 3.3 Standard Library Implementations
- [ ] 3.3.1 Implement `Send` for primitive types
- [ ] 3.3.2 Implement `Sync` for primitive types
- [ ] 3.3.3 Implement `Send` for `Atomic*` types
- [ ] 3.3.4 Implement `Sync` for `Atomic*` types
- [ ] 3.3.5 Implement `Sync` for `Mutex[T]` where T: Send
- [ ] 3.3.6 Implement `Send` for `MutexGuard[T]` where T: Send
- [ ] 3.3.7 Add unit tests for Send/Sync checking

## Phase 4: Mutex and Locking

> **Status**: Complete (TML library + platform runtime)

### 4.1 Mutex Type
- [x] 4.1.1 Design `Mutex[T]` type in `lib/std/src/sync/mutex.tml`
- [x] 4.1.2 Implement `new(value: T) -> Mutex[T]` constructor
- [x] 4.1.3 Implement `lock(this) -> MutexGuard[T]`
- [x] 4.1.4 Implement `try_lock(this) -> Maybe[MutexGuard[T]]`
- [x] 4.1.5 Implement `is_locked(this) -> Bool`
- [x] 4.1.6 Implement `into_inner(this) -> T` (consumes mutex)

### 4.2 MutexGuard Type
- [x] 4.2.1 Design `MutexGuard[T]` RAII type
- [x] 4.2.2 Implement `Deref` behavior for `MutexGuard[T]`
- [x] 4.2.3 Implement `DerefMut` behavior for `MutexGuard[T]`
- [x] 4.2.4 Implement `Drop` behavior to release lock
- [x] 4.2.5 Ensure `MutexGuard` is not `Send`

### 4.3 RwLock Type
- [x] 4.3.1 Design `RwLock[T]` type in `lib/std/src/sync/rwlock.tml`
- [x] 4.3.2 Implement `new(value: T) -> RwLock[T]` constructor
- [x] 4.3.3 Implement `read(this) -> RwLockReadGuard[T]`
- [x] 4.3.4 Implement `try_read(this) -> Maybe[RwLockReadGuard[T]]`
- [x] 4.3.5 Implement `write(this) -> RwLockWriteGuard[T]`
- [x] 4.3.6 Implement `try_write(this) -> Maybe[RwLockWriteGuard[T]]`
- [x] 4.3.7 Implement guard types with RAII drop

### 4.4 Platform Implementation
- [x] 4.4.1 Implement Mutex using pthreads (Unix)
- [x] 4.4.2 Implement Mutex using SRWLOCK (Windows)
- [x] 4.4.3 Implement RwLock using pthreads (Unix)
- [x] 4.4.4 Implement RwLock using SRWLOCK (Windows)
- [ ] 4.4.5 Add unit tests for Mutex and RwLock

## Phase 5: Thread Management

> **Status**: Complete (TML library + platform runtime)

### 5.1 Thread Type
- [x] 5.1.1 Design `Thread` type in `lib/std/src/thread/mod.tml`
- [x] 5.1.2 Implement `spawn[T: Send](f: do() -> T) -> JoinHandle[T]`
- [x] 5.1.3 Implement `spawn_named(name: Str, f: do() -> T) -> JoinHandle[T]` (via Builder)
- [x] 5.1.4 Implement `current() -> Thread` (get current thread)
- [x] 5.1.5 Implement `yield_now()` (yield to scheduler)
- [x] 5.1.6 Implement `sleep_ms(milliseconds: U64)`
- [ ] 5.1.7 Implement `sleep_until(instant: Instant)` (requires time module)
- [x] 5.1.8 Implement `park()` / `unpark()` for thread parking

### 5.2 JoinHandle Type
- [x] 5.2.1 Design `JoinHandle[T]` type
- [x] 5.2.2 Implement `join(this) -> Outcome[T, JoinError]`
- [x] 5.2.3 Implement `is_finished(this) -> Bool`
- [x] 5.2.4 Implement `thread(this) -> Thread` (get thread reference)
- [ ] 5.2.5 Handle panic propagation through join

### 5.3 Thread Properties
- [x] 5.3.1 Implement `Thread::name(this) -> Maybe[Str]`
- [x] 5.3.2 Implement `Thread::id(this) -> ThreadId`
- [x] 5.3.3 Design `ThreadId` type with equality/hash
- [x] 5.3.4 Implement `available_parallelism() -> U32`

### 5.4 Platform Implementation
- [x] 5.4.1 Implement thread creation using pthreads (Unix)
- [x] 5.4.2 Implement thread creation using CreateThread (Windows)
- [ ] 5.4.3 Implement thread-local storage (TLS)
- [ ] 5.4.4 Add unit tests for thread management

## Phase 6: Condition Variables

> **Status**: Complete (TML library + platform runtime)

### 6.1 Condvar Type
- [x] 6.1.1 Design `Condvar` type in `lib/std/src/sync/condvar.tml`
- [x] 6.1.2 Implement `new() -> Condvar` constructor
- [x] 6.1.3 Implement `wait[T](this, guard: MutexGuard[T]) -> MutexGuard[T]`
- [x] 6.1.4 Implement `wait_timeout_ms[T](this, guard: MutexGuard[T], timeout_ms: U64) -> (MutexGuard[T], Bool)`
- [x] 6.1.5 Implement `wait_while[T](this, guard: MutexGuard[T], condition: do(ref T) -> Bool) -> MutexGuard[T]`
- [x] 6.1.6 Implement `notify_one(this)`
- [x] 6.1.7 Implement `notify_all(this)`

### 6.2 Platform Implementation
- [x] 6.2.1 Implement Condvar using pthread_cond (Unix)
- [x] 6.2.2 Implement Condvar using CONDITION_VARIABLE (Windows)
- [ ] 6.2.3 Add unit tests for condition variables

## Phase 7: Barriers and Once

> **Status**: Complete (TML library)

### 7.1 Barrier Type
- [x] 7.1.1 Design `Barrier` type in `lib/std/src/sync/barrier.tml`
- [x] 7.1.2 Implement `new(count: U32) -> Barrier` constructor
- [x] 7.1.3 Implement `wait(this) -> BarrierWaitResult`
- [x] 7.1.4 Design `BarrierWaitResult` with `is_leader()` method

### 7.2 Once Type
- [x] 7.2.1 Design `Once` type in `lib/std/src/sync/once.tml`
- [x] 7.2.2 Implement `new() -> Once` constructor
- [x] 7.2.3 Implement `call_once(this, f: do())`
- [ ] 7.2.4 Implement `call_once_force(this, f: do(ref OnceState))` (deferred)
- [x] 7.2.5 Implement `is_completed(this) -> Bool`

### 7.3 OnceLock Type
- [x] 7.3.1 Design `OnceLock[T]` type for lazy initialization
- [x] 7.3.2 Implement `new() -> OnceLock[T]` constructor
- [x] 7.3.3 Implement `get(this) -> Maybe[ref T]`
- [x] 7.3.4 Implement `get_or_init(this, f: do() -> T) -> ref T`
- [ ] 7.3.5 Implement `get_or_try_init[E](this, f: do() -> Outcome[T, E]) -> Outcome[ref T, E]`
- [x] 7.3.6 Implement `set(this, value: T) -> Outcome[Unit, T]`
- [ ] 7.3.7 Add unit tests for Barrier, Once, OnceLock

## Phase 8: Lock-Free Data Structures

> **Status**: Pending

### 8.1 Lock-Free Queue
- [ ] 8.1.1 Design `LockFreeQueue[T]` in `lib/std/src/sync/queue.tml`
- [ ] 8.1.2 Implement Michael-Scott lock-free queue algorithm
- [ ] 8.1.3 Implement `new() -> LockFreeQueue[T]`
- [ ] 8.1.4 Implement `push(this, value: T)`
- [ ] 8.1.5 Implement `pop(this) -> Maybe[T]`
- [ ] 8.1.6 Implement `is_empty(this) -> Bool`

### 8.2 Lock-Free Stack
- [ ] 8.2.1 Design `LockFreeStack[T]` in `lib/std/src/sync/stack.tml`
- [ ] 8.2.2 Implement Treiber stack algorithm
- [ ] 8.2.3 Implement `new() -> LockFreeStack[T]`
- [ ] 8.2.4 Implement `push(this, value: T)`
- [ ] 8.2.5 Implement `pop(this) -> Maybe[T]`

### 8.3 MPSC Channel
- [ ] 8.3.1 Design `channel[T]() -> (Sender[T], Receiver[T])` in `lib/std/src/sync/mpsc.tml`
- [ ] 8.3.2 Implement `Sender[T]::send(this, value: T) -> Outcome[Unit, SendError[T]]`
- [ ] 8.3.3 Implement `Receiver[T]::recv(this) -> Outcome[T, RecvError]`
- [ ] 8.3.4 Implement `Receiver[T]::try_recv(this) -> Outcome[T, TryRecvError]`
- [ ] 8.3.5 Implement `Receiver[T]::recv_timeout(this, dur: Duration) -> Outcome[T, RecvTimeoutError]`
- [ ] 8.3.6 Implement `Sender[T]::clone(this) -> Sender[T]` (multiple producers)
- [ ] 8.3.7 Add unit tests for lock-free data structures

## Phase 9: Thread-Local Storage

> **Status**: Pending

### 9.1 Thread-Local Type
- [ ] 9.1.1 Design `@thread_local` decorator for static variables
- [ ] 9.1.2 Implement TLS variable declaration parsing
- [ ] 9.1.3 Generate LLVM `thread_local` globals
- [ ] 9.1.4 Implement lazy TLS initialization

### 9.2 LocalKey Type
- [ ] 9.2.1 Design `LocalKey[T]` type in `lib/std/src/thread/local.tml`
- [ ] 9.2.2 Implement `with[R](this, f: do(ref T) -> R) -> R`
- [ ] 9.2.3 Implement `try_with[R](this, f: do(ref T) -> R) -> Outcome[R, AccessError]`
- [ ] 9.2.4 Handle TLS destructor ordering
- [ ] 9.2.5 Add unit tests for thread-local storage

## Phase 10: Scoped Threads

> **Status**: Pending

### 10.1 Scoped Thread API
- [ ] 10.1.1 Design `scope[R](f: do(ref Scope) -> R) -> R` function
- [ ] 10.1.2 Design `Scope` type
- [ ] 10.1.3 Implement `Scope::spawn[T](this, f: do() -> T) -> ScopedJoinHandle[T]`
- [ ] 10.1.4 Implement `ScopedJoinHandle[T]::join(this) -> T`
- [ ] 10.1.5 Ensure all scoped threads join before scope exits

### 10.2 Borrowing Rules
- [ ] 10.2.1 Allow borrowing from parent scope in scoped threads
- [ ] 10.2.2 Verify lifetime bounds at compile time
- [ ] 10.2.3 No `'static` requirement for scoped thread closures
- [ ] 10.2.4 Add unit tests for scoped threads

## Phase 11: Arc and Shared Ownership

> **Status**: Pending

### 11.1 Arc Type
- [ ] 11.1.1 Design `Sync[T]` type (Arc equivalent) in `lib/std/src/sync/arc.tml`
- [ ] 11.1.2 Implement `new(value: T) -> Sync[T]` constructor
- [ ] 11.1.3 Implement atomic reference counting
- [ ] 11.1.4 Implement `clone(this) -> Sync[T]` with atomic increment
- [ ] 11.1.5 Implement `Drop` with atomic decrement and deallocation
- [ ] 11.1.6 Implement `strong_count(this) -> U64`
- [ ] 11.1.7 Implement `ptr_eq(this, other: ref Sync[T]) -> Bool`

### 11.2 Weak References
- [ ] 11.2.1 Design `Weak[T]` type for weak references
- [ ] 11.2.2 Implement `Sync[T]::downgrade(this) -> Weak[T]`
- [ ] 11.2.3 Implement `Weak[T]::upgrade(this) -> Maybe[Sync[T]]`
- [ ] 11.2.4 Implement `Weak[T]::strong_count(this) -> U64`
- [ ] 11.2.5 Implement `Weak[T]::weak_count(this) -> U64`

### 11.3 Interior Mutability
- [ ] 11.3.1 Implement `Send` and `Sync` for `Sync[T]` where T: Send + Sync
- [ ] 11.3.2 Implement `Sync[Mutex[T]]` pattern for shared mutable state
- [ ] 11.3.3 Add unit tests for Arc functionality

## Phase 12: Async Foundation (Future)

> **Status**: Deferred - Requires async runtime design

### 12.1 Async Mutex
- [ ] 12.1.1 Design `AsyncMutex[T]` type (deferred)
- [ ] 12.1.2 Implement `lock(this) -> AsyncMutexGuard[T]` as async (deferred)
- [ ] 12.1.3 Implement non-blocking lock acquisition (deferred)

### 12.2 Async Channel
- [ ] 12.2.1 Design `async_channel[T]()` (deferred)
- [ ] 12.2.2 Implement async send/receive (deferred)
- [ ] 12.2.3 Implement bounded channel with backpressure (deferred)

## Phase 13: Testing Infrastructure

> **Status**: Pending

### 13.1 Concurrent Test Utilities
- [ ] 13.1.1 Implement `loom`-style deterministic testing (basic)
- [ ] 13.1.2 Implement thread interleaving exploration
- [ ] 13.1.3 Add `@concurrent_test` decorator for multi-threaded tests
- [ ] 13.1.4 Implement deadlock detection in tests

### 13.2 Stress Tests
- [ ] 13.2.1 Create atomic operation stress tests
- [ ] 13.2.2 Create mutex contention stress tests
- [ ] 13.2.3 Create channel throughput benchmarks
- [ ] 13.2.4 Create lock-free data structure stress tests

## Phase 14: Documentation

> **Status**: Pending

### 14.1 User Documentation
- [ ] 14.1.1 Write `docs/user/ch16-00-concurrency.md` guide
- [ ] 14.1.2 Document Send/Sync system with examples
- [ ] 14.1.3 Document common concurrency patterns
- [ ] 14.1.4 Document pitfalls and best practices

### 14.2 API Reference
- [ ] 14.2.1 Document all atomic types and methods
- [ ] 14.2.2 Document Mutex/RwLock APIs
- [ ] 14.2.3 Document channel APIs
- [ ] 14.2.4 Document thread management APIs

## Phase 15: Integration and Optimization

> **Status**: Pending

### 15.1 Compiler Integration
- [ ] 15.1.1 Add `--threads` flag for parallel compilation
- [ ] 15.1.2 Verify Send/Sync bounds at call sites
- [ ] 15.1.3 Emit warnings for potential data races
- [ ] 15.1.4 Integrate with borrow checker for thread safety

### 15.2 Performance Optimization
- [ ] 15.2.1 Optimize lock-free algorithms for x86_64
- [ ] 15.2.2 Optimize lock-free algorithms for ARM64
- [ ] 15.2.3 Implement adaptive spinning for mutexes
- [ ] 15.2.4 Add cache-line padding for contended atomics

### 15.3 Benchmarks
- [ ] 15.3.1 Create concurrent counter benchmark
- [ ] 15.3.2 Create producer-consumer throughput benchmark
- [ ] 15.3.3 Create mutex contention benchmark
- [ ] 15.3.4 Compare with Rust std::sync performance

## File Structure

```
lib/std/src/
├── sync/
│   ├── mod.tml              # Module exports
│   ├── atomic.tml           # Atomic types
│   ├── ordering.tml         # Memory ordering
│   ├── mutex.tml            # Mutex[T], MutexGuard[T]
│   ├── rwlock.tml           # RwLock[T], guards
│   ├── condvar.tml          # Condition variables
│   ├── barrier.tml          # Barrier
│   ├── once.tml             # Once, OnceLock[T]
│   ├── arc.tml              # Sync[T], Weak[T]
│   ├── mpsc.tml             # Channels
│   ├── queue.tml            # LockFreeQueue[T]
│   └── stack.tml            # LockFreeStack[T]
├── thread/
│   ├── mod.tml              # Thread management
│   ├── local.tml            # Thread-local storage
│   └── scope.tml            # Scoped threads
└── marker.tml               # Send, Sync behaviors

compiler/
├── include/
│   ├── mir/
│   │   └── atomic_inst.hpp  # Atomic MIR instructions
│   └── types/
│       └── send_sync.hpp    # Send/Sync trait checking
└── src/
    ├── codegen/
    │   └── atomic.cpp       # LLVM atomic codegen
    └── types/
        └── send_sync.cpp    # Send/Sync implementation
```

## Validation

- [ ] V.1 All atomic operations generate correct LLVM atomics
- [ ] V.2 Send/Sync checking catches data race attempts at compile time
- [ ] V.3 Mutex provides mutual exclusion under contention
- [ ] V.4 RwLock allows multiple concurrent readers
- [ ] V.5 Channels work correctly with multiple producers
- [ ] V.6 Lock-free data structures are linearizable
- [ ] V.7 Thread-local storage initializes correctly per thread
- [ ] V.8 Scoped threads enforce lifetime safety
- [ ] V.9 No memory leaks in concurrent scenarios
- [ ] V.10 Performance within 10% of Rust std::sync
- [ ] V.11 All existing tests pass
- [ ] V.12 Documentation complete with examples
