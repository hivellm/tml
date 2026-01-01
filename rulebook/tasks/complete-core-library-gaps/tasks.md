# Tasks: Complete Core Library Gaps

## Progress: 0% (0/124 tasks complete)

---

## Phase 1: Iterator System Completion (P0)

### 1.1 Iterator Consumer Methods
**File**: `lib/core/src/iter/traits.tml`

- [ ] 1.1.1 Add `size_hint(this) -> (I64, Maybe[I64])` to Iterator
- [ ] 1.1.2 Add `count(this) -> I64` - consume and count elements
- [ ] 1.1.3 Add `last(this) -> Maybe[This::Item]` - get last element
- [ ] 1.1.4 Add `nth(mut this, n: I64) -> Maybe[This::Item]` - get nth element
- [ ] 1.1.5 Add `advance_by(mut this, n: I64) -> Outcome[Unit, I64]` - skip n elements
- [ ] 1.1.6 Add `fold[B](this, init: B, f: func(B, This::Item) -> B) -> B`
- [ ] 1.1.7 Add `reduce(this, f: func(This::Item, This::Item) -> This::Item) -> Maybe[This::Item]`
- [ ] 1.1.8 Add `try_fold[B, E](this, init: B, f: func(B, This::Item) -> Outcome[B, E]) -> Outcome[B, E]`
- [ ] 1.1.9 Add `all(this, pred: func(This::Item) -> Bool) -> Bool`
- [ ] 1.1.10 Add `any(this, pred: func(This::Item) -> Bool) -> Bool`
- [ ] 1.1.11 Add `find(this, pred: func(ref This::Item) -> Bool) -> Maybe[This::Item]`
- [ ] 1.1.12 Add `find_map[B](this, f: func(This::Item) -> Maybe[B]) -> Maybe[B]`
- [ ] 1.1.13 Add `position(this, pred: func(This::Item) -> Bool) -> Maybe[I64]`
- [ ] 1.1.14 Add `max(this) -> Maybe[This::Item] where This::Item: Ord`
- [ ] 1.1.15 Add `min(this) -> Maybe[This::Item] where This::Item: Ord`
- [ ] 1.1.16 Add `max_by(this, compare: func(ref This::Item, ref This::Item) -> Ordering) -> Maybe[This::Item]`
- [ ] 1.1.17 Add `min_by(this, compare: func(ref This::Item, ref This::Item) -> Ordering) -> Maybe[This::Item]`
- [ ] 1.1.18 Add `sum(this) -> This::Item where This::Item: Add + Default`
- [ ] 1.1.19 Add `product(this) -> This::Item where This::Item: Mul + Default`
- [ ] 1.1.20 Add `collect[C: FromIterator[This::Item]](this) -> C`
- [ ] 1.1.21 Add `partition[C](this, pred: func(ref This::Item) -> Bool) -> (C, C)`
- [ ] 1.1.22 Add `for_each(this, f: func(This::Item))`
- [ ] 1.1.23 Add `eq[I: Iterator](this, other: I) -> Bool`
- [ ] 1.1.24 Add `cmp[I: Iterator](this, other: I) -> Ordering`
- [ ] 1.1.25 Write tests for iterator consumer methods
- [ ] 1.1.26 Verify test coverage ≥95%

### 1.2 Iterator Adapter Implementations
**File**: `lib/core/src/iter/adapters.tml`

- [ ] 1.2.1 Implement `Iterator` for `Map[I, F]` (requires closures)
- [ ] 1.2.2 Implement `Iterator` for `Filter[I, P]` (requires closures)
- [ ] 1.2.3 Implement `Iterator` for `FilterMap[I, F]` (requires closures)
- [ ] 1.2.4 Implement `Iterator` for `Flatten[I]`
- [ ] 1.2.5 Implement `Iterator` for `FlatMap[I, F]` (requires closures)
- [ ] 1.2.6 Implement `Iterator` for `Peekable[I]` with `peek()` method
- [ ] 1.2.7 Implement `Iterator` for `TakeWhile[I, P]` (requires closures)
- [ ] 1.2.8 Implement `Iterator` for `SkipWhile[I, P]` (requires closures)
- [ ] 1.2.9 Implement `Iterator` for `Cloned[I]`
- [ ] 1.2.10 Implement `Iterator` for `Copied[I]`
- [ ] 1.2.11 Implement `Iterator` for `Cycle[I]`
- [ ] 1.2.12 Implement `Iterator` for `Rev[I]` (requires DoubleEndedIterator)
- [ ] 1.2.13 Implement `Iterator` for `Inspect[I, F]` (requires closures)
- [ ] 1.2.14 Implement `Iterator` for `Scan[I, St, F]` (requires closures)
- [ ] 1.2.15 Implement `Iterator` for `Intersperse[I]`
- [ ] 1.2.16 Write tests for adapter implementations
- [ ] 1.2.17 Verify test coverage ≥95%

