# Notes: MPSC Channel Codegen Fixes

## Problem Analysis

### What Works

1. **LockFreeQueue[T]** - Full implementation works
   - Uses `AtomicPtr[Node[T]]` for thread-safe pointer operations
   - 17 tests passing in `sync_lockfree.test.tml`

2. **Arc[T]** with simple T - Works
   - 30 tests passing in `sync_arc.test.tml`
   - `duplicate()`, `strong_count()`, `Weak` references all work

3. **Atomic types** - All work
   - AtomicI32, AtomicI64, AtomicU32, AtomicU64, AtomicUsize, AtomicIsize, AtomicBool, AtomicPtr
   - 38 tests passing in `sync_atomic.test.tml`

4. **MPSC Error types** - Work
   - `SendError[T]`, `RecvError`, `TryRecvError`
   - 6 tests passing in current `sync_mpsc.test.tml`

### What Fails

The MPSC channel uses this structure:

```tml
type ChannelInner[T] {
    head: Mutex[Ptr[ChannelNode[T]]],      // FAILS - Mutex with Ptr[T]
    tail: Mutex[Ptr[ChannelNode[T]]],      // FAILS
    not_empty: Condvar,                     // OK alone
    sender_count: AtomicUsize,              // OK
    receiver_alive: AtomicBool,             // OK
    len: AtomicUsize,                       // OK
}

type Sender[T] {
    inner: Arc[ChannelInner[T]],           // FAILS - Arc with complex nested generic
}
```

Codegen errors when compiling MPSC:
```
Unknown method: lock           (Mutex[Ptr[ChannelNode[T]]].lock())
Unknown method: wait           (Condvar.wait(MutexGuard[Ptr[...]])
Unknown method: fetch_add      (inner field access through Arc)
Unknown method: notify_one     (inner field access through Arc)
Unknown method: duplicate      (Arc[ChannelInner[T]].duplicate())
Cannot dereference non-reference type  (lowlevel { (*ptr).field })
```

### Root Cause Hypothesis

The codegen for generic methods fails when:

1. **Type parameter is itself a generic type containing Ptr**
   - `Mutex[T]` works when T = I32
   - `Mutex[T]` fails when T = Ptr[SomeStruct[U]]

2. **Method calls on fields of Arc[ComplexStruct]**
   - `Arc[SimpleStruct].duplicate()` works
   - `Arc[StructWithMutex[T]].inner.mutex_field.lock()` fails

3. **Condvar.wait() with generic guard type**
   - The guard type needs to carry through the generic parameter

### Comparison: LockFreeQueue vs MPSC

| Feature | LockFreeQueue | MPSC |
|---------|---------------|------|
| Pointer storage | `AtomicPtr[Node[T]]` | `Mutex[Ptr[ChannelNode[T]]]` |
| Synchronization | Lock-free atomics | Blocking Mutex + Condvar |
| Blocking recv | Not supported | Required (via Condvar.wait) |
| Guard types | None | `MutexGuard[Ptr[...]]` |

The LockFreeQueue avoids the problematic patterns by using only AtomicPtr operations.

### Minimal Repro Cases

1. **Mutex with generic pointer:**
```tml
type Wrapper[T] {
    ptr: Ptr[T],
}

func test() {
    var mutex: Mutex[Ptr[I32]] = Mutex::new(null as Ptr[I32])
    let guard = mutex.lock()  // ERROR: Unknown method: lock
}
```

2. **Condvar wait with generic:**
```tml
func test() {
    var mutex: Mutex[I32] = Mutex::new(42)
    let condvar: Condvar = Condvar::new()
    var guard = mutex.lock()
    guard = condvar.wait(guard)  // ERROR: Unknown method: wait
}
```

3. **Arc with nested struct:**
```tml
type Inner[T] {
    data: Mutex[T],
}

func test() {
    let arc: Arc[Inner[I32]] = Arc::new(Inner { data: Mutex::new(0) })
    let guard = arc.data.lock()  // ERROR: Unknown method: lock
}
```

## Implementation Strategy

### Phase 1: Fix Method Resolution

The codegen needs to properly resolve methods when:
- The receiver type is a generic instantiation with complex type arguments
- Field access chains like `arc.field.method()` where field is also generic

Key files:
- `compiler/src/codegen/expr/method.cpp` - method call codegen
- `compiler/src/types/env_lookups.cpp` - method lookup

### Phase 2: Fix Lowlevel Pointer Ops

The `lowlevel` block type inference fails for pointer dereference when the pointer type is generic. The expression `(*ptr).field` returns `()` instead of the field type.

Key files:
- `compiler/src/codegen/expr/unary.cpp` - dereference codegen
- `compiler/src/codegen/core/types.cpp` - type resolution

### Phase 3: Integration

After fixing the core issues, the full MPSC implementation should work without changes to `mpsc.tml`.

## Test Strategy

1. Create minimal repro tests in `lib/std/tests/pending/`
2. Fix codegen for each pattern
3. Move repro tests to active tests as they pass
4. Enable full MPSC tests
5. Run full test suite to verify no regressions

## References

- LockFreeQueue implementation: `lib/std/src/sync/queue.tml`
- MPSC implementation: `lib/std/src/sync/mpsc.tml`
- Mutex implementation: `lib/std/src/sync/mutex.tml`
- Arc implementation: `lib/std/src/sync/arc.tml`
- Condvar implementation: `lib/std/src/sync/condvar.tml`
