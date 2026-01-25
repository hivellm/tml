# Tasks: MPSC Channel Codegen Fixes

**Status**: Blocked (0%)

**Blocker**: The MPSC channel implementation in `lib/std/src/sync/mpsc.tml` has complete library code, but codegen fails for complex generic synchronization primitives.

## Problem Summary

The MPSC module uses these patterns that don't work:
- `Mutex[Ptr[ChannelNode[T]]]` - Mutex containing generic pointer
- `Condvar.wait(guard: MutexGuard[T])` - Condvar wait with generic guard
- `Arc[ChannelInner[T]]` with nested generic structs containing Mutex/Condvar/Atomics
- Pointer dereference in `lowlevel` blocks: `(*ptr).field = value`

Working alternatives (for reference):
- `AtomicPtr[Node[T]]` - works in LockFreeQueue
- `AtomicUsize`, `AtomicBool` - work standalone
- `Arc[T]` with simple T - works
- `Mutex[T]` with simple T - works (sync_primitives tests pass)

## Phase 1: Diagnose Root Causes

### 1.1 Identify Specific Codegen Failures
- [ ] 1.1.1 Create minimal repro: `Mutex[Ptr[I32]]` basic usage
- [ ] 1.1.2 Create minimal repro: `Condvar.wait(MutexGuard[Ptr[I32]])`
- [ ] 1.1.3 Create minimal repro: Nested generic struct with Mutex field
- [ ] 1.1.4 Create minimal repro: `lowlevel { (*ptr).field = value }` in generic context
- [ ] 1.1.5 Compare IR output between working LockFreeQueue and failing MPSC

### 1.2 Document Failure Points
- [ ] 1.2.1 Map "Unknown method: lock" error to codegen source
- [ ] 1.2.2 Map "Unknown method: wait" error to codegen source
- [ ] 1.2.3 Map "Cannot dereference non-reference type" error to codegen source
- [ ] 1.2.4 Identify why `duplicate()` (Arc clone) fails in nested generic context

## Phase 2: Fix Mutex[Ptr[T]] Codegen

### 2.1 Generic Pointer in Mutex
- [ ] 2.1.1 Fix `Mutex::new(ptr: Ptr[T])` instantiation
- [ ] 2.1.2 Fix `Mutex::lock()` return type resolution for `MutexGuard[Ptr[T]]`
- [ ] 2.1.3 Fix `MutexGuard[Ptr[T]]` deref to get inner Ptr
- [ ] 2.1.4 Fix `*guard` assignment: `*guard = new_ptr`
- [ ] 2.1.5 Add test: `mutex_ptr_basic.test.tml`

### 2.2 MutexGuard Operations
- [ ] 2.2.1 Fix `MutexGuard[T]` Deref behavior codegen
- [ ] 2.2.2 Fix `MutexGuard[T]` DerefMut behavior codegen
- [ ] 2.2.3 Fix `MutexGuard[T]` Drop behavior codegen
- [ ] 2.2.4 Add test: `mutex_guard_deref.test.tml`

## Phase 3: Fix Condvar.wait() Codegen

### 3.1 Condvar Wait with Generic Guard
- [ ] 3.1.1 Fix `Condvar::wait[T](guard: MutexGuard[T]) -> MutexGuard[T]`
- [ ] 3.1.2 Fix guard ownership transfer through wait
- [ ] 3.1.3 Fix `wait_timeout_ms` return type `(MutexGuard[T], Bool)`
- [ ] 3.1.4 Add test: `condvar_wait_generic.test.tml`

### 3.2 Condvar Notification
- [ ] 3.2.1 Verify `notify_one()` codegen (may already work)
- [ ] 3.2.2 Verify `notify_all()` codegen (may already work)

## Phase 4: Fix Nested Generic Structs

### 4.1 ChannelInner[T] Pattern
- [ ] 4.1.1 Fix struct with multiple generic fields: Mutex + Condvar + Atomics
- [ ] 4.1.2 Fix `Arc::new(ChannelInner { ... })` construction
- [ ] 4.1.3 Fix field access: `arc.inner.field.method()`
- [ ] 4.1.4 Add test: `nested_generic_struct.test.tml`