### 1.3 Generic Iterator Sources
**File**: `lib/core/src/iter/sources.tml`

- [ ] 1.3.1 Add generic `Empty[T]` type
- [ ] 1.3.2 Add `empty[T]() -> Empty[T]` function
- [ ] 1.3.3 Add generic `Once[T]` type
- [ ] 1.3.4 Add `once[T](value: T) -> Once[T]` function
- [ ] 1.3.5 Add generic `Repeat[T]` type (infinite)
- [ ] 1.3.6 Add `repeat[T](value: T) -> Repeat[T]` function
- [ ] 1.3.7 Add generic `RepeatN[T]` type
- [ ] 1.3.8 Add `repeat_n[T](value: T, n: I64) -> RepeatN[T]` function
- [ ] 1.3.9 Add `FromFn[T, F]` type
- [ ] 1.3.10 Add `from_fn[T](f: func() -> Maybe[T]) -> FromFn[T]` function
- [ ] 1.3.11 Add `Successors[T, F]` type
- [ ] 1.3.12 Add `successors[T](first: Maybe[T], succ: func(ref T) -> Maybe[T])` function
- [ ] 1.3.13 Deprecate/remove type-specific functions (empty_i32, etc.)
- [ ] 1.3.14 Write tests for generic sources
- [ ] 1.3.15 Verify test coverage ≥95%

---

## Phase 2: Operator Completion (P0)

### 2.1 Deref/DerefMut Behaviors
**File**: `lib/core/src/ops.tml`

- [ ] 2.1.1 Define `Deref` behavior with `type Target` and `deref()` method
- [ ] 2.1.2 Define `DerefMut` behavior extending Deref with `deref_mut()` method
- [ ] 2.1.3 Document Deref coercion semantics
- [ ] 2.1.4 Write tests for Deref/DerefMut
- [ ] 2.1.5 Verify test coverage ≥95%

### 2.2 Bitwise Assignment Operators
**File**: `lib/core/src/ops.tml`

- [ ] 2.2.1 Define `BitAndAssign` behavior with `bitand_assign()` method
- [ ] 2.2.2 Define `BitOrAssign` behavior with `bitor_assign()` method
- [ ] 2.2.3 Define `BitXorAssign` behavior with `bitxor_assign()` method
- [ ] 2.2.4 Define `ShlAssign` behavior with `shl_assign()` method
- [ ] 2.2.5 Define `ShrAssign` behavior with `shr_assign()` method
- [ ] 2.2.6 Implement bitwise assign for I8, I16, I32, I64
- [ ] 2.2.7 Implement bitwise assign for U8, U16, U32, U64
- [ ] 2.2.8 Write tests for bitwise assignment operators
- [ ] 2.2.9 Verify test coverage ≥95%

### 2.3 Range Types
**File**: `lib/core/src/ops/range.tml` (new)

- [ ] 2.3.1 Create `lib/core/src/ops/range.tml` file
- [ ] 2.3.2 Define `Bound[T]` type (Included, Excluded, Unbounded)
- [ ] 2.3.3 Define `Range[Idx]` type with `start` and `end` fields
- [ ] 2.3.4 Define `RangeFrom[Idx]` type with `start` field
- [ ] 2.3.5 Define `RangeTo[Idx]` type with `end` field
- [ ] 2.3.6 Define `RangeInclusive[Idx]` type with `start`, `end`, `exhausted` fields
- [ ] 2.3.7 Define `RangeFull` type (empty struct)
- [ ] 2.3.8 Define `RangeBounds[T]` behavior with `start_bound()`, `end_bound()`, `contains()`
- [ ] 2.3.9 Implement `RangeBounds` for all range types
- [ ] 2.3.10 Implement `Iterator` for `Range[I64]`
- [ ] 2.3.11 Implement `Iterator` for `RangeInclusive[I64]`
- [ ] 2.3.12 Implement `DoubleEndedIterator` for `Range[I64]`
- [ ] 2.3.13 Implement `ExactSizeIterator` for `Range[I64]`
- [ ] 2.3.14 Update `lib/core/src/ops.tml` to include range module
- [ ] 2.3.15 Write tests for range types
- [ ] 2.3.16 Verify test coverage ≥95%

