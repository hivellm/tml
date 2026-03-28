# Proposal: Complete Core Library Gaps

## Why

The TML core library has been implemented following Rust's core library structure, but a detailed comparison reveals significant gaps that prevent full functionality:

1. **Iterator System Incomplete**: Adapter types (`Map`, `Filter`, etc.) are defined but lack Iterator implementations. Consumer methods (`fold`, `collect`, `count`) are missing entirely.

2. **Missing Critical Operators**: `Deref`/`DerefMut` (required for smart pointers), bitwise assignment operators, and range types are absent.

3. **No Async Foundation**: `Pin`, `Future`, and `Task` modules don't exist, blocking async/await support.

4. **Numeric Gaps**: No `Zero`, `One`, `Saturating`, `Wrapping`, `Checked` traits for safe numeric operations.

5. **Missing Marker Types**: `PhantomData` is essential for safe generic programming but doesn't exist.

**Impact**: Without these gaps filled, TML cannot support:
- Idiomatic iterator chains (`iter.map().filter().collect()`)
- Smart pointer types (Box, Rc, Arc equivalents)
- Async/await programming
- Safe numeric overflow handling

## What Changes

### Phase 1: Iterator Completion (P0 - Blocking)

#### 1.1 Iterator Consumer Methods
**File**: `lib/core/src/iter/traits.tml`

Add methods to `Iterator` behavior:
- `size_hint()`, `count()`, `last()`, `nth()`, `advance_by()`
- `fold()`, `reduce()`, `try_fold()`
- `all()`, `any()`, `find()`, `find_map()`, `position()`
- `max()`, `min()`, `max_by()`, `min_by()`
- `sum()`, `product()`
- `collect()`, `partition()`
- `for_each()`
- `eq()`, `cmp()`, `partial_cmp()`

#### 1.2 Iterator Adapter Implementations
**File**: `lib/core/src/iter/adapters.tml`

Implement `Iterator` for defined adapter types:
- `Map[I, F]` - Requires closure support
- `Filter[I, P]` - Requires closure support
- `FilterMap[I, F]` - Requires closure support
- `Flatten[I]`, `FlatMap[I, F]`
- `Peekable[I]`
- `TakeWhile[I, P]`, `SkipWhile[I, P]`
- `Cloned[I]`, `Copied[I]`
- `Cycle[I]`
- `Rev[I]` - Requires `DoubleEndedIterator`
- `Inspect[I, F]`
- `Scan[I, St, F]`
- `Intersperse[I]`

#### 1.3 Generic Iterator Sources
**File**: `lib/core/src/iter/sources.tml`

Replace type-specific functions with generics:
- `empty[T]()` instead of `empty_i32()`, `empty_i64()`
- `once[T](value)` instead of `once_i32()`, `once_i64()`
- `repeat[T](value)` - infinite repetition
- `repeat_n[T](value, n)` instead of `repeat_n_i32()`, etc.
- `from_fn[T](f)` - iterator from closure
- `successors[T](first, succ)` - generate from seed

### Phase 2: Operator Completion (P0 - Blocking)

#### 2.1 Deref/DerefMut
**File**: `lib/core/src/ops.tml`

```tml
pub behavior Deref {
    type Target
    func deref(this) -> ref This::Target
}

pub behavior DerefMut: Deref {
    func deref_mut(mut this) -> mut ref This::Target
}
```

Required for: Smart pointers, string dereferencing, custom pointer types.

#### 2.2 Bitwise Assignment Operators
**File**: `lib/core/src/ops.tml`

Add behaviors:
- `BitAndAssign`, `BitOrAssign`, `BitXorAssign`
- `ShlAssign`, `ShrAssign`

Implement for all integer types.

#### 2.3 Range Types
**File**: `lib/core/src/ops/range.tml` (new)

Types:
- `Range[Idx]` - `start..end`
- `RangeFrom[Idx]` - `start..`
- `RangeTo[Idx]` - `..end`
- `RangeInclusive[Idx]` - `start..=end` (or `start through end` in TML)
- `RangeFull` - `..`

Behaviors:
- `RangeBounds[T]` with `start_bound()`, `end_bound()`, `contains()`
- `Bound[T]` type (Included, Excluded, Unbounded)

Implement `Iterator` for `Range[I64]`, `RangeInclusive[I64]`.

#### 2.4 Function Traits
**File**: `lib/core/src/ops/function.tml` (new)

