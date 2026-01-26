# Tasks: MPSC Channel Codegen Fixes

**Status**: In Progress (60%)

## Completed Fixes

These issues have been resolved:

- [x] 2.2.3 Fix `MutexGuard[T]` Drop behavior codegen - load ptr before calling unlock
- [x] 3.1.1 Fix `Condvar::wait[T](guard: MutexGuard[T]) -> MutexGuard[T]` type resolution
- [x] 2.1.1 Fix `Mutex::new(ptr: Ptr[T])` instantiation - Ptr[T] mangling in imported generics
- [x] 1.1.1 Minimal repro `Mutex[Ptr[I32]]` - now passes (mpsc_repro_mutex_ptr.test.tml)
- [x] 1.1.2 Minimal repro `Condvar.wait(MutexGuard[I32])` - now passes (condvar_wait_repro.test.tml)
- [x] Method.cpp refactoring for maintainability (2609 -> 1612 lines)
- [x] 4.1.1 Fix internal struct registration (ArcInner now in internal_structs map)
- [x] 4.1.2 Fix Ptr[T] dereference in gen_field for NamedType("Ptr")
- [x] 4.1.3 Fix Ptr[T] dereference in infer_expr_type for NamedType("Ptr")
- [x] 4.1.4 Fix module imports in arc.tml and atomic.tml (use std:: prefix)
- [x] 4.1.5 Arc::drop now works correctly (arc_simple.test.tml passes)

## Current Blockers

### Primary Issue: MPSC Channel Method Calls

When calling methods through complex nested field access in MPSC channel:

```tml
type ChannelInner[T] {
    sender_count: AtomicUsize,
    head: Mutex[Ptr[ChannelNode[T]]],
    // ...
}

let inner: Arc[ChannelInner[T]] = Arc::new(...)
inner.sender_count.fetch_sub(1)  // ERROR: Unknown method: fetch_sub
inner.head.lock()                 // ERROR: Unknown method: lock
inner.duplicate()                 // ERROR: Unknown method: duplicate
```

The issue is that field access through Arc doesn't properly resolve the method lookup.

## Phase 1: Diagnose Root Causes

### 1.1 Identify Specific Codegen Failures
- [x] 1.1.1 Create minimal repro: `Mutex[Ptr[I32]]` basic usage
- [x] 1.1.2 Create minimal repro: `Condvar.wait(MutexGuard[I32])`
- [ ] 1.1.3 Create minimal repro: Nested generic struct with Mutex field
- [ ] 1.1.4 Create minimal repro: `lowlevel { (*ptr).field = value }` in generic context
- [ ] 1.1.5 Compare IR output between working LockFreeQueue and failing MPSC

### 1.2 Document Failure Points
- [x] 1.2.1 Map "Unknown method: lock" error - happens through Arc field access
- [x] 1.2.2 Map "Unknown method: wait" error - fixed for direct calls
- [ ] 1.2.3 Map "Cannot dereference non-reference type" error to codegen source
- [ ] 1.2.4 Identify why `duplicate()` (Arc clone) fails in nested generic context

## Phase 2: Fix Mutex[Ptr[T]] Codegen

### 2.1 Generic Pointer in Mutex
- [x] 2.1.1 Fix `Mutex::new(ptr: Ptr[T])` instantiation
- [x] 2.1.2 Fix `Mutex::lock()` return type resolution for `MutexGuard[Ptr[T]]`
- [ ] 2.1.3 Fix `MutexGuard[Ptr[T]]` deref to get inner Ptr
- [ ] 2.1.4 Fix `*guard` assignment: `*guard = new_ptr`
- [x] 2.1.5 Test passes: `mpsc_repro_mutex_ptr.test.tml`

### 2.2 MutexGuard Operations
- [ ] 2.2.1 Fix `MutexGuard[T]` Deref behavior codegen
- [ ] 2.2.2 Fix `MutexGuard[T]` DerefMut behavior codegen
- [x] 2.2.3 Fix `MutexGuard[T]` Drop behavior codegen

## Phase 3: Fix Condvar.wait() Codegen

### 3.1 Condvar Wait with Generic Guard
- [x] 3.1.1 Fix `Condvar::wait[T](guard: MutexGuard[T]) -> MutexGuard[T]`
- [x] 3.1.2 Fix guard ownership transfer through wait
- [x] 3.1.3 Fix `wait_timeout_ms` return type `(MutexGuard[T], Bool)`
- [x] 3.1.4 Test passes: `condvar_wait_repro.test.tml`

### 3.2 Condvar Notification
- [x] 3.2.1 Verify `notify_one()` codegen (works for direct calls)
- [x] 3.2.2 Verify `notify_all()` codegen (works for direct calls)

## Phase 4: Fix Arc[ComplexStruct[T]] Field Access (CURRENT FOCUS)

### 4.1 Arc Field Method Resolution
- [ ] 4.1.1 Fix `arc.field.method()` chain when arc wraps generic struct
- [ ] 4.1.2 Fix method lookup for fields accessed through deref chains
- [ ] 4.1.3 Fix `Arc::duplicate()` on complex generic types
- [ ] 4.1.4 Add test: `arc_complex_field_access.test.tml`

### 4.2 ChannelInner[T] Pattern
- [ ] 4.2.1 Fix struct with multiple generic fields: Mutex + Condvar + Atomics
- [ ] 4.2.2 Fix `Arc::new(ChannelInner { ... })` construction
- [ ] 4.2.3 Fix field access: `arc.field.method()`
- [ ] 4.2.4 Add test: `nested_generic_struct.test.tml`

## Phase 5: Fix Lowlevel Pointer Operations

### 5.1 Pointer Dereference in Generic Context
- [ ] 5.1.1 Fix `lowlevel { (*ptr).field = value }` type resolution
- [ ] 5.1.2 Fix `lowlevel { (*ptr).field }` read access
- [ ] 5.1.3 Fix pointer arithmetic in generic functions
- [ ] 5.1.4 Add test: `lowlevel_ptr_generic.test.tml`

## Phase 6: MPSC Integration Tests

### 6.1 Basic Channel Operations
- [ ] 6.1.1 Test `channel[I32]()` creation
- [ ] 6.1.2 Test `Sender::send(value)` single value
- [ ] 6.1.3 Test `Receiver::recv()` single value
- [ ] 6.1.4 Test `Receiver::try_recv()` empty channel
- [ ] 6.1.5 Test `Receiver::is_empty()` and `len()`

## Files to Modify

```
compiler/src/codegen/
├── expr/method.cpp       # Fix method resolution through Arc deref
├── expr/struct.cpp       # Fix field access on generic wrapped types
├── core/generic.cpp      # Fix generic type substitution
└── core/types.cpp        # Fix type resolution for wrapped fields

compiler/src/types/
└── env_lookups.cpp       # Fix method lookup for chained field access
```

## Validation Checklist

- [x] V.1 `Mutex[Ptr[T]]` compiles and runs correctly
- [x] V.2 `Condvar.wait(MutexGuard[T])` works with generic T
- [ ] V.3 `Arc[ChannelInner[T]]` compiles with all fields accessible
- [ ] V.4 `lowlevel { (*ptr).field = value }` works in generic context
- [ ] V.5 All sync_mpsc.test.tml channel tests pass
- [x] V.6 No regression in sync_lockfree, sync_arc, sync_atomic tests
- [x] V.7 Full test suite (860 tests) passes