### 2.4 Function Traits
**File**: `lib/core/src/ops/function.tml` (new)

- [ ] 2.4.1 Create `lib/core/src/ops/function.tml` file
- [ ] 2.4.2 Define `FnOnce[Args]` behavior with `type Output` and `call_once()`
- [ ] 2.4.3 Define `FnMut[Args]` behavior extending FnOnce with `call_mut()`
- [ ] 2.4.4 Define `Fn[Args]` behavior extending FnMut with `call()`
- [ ] 2.4.5 Document closure trait hierarchy
- [ ] 2.4.6 Update `lib/core/src/ops.tml` to include function module
- [ ] 2.4.7 Write tests for function traits (basic)
- [ ] 2.4.8 **[COMPILER]** Add closure syntax support
- [ ] 2.4.9 **[COMPILER]** Generate Fn trait impls for closures

---

## Phase 3: Marker Types (P0)

### 3.1 PhantomData
**File**: `lib/core/src/marker.tml`

- [ ] 3.1.1 Define `PhantomData[T]` type (empty struct)
- [ ] 3.1.2 Implement `Default` for `PhantomData[T]`
- [ ] 3.1.3 Implement `Duplicate` for `PhantomData[T]`
- [ ] 3.1.4 Implement `Copy` for `PhantomData[T]`
- [ ] 3.1.5 Implement `Send` for `PhantomData[T]` where T: Send
- [ ] 3.1.6 Implement `Sync` for `PhantomData[T]` where T: Sync
- [ ] 3.1.7 Implement `PartialEq`, `Eq` for `PhantomData[T]`
- [ ] 3.1.8 Implement `PartialOrd`, `Ord` for `PhantomData[T]`
- [ ] 3.1.9 Implement `Hash` for `PhantomData[T]`
- [ ] 3.1.10 Document PhantomData use cases
- [ ] 3.1.11 Write tests for PhantomData
- [ ] 3.1.12 Verify test coverage ≥95%

### 3.2 PhantomPinned
**File**: `lib/core/src/marker.tml`

- [ ] 3.2.1 Define `PhantomPinned` type (empty struct)
- [ ] 3.2.2 Implement `Default` for `PhantomPinned`
- [ ] 3.2.3 Implement `Duplicate`, `Copy` for `PhantomPinned`
- [ ] 3.2.4 Ensure `Unpin` is NOT implemented for `PhantomPinned`
- [ ] 3.2.5 Document PhantomPinned use cases
- [ ] 3.2.6 Write tests for PhantomPinned
- [ ] 3.2.7 Verify test coverage ≥95%

---

## Phase 4: Pin Module (P1)

### 4.1 Pin Type
**File**: `lib/core/src/pin.tml` (new)

- [ ] 4.1.1 Create `lib/core/src/pin.tml` file
- [ ] 4.1.2 Define `Pin[P]` type wrapping pointer P
- [ ] 4.1.3 Implement `new_unchecked(pointer: P) -> Pin[P]` (lowlevel)
- [ ] 4.1.4 Implement `as_ref(this) -> Pin[ref P::Target]` for `Pin[P: Deref]`
- [ ] 4.1.5 Implement `as_mut(mut this) -> Pin[mut ref P::Target]` for `Pin[P: DerefMut]`
- [ ] 4.1.6 Implement `new(pointer: ref T) -> Pin[ref T]` for `T: Unpin`
- [ ] 4.1.7 Implement `new(pointer: mut ref T) -> Pin[mut ref T]` for `T: Unpin`
- [ ] 4.1.8 Implement `get_ref(this) -> ref T` for `Pin[ref T]`
- [ ] 4.1.9 Implement `get_mut(mut this) -> mut ref T` for `Pin[mut ref T]` where T: Unpin
- [ ] 4.1.10 Implement `into_inner(this) -> P` for `Pin[P]` where P::Target: Unpin
- [ ] 4.1.11 Implement `Deref` for `Pin[P]`
- [ ] 4.1.12 Implement `DerefMut` for `Pin[P]` where P::Target: Unpin
- [ ] 4.1.13 Document Pin safety guarantees
- [ ] 4.1.14 Update `lib/core/src/mod.tml` to include pin module
- [ ] 4.1.15 Write tests for Pin
- [ ] 4.1.16 Verify test coverage ≥95%