```tml
pub behavior FnOnce[Args] {
    type Output
    func call_once(this, args: Args) -> This::Output
}

pub behavior FnMut[Args]: FnOnce[Args] {
    func call_mut(mut this, args: Args) -> This::Output
}

pub behavior Fn[Args]: FnMut[Args] {
    func call(this, args: Args) -> This::Output
}
```

**Note**: Full closure support requires compiler changes.

### Phase 3: Marker Types (P0 - Blocking)

#### 3.1 PhantomData
**File**: `lib/core/src/marker.tml`

```tml
pub type PhantomData[T] {}

impl[T] Default for PhantomData[T] { ... }
impl[T] Duplicate for PhantomData[T] { ... }
impl[T] Copy for PhantomData[T] {}
impl[T: Send] Send for PhantomData[T] {}
impl[T: Sync] Sync for PhantomData[T] {}
```

Required for: Safe generic pointer wrappers, variance markers.

#### 3.2 PhantomPinned
**File**: `lib/core/src/marker.tml`

```tml
pub type PhantomPinned {}
// Explicitly does NOT implement Unpin
```

Required for: Self-referential types, async futures.

### Phase 4: Pin Module (P1 - High)

**File**: `lib/core/src/pin.tml` (new)

```tml
pub type Pin[P] {
    pointer: P
}

impl[P: Deref] Pin[P] {
    pub func new_unchecked(pointer: P) -> Pin[P]
    pub func as_ref(this) -> Pin[ref P::Target]
}

impl[P: DerefMut] Pin[P] {
    pub func as_mut(mut this) -> Pin[mut ref P::Target]
}

impl[T: Unpin] Pin[ref T] {
    pub func new(pointer: ref T) -> Pin[ref T]
}

impl[T: Unpin] Pin[mut ref T] {
    pub func new(pointer: mut ref T) -> Pin[mut ref T]
    pub func get_mut(mut this) -> mut ref T
}
```

Required for: Async futures, self-referential structs.

### Phase 5: Future/Task Modules (P1 - High)

#### 5.1 Poll Type
**File**: `lib/core/src/future/poll.tml` (new)

```tml
pub type Poll[T] {
    Ready(T),
    Pending
}

impl[T] Poll[T] {
    pub func is_ready(this) -> Bool
    pub func is_pending(this) -> Bool
    pub func map[U](this, f: func(T) -> U) -> Poll[U]
}
```

#### 5.2 Future Behavior
**File**: `lib/core/src/future/mod.tml` (new)

```tml
pub behavior Future {
    type Output
    func poll(self: Pin[mut ref Self], cx: mut ref Context) -> Poll[This::Output]
}

pub behavior IntoFuture {
    type Output
    type IntoFuture: Future
    func into_future(this) -> This::IntoFuture
}
```

#### 5.3 Task Module
**Files**: `lib/core/src/task/mod.tml`, `context.tml`, `wake.tml` (new)

Types:
- `Context` - task context with waker
- `Waker` - wake handle for tasks
- `RawWaker`, `RawWakerVTable` - low-level waker support

### Phase 6: Numeric Traits (P2 - Medium)

**File**: `lib/core/src/num/mod.tml` (new)

Behaviors:
- `Zero` - `zero()`, `is_zero()`
- `One` - `one()`
- `Saturating` - `saturating_add()`, `saturating_sub()`, `saturating_mul()`
- `Wrapping` - `wrapping_add()`, `wrapping_sub()`, `wrapping_mul()`
- `Checked` - `checked_add()`, `checked_sub()`, `checked_mul()`, `checked_div()`
- `Overflowing` - `overflowing_add()`, `overflowing_sub()`, `overflowing_mul()`

Integer methods:
- `abs()`, `signum()`, `is_positive()`, `is_negative()`
- `count_ones()`, `count_zeros()`, `leading_zeros()`, `trailing_zeros()`
- `rotate_left()`, `rotate_right()`, `swap_bytes()`, `reverse_bits()`
- `pow()`, `ilog()`, `ilog2()`, `ilog10()`

### Phase 7: Formatting Improvements (P2 - Medium)

**File**: `lib/core/src/fmt.tml` (expand)

Behaviors:
- `Binary` - `fmt_binary()`
- `Octal` - `fmt_octal()`
- `LowerHex` - `fmt_lower_hex()`
- `UpperHex` - `fmt_upper_hex()`
- `Pointer` - `fmt_pointer()`