### 4.2 Arc[ComplexStruct[T]]
- [ ] 4.2.1 Fix `Arc[T]::duplicate()` when T is complex generic struct
- [ ] 4.2.2 Fix `Arc[T]` Drop when T contains Mutex/Condvar
- [ ] 4.2.3 Add test: `arc_complex_struct.test.tml`

## Phase 5: Fix Lowlevel Pointer Operations

### 5.1 Pointer Dereference in Generic Context
- [ ] 5.1.1 Fix `lowlevel { (*ptr).field = value }` type resolution
- [ ] 5.1.2 Fix `lowlevel { (*ptr).field }` read access
- [ ] 5.1.3 Fix pointer arithmetic in generic functions
- [ ] 5.1.4 Add test: `lowlevel_ptr_generic.test.tml`

### 5.2 Pointer Assignment
- [ ] 5.2.1 Fix `*guard = new_value` when guard derefs to Ptr[T]
- [ ] 5.2.2 Fix null pointer comparison in generic context

## Phase 6: MPSC Integration Tests

### 6.1 Basic Channel Operations
- [ ] 6.1.1 Test `channel[I32]()` creation
- [ ] 6.1.2 Test `Sender::send(value)` single value
- [ ] 6.1.3 Test `Receiver::recv()` single value
- [ ] 6.1.4 Test `Receiver::try_recv()` empty channel
- [ ] 6.1.5 Test `Receiver::is_empty()` and `len()`

### 6.2 Channel Lifecycle
- [ ] 6.2.1 Test send/recv multiple values (FIFO order)
- [ ] 6.2.2 Test `Sender::duplicate()` (multiple producers)
- [ ] 6.2.3 Test sender drop → receiver gets RecvError
- [ ] 6.2.4 Test receiver drop → sender gets SendError
- [ ] 6.2.5 Test all senders drop → channel closes

### 6.3 Stress Tests
- [ ] 6.3.1 Test 1000 send/recv operations
- [ ] 6.3.2 Test multiple senders (3+) with single receiver
- [ ] 6.3.3 Test rapid send/recv alternation

## Phase 7: Documentation and Cleanup

### 7.1 Update Tests
- [ ] 7.1.1 Move `sync_mpsc.test.tml` from error-types-only to full channel tests
- [ ] 7.1.2 Add performance benchmark for MPSC vs LockFreeQueue

### 7.2 Update Task Status
- [ ] 7.2.1 Update `thread-safe-native/tasks.md` Phase 8.3 items to reflect actual status
- [ ] 7.2.2 Add note about Mutex[Ptr[T]] limitation if not fully fixed

## Files to Modify

```
compiler/src/codegen/
├── expr/method.cpp       # Fix generic method resolution for Mutex/Condvar
├── expr/struct.cpp       # Fix nested generic struct instantiation
├── core/generic.cpp      # Fix generic type substitution for Ptr[T] in Mutex
└── core/types.cpp        # Fix type resolution for MutexGuard[Ptr[T]]

compiler/src/types/
└── env_lookups.cpp       # Fix method lookup for generic struct fields

lib/std/tests/
├── sync_mpsc.test.tml    # Full MPSC tests (currently error-types only)
└── pending/
    └── mutex_ptr.test.tml  # Minimal repro tests
```

## Validation Checklist

- [ ] V.1 `Mutex[Ptr[T]]` compiles and runs correctly
- [ ] V.2 `Condvar.wait(MutexGuard[T])` works with generic T
- [ ] V.3 `Arc[ChannelInner[T]]` compiles with all fields accessible
- [ ] V.4 `lowlevel { (*ptr).field = value }` works in generic context
- [ ] V.5 All sync_mpsc.test.tml channel tests pass
- [ ] V.6 No regression in sync_lockfree, sync_arc, sync_atomic tests
- [ ] V.7 Full test suite (1924+ tests) passes