---

## Phase 5: Future/Task Modules (P1)

### 5.1 Poll Type
**File**: `lib/core/src/future/poll.tml` (new)

- [ ] 5.1.1 Create `lib/core/src/future/` directory
- [ ] 5.1.2 Create `lib/core/src/future/poll.tml` file
- [ ] 5.1.3 Define `Poll[T]` type with `Ready(T)` and `Pending` variants
- [ ] 5.1.4 Implement `is_ready(this) -> Bool`
- [ ] 5.1.5 Implement `is_pending(this) -> Bool`
- [ ] 5.1.6 Implement `map[U](this, f: func(T) -> U) -> Poll[U]`
- [ ] 5.1.7 Implement `map_ok[U](this, f: func(T) -> U) -> Poll[Outcome[U, E]]`
- [ ] 5.1.8 Implement `map_err[F](this, f: func(E) -> F) -> Poll[Outcome[T, F]]`
- [ ] 5.1.9 Write tests for Poll
- [ ] 5.1.10 Verify test coverage ≥95%

### 5.2 Future Behavior
**File**: `lib/core/src/future/mod.tml` (new)

- [ ] 5.2.1 Create `lib/core/src/future/mod.tml` file
- [ ] 5.2.2 Define `Future` behavior with `type Output` and `poll()` method
- [ ] 5.2.3 Define `IntoFuture` behavior with `type IntoFuture` and `into_future()`
- [ ] 5.2.4 Implement `IntoFuture` for all `Future` types (blanket impl)
- [ ] 5.2.5 Document Future usage patterns
- [ ] 5.2.6 Update `lib/core/src/mod.tml` to include future module
- [ ] 5.2.7 Write tests for Future behavior
- [ ] 5.2.8 Verify test coverage ≥95%

### 5.3 Task Module
**Files**: `lib/core/src/task/` (new directory)

- [ ] 5.3.1 Create `lib/core/src/task/` directory
- [ ] 5.3.2 Create `lib/core/src/task/wake.tml`
- [ ] 5.3.3 Define `RawWakerVTable` type with function pointers
- [ ] 5.3.4 Define `RawWaker` type with data pointer and vtable
- [ ] 5.3.5 Define `Waker` type wrapping RawWaker
- [ ] 5.3.6 Implement `wake(this)` for Waker
- [ ] 5.3.7 Implement `wake_by_ref(this)` for Waker
- [ ] 5.3.8 Implement `Duplicate` for Waker
- [ ] 5.3.9 Implement `Drop` for Waker
- [ ] 5.3.10 Create `lib/core/src/task/context.tml`
- [ ] 5.3.11 Define `Context` type with waker reference
- [ ] 5.3.12 Implement `from_waker(waker: ref Waker) -> Context`
- [ ] 5.3.13 Implement `waker(this) -> ref Waker`
- [ ] 5.3.14 Create `lib/core/src/task/mod.tml` with re-exports
- [ ] 5.3.15 Update `lib/core/src/mod.tml` to include task module
- [ ] 5.3.16 Write tests for task module
- [ ] 5.3.17 Verify test coverage ≥95%

---

## Phase 6: Numeric Traits (P2)

### 6.1 Core Numeric Behaviors
**File**: `lib/core/src/num/mod.tml` (new)

- [ ] 6.1.1 Create `lib/core/src/num/` directory
- [ ] 6.1.2 Create `lib/core/src/num/mod.tml` file
- [ ] 6.1.3 Define `Zero` behavior with `zero()` and `is_zero()`
- [ ] 6.1.4 Define `One` behavior with `one()`
- [ ] 6.1.5 Define `Saturating` behavior with saturating ops
- [ ] 6.1.6 Define `Wrapping` behavior with wrapping ops
- [ ] 6.1.7 Define `Checked` behavior with checked ops
- [ ] 6.1.8 Define `Overflowing` behavior with overflowing ops
- [ ] 6.1.9 Implement Zero/One for all integer types
- [ ] 6.1.10 Implement Zero/One for F32, F64
- [ ] 6.1.11 Implement Saturating for all integer types
- [ ] 6.1.12 Implement Wrapping for all integer types
- [ ] 6.1.13 Implement Checked for all integer types
- [ ] 6.1.14 Implement Overflowing for all integer types
- [ ] 6.1.15 Update `lib/core/src/mod.tml` to include num module
- [ ] 6.1.16 Write tests for numeric behaviors
- [ ] 6.1.17 Verify test coverage ≥95%