Implement for all integer and pointer types.

### Phase 8: Memory Layout (P2 - Medium)

**File**: `lib/core/src/alloc/layout.tml` (new)

```tml
pub type Layout {
    size: I64,
    align: I64
}

pub type LayoutError {}

impl Layout {
    pub func from_size_align(size: I64, align: I64) -> Outcome[Layout, LayoutError]
    pub func new[T]() -> Layout
    pub func for_value[T](t: ref T) -> Layout
    pub func size(this) -> I64
    pub func align(this) -> I64
    pub func repeat(this, n: I64) -> Outcome[(Layout, I64), LayoutError]
    pub func pad_to_align(this) -> Layout
    pub func array[T](n: I64) -> Outcome[Layout, LayoutError]
}
```

### Phase 9: Hash Improvements (P2 - Medium)

**File**: `lib/core/src/hash.tml` (expand)

```tml
pub behavior BuildHasher {
    type Hasher: Hasher
    func build_hasher(this) -> This::Hasher
}

pub type DefaultHasher { state: I64 }
pub type RandomState {}

impl BuildHasher for RandomState {
    type Hasher = DefaultHasher
    ...
}
```

### Phase 10: Utility Types (P3 - Low)

#### 10.1 Duration Type
**File**: `lib/core/src/time.tml` (new)

```tml
pub type Duration {
    secs: U64,
    nanos: U32
}

impl Duration {
    pub func from_secs(secs: U64) -> Duration
    pub func from_millis(millis: U64) -> Duration
    pub func from_micros(micros: U64) -> Duration
    pub func from_nanos(nanos: U64) -> Duration
    pub func as_secs(this) -> U64
    pub func as_millis(this) -> U128
    pub func as_micros(this) -> U128
    pub func as_nanos(this) -> U128
    pub func checked_add(this, rhs: Duration) -> Maybe[Duration]
    pub func checked_sub(this, rhs: Duration) -> Maybe[Duration]
}
```

#### 10.2 Any/TypeId
**File**: `lib/core/src/any.tml` (new)

```tml
pub type TypeId { id: U64 }

impl TypeId {
    pub func of[T: 'static]() -> TypeId
}

pub behavior Any: 'static {
    func type_id(this) -> TypeId
}
```

## Impact

- **Affected specs**: None (implementation only)

- **Affected code**:
  - `lib/core/src/iter/traits.tml` - Add iterator methods
  - `lib/core/src/iter/adapters.tml` - Implement adapters
  - `lib/core/src/iter/sources.tml` - Add generic sources
  - `lib/core/src/ops.tml` - Add Deref, assignment ops
  - `lib/core/src/ops/range.tml` - New file
  - `lib/core/src/ops/function.tml` - New file
  - `lib/core/src/marker.tml` - Add PhantomData
  - `lib/core/src/pin.tml` - New file
  - `lib/core/src/future/` - New directory
  - `lib/core/src/task/` - New directory
  - `lib/core/src/num/` - New directory
  - `lib/core/src/alloc/layout.tml` - New file
  - `lib/core/src/time.tml` - New file
  - `lib/core/src/any.tml` - New file

- **Breaking change**: NO (pure additions)

- **Compiler dependencies**:
  - Closure support needed for: Map, Filter, iterator consumer methods
  - `'static` lifetime for: Any, TypeId

## Success Criteria

### Phase 1: Iterator
- [ ] All iterator consumer methods callable
- [ ] `iter.map().filter().collect()` chains work (pending closures)
- [ ] Generic iterator sources work

### Phase 2: Operators
- [ ] `Deref`/`DerefMut` enable `*ptr` syntax
- [ ] Range types iterate correctly
- [ ] All bitwise assignment operators work

### Phase 3: Markers
- [ ] `PhantomData` usable in generic wrappers
- [ ] `PhantomPinned` prevents `Unpin` auto-impl

### Phase 4-5: Async
- [ ] `Pin` correctly wraps pointers
- [ ] `Future` behavior is implementable
- [ ] `Context` and `Waker` work for task management

### Phase 6-10: Utilities
- [ ] Numeric traits implemented for all integer types
- [ ] Hex/binary formatting works
- [ ] Duration arithmetic correct
- [ ] Layout calculations accurate

### Overall
- [ ] All new code has documentation
- [ ] All new code has tests
- [ ] No regressions in existing functionality
