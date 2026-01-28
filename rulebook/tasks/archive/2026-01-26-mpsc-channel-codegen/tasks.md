# Tasks: MPSC Channel Codegen Fixes

**Status**: Complete (100%)

## Phase 1: Diagnose Root Causes

### 1.1 Identify Specific Codegen Failures
- [x] 1.1.1 Create minimal repro: `Mutex[Ptr[I32]]` basic usage
- [x] 1.1.2 Create minimal repro: `Condvar.wait(MutexGuard[I32])`
- [x] 1.1.3 Create minimal repro: Nested generic struct with Mutex field
- [x] 1.1.4 Create minimal repro: `lowlevel { (*ptr).field = value }` in generic context
- [x] 1.1.5 Compare IR output between working LockFreeQueue and failing MPSC

### 1.2 Document Failure Points
- [x] 1.2.1 Map "Unknown method: lock" error
- [x] 1.2.2 Map "Unknown method: wait" error
- [x] 1.2.3 Map "Cannot dereference non-reference type" error (Fixed: NamedType "Ptr" handling)
- [x] 1.2.4 Identify why `duplicate()` fails in nested generic context

## Phase 2: Fix Mutex[Ptr[T]] Codegen

### 2.1 Generic Pointer in Mutex
- [x] 2.1.1 Fix `Mutex::new(ptr: Ptr[T])` instantiation
- [x] 2.1.2 Fix `Mutex::lock()` return type resolution
- [x] 2.1.3 Fix `MutexGuard[Ptr[T]]` deref to get inner Ptr (Fixed in binary.cpp, unary.cpp)
- [x] 2.1.4 Fix `*guard` assignment: `*guard = new_ptr` (Fixed: MutexGuard deref codegen)
- [x] 2.1.5 Test passes: `mpsc_repro_mutex_ptr.test.tml`

### 2.2 MutexGuard Operations
- [x] 2.2.1 Fix `MutexGuard[T]` Deref behavior codegen (Fixed in unary.cpp)
- [x] 2.2.2 Fix `MutexGuard[T]` DerefMut behavior codegen (Fixed in binary.cpp)
- [x] 2.2.3 Fix `MutexGuard[T]` Drop behavior codegen

## Phase 3: Fix Condvar.wait() Codegen

### 3.1 Condvar Wait with Generic Guard
- [x] 3.1.1 Fix `Condvar::wait[T](guard: MutexGuard[T]) -> MutexGuard[T]`
- [x] 3.1.2 Fix guard ownership transfer through wait
- [x] 3.1.3 Fix `wait_timeout_ms` return type
- [x] 3.1.4 Test passes: `condvar_wait_repro.test.tml`

### 3.2 Condvar Notification
- [x] 3.2.1 Verify `notify_one()` codegen
- [x] 3.2.2 Verify `notify_all()` codegen

## Phase 4: Fix Arc[ComplexStruct[T]] Field Access

### 4.1 Arc Field Method Resolution
- [x] 4.1.1 Add Deref coercion in type checker
- [x] 4.1.2 Add `drop` intrinsic with type substitution
- [x] 4.1.3 Fix generic struct static method instantiation (impl Wrapper[T] syntax)
- [x] 4.1.4 Fix AtomicUsize method instantiation (add atomic to essential modules)
- [x] 4.1.5 Fix Layout/LayoutError method instantiation (add to always_generate)
- [x] 4.1.6 Arc tests pass: `arc_deref_debug.test.tml`, `arc_complex_struct.test.tml`
- [x] 4.1.7 Fix `arc.field.method()` chain in codegen (Fixed: Arc deref in unary.cpp)
- [x] 4.1.8 Fix `Arc::duplicate()` on complex generic types

### 4.2 ChannelInner[T] Pattern
- [x] 4.2.1 Fix struct with multiple generic fields
- [x] 4.2.2 Fix `Arc::new(ChannelInner { ... })` construction
- [x] 4.2.3 Fix field access: `arc.field.method()`
- [x] 4.2.4 Add test: `nested_generic_struct.test.tml`

## Phase 5: Fix Lowlevel Pointer Operations

### 5.1 Pointer Dereference in Generic Context
- [x] 5.1.1 Fix `lowlevel { (*ptr).field = value }` type resolution (Fixed: NamedType "Ptr" and smart pointer types in check_unary)
- [x] 5.1.2 Fix `lowlevel { (*ptr).field }` read access (Fixed: same as 5.1.1)
- [x] 5.1.3 Fix pointer arithmetic in generic functions
- [x] 5.1.4 Add test: `lowlevel_ptr_generic.test.tml`

## Phase 6: MPSC Integration Tests

### 6.1 Basic Channel Operations
- [x] 6.1.1 Test `channel[I32]()` creation (Fixed: double-drop bug in call.cpp, intrinsics.cpp)
- [x] 6.1.2 Test `Sender::send(value)` single value
- [x] 6.1.3 Test `Receiver::recv()` single value
- [x] 6.1.4 Test `Receiver::try_recv()` empty channel
- [x] 6.1.5 Test `Receiver::is_empty()` and `len()`

## Validation Checklist

- [x] V.1 `Mutex[Ptr[T]]` compiles and runs correctly
- [x] V.2 `Condvar.wait(MutexGuard[T])` works with generic T
- [x] V.3 `Arc[ChannelInner[T]]` compiles with all fields accessible
- [x] V.4 `lowlevel { (*ptr).field = value }` works in generic context
- [x] V.5 All sync_mpsc.test.tml channel tests pass
- [x] V.6 No regression in sync_lockfree, sync_arc, sync_atomic tests
- [x] V.7 Full test suite passes