### 6.2 Integer Methods
**File**: `lib/core/src/num/mod.tml` (expand)

- [ ] 6.2.1 Add `abs()`, `signum()` for signed integers
- [ ] 6.2.2 Add `is_positive()`, `is_negative()` for signed integers
- [ ] 6.2.3 Add `count_ones()`, `count_zeros()` for all integers
- [ ] 6.2.4 Add `leading_zeros()`, `trailing_zeros()` for all integers
- [ ] 6.2.5 Add `rotate_left()`, `rotate_right()` for all integers
- [ ] 6.2.6 Add `swap_bytes()`, `reverse_bits()` for all integers
- [ ] 6.2.7 Add `from_be()`, `from_le()`, `to_be()`, `to_le()` for all integers
- [ ] 6.2.8 Add `pow()` for all integers
- [ ] 6.2.9 Add `MIN`, `MAX` constants for all integers
- [ ] 6.2.10 Write tests for integer methods
- [ ] 6.2.11 Verify test coverage ≥95%

---

## Phase 7: Formatting Improvements (P2)

### 7.1 Additional Format Behaviors
**File**: `lib/core/src/fmt.tml` (expand)

- [ ] 7.1.1 Define `Binary` behavior with `fmt_binary()` method
- [ ] 7.1.2 Define `Octal` behavior with `fmt_octal()` method
- [ ] 7.1.3 Define `LowerHex` behavior with `fmt_lower_hex()` method
- [ ] 7.1.4 Define `UpperHex` behavior with `fmt_upper_hex()` method
- [ ] 7.1.5 Define `Pointer` behavior with `fmt_pointer()` method
- [ ] 7.1.6 Implement Binary for all integer types
- [ ] 7.1.7 Implement Octal for all integer types
- [ ] 7.1.8 Implement LowerHex for all integer types
- [ ] 7.1.9 Implement UpperHex for all integer types
- [ ] 7.1.10 Implement Pointer for RawPtr, RawMutPtr, NonNull
- [ ] 7.1.11 Write tests for format behaviors
- [ ] 7.1.12 Verify test coverage ≥95%

---

## Phase 8: Memory Layout (P2)

### 8.1 Layout Type
**File**: `lib/core/src/alloc/layout.tml` (new)

- [ ] 8.1.1 Create `lib/core/src/alloc/layout.tml` file
- [ ] 8.1.2 Define `Layout` type with size and align fields
- [ ] 8.1.3 Define `LayoutError` type
- [ ] 8.1.4 Implement `from_size_align()` with validation
- [ ] 8.1.5 Implement `from_size_align_unchecked()`
- [ ] 8.1.6 Implement `new[T]()` to get layout of type
- [ ] 8.1.7 Implement `for_value[T](t: ref T)` to get layout of value
- [ ] 8.1.8 Implement `size()`, `align()` getters
- [ ] 8.1.9 Implement `repeat(n: I64)` for array layouts
- [ ] 8.1.10 Implement `pad_to_align()` for padding
- [ ] 8.1.11 Implement `array[T](n: I64)` convenience function
- [ ] 8.1.12 Implement `Display`, `Debug` for LayoutError
- [ ] 8.1.13 Implement `Error` for LayoutError
- [ ] 8.1.14 Update `lib/core/src/alloc/mod.tml` to include layout
- [ ] 8.1.15 Write tests for Layout
- [ ] 8.1.16 Verify test coverage ≥95%

---

## Phase 9: Hash Improvements (P2)

### 9.1 BuildHasher and DefaultHasher
**File**: `lib/core/src/hash.tml` (expand)

- [ ] 9.1.1 Define `BuildHasher` behavior with `type Hasher` and `build_hasher()`
- [ ] 9.1.2 Define `DefaultHasher` type with state field
- [ ] 9.1.3 Implement `Hasher` for `DefaultHasher`
- [ ] 9.1.4 Define `RandomState` type
- [ ] 9.1.5 Implement `BuildHasher` for `RandomState`
- [ ] 9.1.6 Implement `Default` for `RandomState`
- [ ] 9.1.7 Write tests for BuildHasher
- [ ] 9.1.8 Verify test coverage ≥95%

---

## Phase 10: Utility Types (P3)

### 10.1 Duration Type
**File**: `lib/core/src/time.tml` (new)

- [ ] 10.1.1 Create `lib/core/src/time.tml` file
- [ ] 10.1.2 Define `Duration` type with secs and nanos fields
- [ ] 10.1.3 Define constants (NANOS_PER_SEC, etc.)
- [ ] 10.1.4 Implement `new()`, `from_secs()`, `from_millis()`
- [ ] 10.1.5 Implement `from_micros()`, `from_nanos()`
- [ ] 10.1.6 Implement `is_zero()`, `as_secs()`, `subsec_nanos()`
- [ ] 10.1.7 Implement `as_millis()`, `as_micros()`, `as_nanos()`
- [ ] 10.1.8 Implement `checked_add()`, `checked_sub()`
- [ ] 10.1.9 Implement `saturating_add()`, `saturating_sub()`
- [ ] 10.1.10 Implement `Add`, `Sub` for Duration
- [ ] 10.1.11 Implement `Mul[I32]`, `Div[I32]` for Duration
- [ ] 10.1.12 Implement `PartialEq`, `Eq`, `PartialOrd`, `Ord` for Duration
- [ ] 10.1.13 Implement `Default`, `Display`, `Debug` for Duration
- [ ] 10.1.14 Define `ZERO`, `MAX` constants
- [ ] 10.1.15 Update `lib/core/src/mod.tml` to include time module
- [ ] 10.1.16 Write tests for Duration
- [ ] 10.1.17 Verify test coverage ≥95%

### 10.2 Any/TypeId
**File**: `lib/core/src/any.tml` (new)

- [ ] 10.2.1 Create `lib/core/src/any.tml` file
- [ ] 10.2.2 Define `TypeId` type with id field
- [ ] 10.2.3 Implement `of[T: 'static]() -> TypeId` (requires compiler support)
- [ ] 10.2.4 Implement `PartialEq`, `Eq` for TypeId
- [ ] 10.2.5 Implement `Hash` for TypeId
- [ ] 10.2.6 Implement `Debug` for TypeId
- [ ] 10.2.7 Define `Any` behavior with `type_id()` method
- [ ] 10.2.8 Document Any/TypeId usage
- [ ] 10.2.9 Update `lib/core/src/mod.tml` to include any module
- [ ] 10.2.10 Write tests for Any/TypeId
- [ ] 10.2.11 Verify test coverage ≥95%

---

## Summary

| Phase | Description | Priority | Tasks | Status |
|-------|-------------|----------|-------|--------|
| 1 | Iterator System | P0 | 58 | 0% |
| 2 | Operator Completion | P0 | 30 | 0% |
| 3 | Marker Types | P0 | 19 | 0% |
| 4 | Pin Module | P1 | 16 | 0% |
| 5 | Future/Task | P1 | 27 | 0% |
| 6 | Numeric Traits | P2 | 28 | 0% |
| 7 | Formatting | P2 | 12 | 0% |
| 8 | Memory Layout | P2 | 16 | 0% |
| 9 | Hash Improvements | P2 | 8 | 0% |
| 10 | Utility Types | P3 | 28 | 0% |
| **Total** | | | **124** | **0%** |

---

## Dependencies

```
Phase 1 (Iterator) ──────────────────────────┐
  └── Requires: Closure support (compiler)    │
                                              │
Phase 2.1 (Deref) ───────────────────────────┼───┐
  └── Required by: Smart pointers             │   │
                                              │   │
Phase 2.4 (Fn traits) ───────────────────────┤   │
  └── Requires: Closure support (compiler)    │   │
                                              │   │
Phase 3.1 (PhantomData) ─────────────────────┤   │
  └── Required by: Safe generic wrappers      │   │
                                              │   │
Phase 4 (Pin) ───────────────────────────────┼───┘
  └── Requires: Deref/DerefMut                │
  └── Required by: Futures                    │
                                              │
Phase 5 (Future/Task) ───────────────────────┘
  └── Requires: Pin
  └── Required by: async/await (compiler)
```

---

## Blocked Items

The following items are blocked on compiler features:

| Item | Blocker | Phase |
|------|---------|-------|
| Iterator adapters (Map, Filter, etc.) | Closure support | 1.2 |
| Iterator consumers (fold, collect, etc.) | Closure support | 1.1 |
| Generic iterator sources | Closure support | 1.3 |
| Fn traits implementation | Closure support | 2.4 |
| TypeId::of | 'static lifetime | 10.2 |
