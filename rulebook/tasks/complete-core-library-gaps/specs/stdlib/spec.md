# Specification: Core Library Gap Completion

## Module: stdlib

## Overview

This specification defines the API and behavior for completing the TML core library gaps identified in the comparison with Rust's `core` library.

---

## 1. Iterator Extensions

### 1.1 Consumer Methods (traits.tml)

```tml
pub behavior Iterator {
    type Item

    // Required method
    pub func next(mut this) -> Maybe[This::Item]

    // === NEW: Sizing methods ===

    /// Returns bounds on remaining elements.
    /// Lower bound is always accurate; upper may be None if unknown.
    pub func size_hint(this) -> (I64, Maybe[I64]) {
        return (0, Nothing)
    }

    /// Consumes iterator and returns element count.
    pub func count(this) -> I64 {
        let mut n: I64 = 0
        loop {
            when this.next() {
                Just(_) => n = n + 1,
                Nothing => break
            }
        }
        return n
    }

    /// Returns the last element.
    pub func last(this) -> Maybe[This::Item] {
        let mut last: Maybe[This::Item] = Nothing
        loop {
            when this.next() {
                Just(x) => last = Just(x),
                Nothing => break
            }
        }
        return last
    }

    /// Returns the nth element (0-indexed).
    pub func nth(mut this, n: I64) -> Maybe[This::Item] {
        let mut i: I64 = 0
        loop {
            when this.next() {
                Just(x) => {
                    if i == n {
                        return Just(x)
                    }
                    i = i + 1
                },
                Nothing => return Nothing
            }
        }
    }

    // === NEW: Folding methods ===

    /// Folds every element into an accumulator.
    pub func fold[B](this, init: B, f: func(B, This::Item) -> B) -> B {
        let mut acc: B = init
        loop {
            when this.next() {
                Just(x) => acc = f(acc, x),
                Nothing => break
            }
        }
        return acc
    }

    /// Reduces elements to a single value using first as initial.
    pub func reduce(this, f: func(This::Item, This::Item) -> This::Item) -> Maybe[This::Item] {
        when this.next() {
            Just(first) => {
                return Just(this.fold(first, f))
            },
            Nothing => return Nothing
        }
    }

    // === NEW: Searching methods ===

    /// Tests if all elements satisfy a predicate.
    pub func all(this, pred: func(This::Item) -> Bool) -> Bool {
        loop {
            when this.next() {
                Just(x) => {
                    if not pred(x) {
                        return false
                    }
                },
                Nothing => return true
            }
        }
    }

    /// Tests if any element satisfies a predicate.
    pub func any(this, pred: func(This::Item) -> Bool) -> Bool {
        loop {
            when this.next() {
                Just(x) => {
                    if pred(x) {
                        return true
                    }
                },
                Nothing => return false
            }
        }
    }

    /// Finds first element satisfying predicate.
    pub func find(this, pred: func(ref This::Item) -> Bool) -> Maybe[This::Item] {
        loop {
            when this.next() {
                Just(x) => {
                    if pred(ref x) {
                        return Just(x)
                    }
                },
                Nothing => return Nothing
            }
        }
    }

    /// Returns position of first element satisfying predicate.
    pub func position(this, pred: func(This::Item) -> Bool) -> Maybe[I64] {
        let mut i: I64 = 0
        loop {
            when this.next() {
                Just(x) => {
                    if pred(x) {
                        return Just(i)
                    }
                    i = i + 1
                },
                Nothing => return Nothing
            }
        }
    }

    // === NEW: Min/Max methods ===

    /// Returns the maximum element.
    pub func max(this) -> Maybe[This::Item] where This::Item: Ord {
        return this.reduce(do(a, b) if a.cmp(ref b) == Greater then a else b)
    }

    /// Returns the minimum element.
    pub func min(this) -> Maybe[This::Item] where This::Item: Ord {
        return this.reduce(do(a, b) if a.cmp(ref b) == Less then a else b)
    }

    /// Returns maximum by comparator function.
    pub func max_by(this, compare: func(ref This::Item, ref This::Item) -> Ordering) -> Maybe[This::Item] {
        return this.reduce(do(a, b) if compare(ref a, ref b) == Greater then a else b)
    }

    /// Returns minimum by comparator function.
    pub func min_by(this, compare: func(ref This::Item, ref This::Item) -> Ordering) -> Maybe[This::Item] {
        return this.reduce(do(a, b) if compare(ref a, ref b) == Less then a else b)
    }

    // === NEW: Aggregation methods ===

    /// Sums all elements.
    pub func sum(this) -> This::Item where This::Item: Add[Output = This::Item] + Default {
        return this.fold(This::Item::default(), do(acc, x) acc + x)
    }

    /// Multiplies all elements.
    pub func product(this) -> This::Item where This::Item: Mul[Output = This::Item] + One {
        return this.fold(This::Item::one(), do(acc, x) acc * x)
    }

    // === NEW: Collection methods ===

    /// Collects elements into a collection.
    pub func collect[C: FromIterator[This::Item]](this) -> C {
        return C::from_iter(this)
    }

    /// Calls a function on each element.
    pub func for_each(this, f: func(This::Item)) {
        loop {
            when this.next() {
                Just(x) => f(x),
                Nothing => break
            }
        }
    }

    // === NEW: Comparison methods ===

    /// Lexicographically compares with another iterator.
    pub func cmp[I: Iterator](this, other: I) -> Ordering
    where This::Item: Ord, I::Item = This::Item {
        loop {
            when (this.next(), other.next()) {
                (Just(a), Just(b)) => {
                    when a.cmp(ref b) {
                        Equal => {},
                        ord => return ord
                    }
                },
                (Just(_), Nothing) => return Greater,
                (Nothing, Just(_)) => return Less,
                (Nothing, Nothing) => return Equal
            }
        }
    }

    /// Tests equality with another iterator.
    pub func eq[I: Iterator](this, other: I) -> Bool
    where This::Item: PartialEq, I::Item = This::Item {
        loop {
            when (this.next(), other.next()) {
                (Just(a), Just(b)) => {
                    if a.ne(ref b) {
                        return false
                    }
                },
                (Nothing, Nothing) => return true,
                _ => return false
            }
        }
    }
}
```

---

## 2. Operator Behaviors

### 2.1 Deref/DerefMut (ops.tml)

```tml
/// Dereference operator (*v).
/// Enables custom pointer-like types.
pub behavior Deref {
    /// The resulting type after dereferencing.
    type Target

    /// Dereferences the value.
    pub func deref(this) -> ref This::Target
}

/// Mutable dereference operator (*v = x).
pub behavior DerefMut: Deref {
    /// Mutably dereferences the value.
    pub func deref_mut(mut this) -> mut ref This::Target
}
```

**Compiler Support Required**:
- `*ptr` desugars to `ptr.deref()` for immutable
- `*ptr = value` desugars to `*ptr.deref_mut() = value`
- Deref coercion: `&T` where `T: Deref[Target=U]` coerces to `&U`

### 2.2 Range Types (ops/range.tml)

```tml
/// Bound of a range endpoint.
pub type Bound[T] {
    /// Included in the range.
    Included(T),
    /// Excluded from the range.
    Excluded(T),
    /// No bound (infinite).
    Unbounded
}

/// A range `start..end` (exclusive end).
pub type Range[Idx] {
    start: Idx,
    end: Idx
}

/// A range `start..` (unbounded end).
pub type RangeFrom[Idx] {
    start: Idx
}

/// A range `..end` (unbounded start).
pub type RangeTo[Idx] {
    end: Idx
}

/// A range `start..=end` (inclusive end).
/// TML syntax: `start through end`
pub type RangeInclusive[Idx] {
    start: Idx,
    end: Idx,
    exhausted: Bool
}

/// The full range `..` (unbounded).
pub type RangeFull {}

/// Behavior for range bounds.
pub behavior RangeBounds[T] {
    /// Start bound.
    pub func start_bound(this) -> Bound[ref T]

    /// End bound.
    pub func end_bound(this) -> Bound[ref T]

    /// Tests if item is within bounds.
    pub func contains(this, item: ref T) -> Bool where T: PartialOrd {
        let start_ok: Bool = when this.start_bound() {
            Included(s) => item >= s,
            Excluded(s) => item > s,
            Unbounded => true
        }
        let end_ok: Bool = when this.end_bound() {
            Included(e) => item <= e,
            Excluded(e) => item < e,
            Unbounded => true
        }
        return start_ok and end_ok
    }
}

// Iterator implementation for Range[I64]
impl Iterator for Range[I64] {
    type Item = I64

    pub func next(mut this) -> Maybe[I64] {
        if this.start >= this.end {
            return Nothing
        }
        let val: I64 = this.start
        this.start = this.start + 1
        return Just(val)
    }
}

impl DoubleEndedIterator for Range[I64] {
    pub func next_back(mut this) -> Maybe[I64] {
        if this.start >= this.end {
            return Nothing
        }
        this.end = this.end - 1
        return Just(this.end)
    }
}

impl ExactSizeIterator for Range[I64] {
    pub func len(this) -> I64 {
        if this.end <= this.start {
            return 0
        }
        return this.end - this.start
    }
}
```

---

## 3. Marker Types

### 3.1 PhantomData (marker.tml)

```tml
/// Zero-sized type used to mark type parameters.
///
/// # Use Cases
///
/// 1. **Variance marking**: Tell compiler how T relates to lifetime/ownership
/// 2. **Drop checking**: Act as if owning T for drop order
/// 3. **Auto-trait propagation**: Inherit Send/Sync based on T
///
/// # Examples
///
/// ```tml
/// // Raw pointer wrapper that "owns" T
/// pub type Unique[T] {
///     ptr: RawPtr[T],
///     _marker: PhantomData[T]
/// }
///
/// // Type that doesn't contain T but should be Send only if T is
/// pub type Handle[T] {
///     id: I64,
///     _marker: PhantomData[T]
/// }
/// ```
pub type PhantomData[T] {}

impl[T] PhantomData[T] {
    /// Creates a new PhantomData.
    pub func new() -> PhantomData[T] {
        return PhantomData {}
    }
}

impl[T] Default for PhantomData[T] {
    pub func default() -> PhantomData[T] {
        return PhantomData {}
    }
}

impl[T] Duplicate for PhantomData[T] {
    pub func duplicate(this) -> PhantomData[T] {
        return PhantomData {}
    }
}

impl[T] Copy for PhantomData[T] {}

impl[T] PartialEq for PhantomData[T] {
    pub func eq(this, other: ref PhantomData[T]) -> Bool {
        return true  // All PhantomData are equal
    }
}

impl[T] Eq for PhantomData[T] {}

impl[T] PartialOrd for PhantomData[T] {
    pub func partial_cmp(this, other: ref PhantomData[T]) -> Maybe[Ordering] {
        return Just(Equal)
    }
}

impl[T] Ord for PhantomData[T] {
    pub func cmp(this, other: ref PhantomData[T]) -> Ordering {
        return Equal
    }
}

impl[T] Hash for PhantomData[T] {
    pub func hash(this) -> I64 {
        return 0
    }
}

impl[T] Debug for PhantomData[T] {
    pub func debug_string(this) -> Str {
        return "PhantomData"
    }
}

// Auto-trait propagation
impl[T: Send] Send for PhantomData[T] {}
impl[T: Sync] Sync for PhantomData[T] {}
impl[T] Sized for PhantomData[T] {}
impl[T] Unpin for PhantomData[T] {}
```

### 3.2 PhantomPinned (marker.tml)

```tml
/// Marker type that is NOT Unpin.
///
/// Adding this to a type prevents it from being unpinned,
/// which is necessary for self-referential types.
///
/// # Example
///
/// ```tml
/// pub type SelfReferential {
///     data: Str,
///     ptr: RawPtr[Str],  // Points to data
///     _pin: PhantomPinned
/// }
/// ```
pub type PhantomPinned {}

impl Default for PhantomPinned {
    pub func default() -> PhantomPinned {
        return PhantomPinned {}
    }
}

impl Duplicate for PhantomPinned {
    pub func duplicate(this) -> PhantomPinned {
        return PhantomPinned {}
    }
}

impl Copy for PhantomPinned {}

impl Debug for PhantomPinned {
    pub func debug_string(this) -> Str {
        return "PhantomPinned"
    }
}

// NOTE: PhantomPinned does NOT implement Unpin
// This is the whole point of its existence
```

---

## 4. Pin Module

### 4.1 Pin Type (pin.tml)

```tml
//! Pinned pointers.
//!
//! A `Pin[P]` wraps a pointer P and guarantees the pointed-to
//! value will not be moved until dropped.

/// A pinned pointer.
///
/// `Pin[P]` ensures the value behind P cannot be moved. This is
/// essential for self-referential types where internal pointers
/// would be invalidated by moves.
///
/// # Invariants
///
/// If `T: !Unpin`, the value cannot be moved after pinning.
/// If `T: Unpin`, the value can still be moved freely.
pub type Pin[P] {
    pointer: P
}

impl[P: Deref] Pin[P] {
    /// Creates a new Pin from a pointer.
    ///
    /// # Safety (lowlevel)
    ///
    /// Caller must guarantee the pointed value won't be moved.
    pub func new_unchecked(pointer: P) -> Pin[P] {
        return Pin { pointer: pointer }
    }

    /// Gets a pinned reference to the value.
    pub func as_ref(this) -> Pin[ref P::Target] {
        return Pin { pointer: this.pointer.deref() }
    }

    /// Returns a reference to the value inside.
    pub func get_ref(this) -> ref P::Target {
        return this.pointer.deref()
    }
}

impl[P: DerefMut] Pin[P] {
    /// Gets a pinned mutable reference.
    pub func as_mut(mut this) -> Pin[mut ref P::Target] {
        return Pin { pointer: this.pointer.deref_mut() }
    }
}

// Safe Pin creation for Unpin types
impl[T: Unpin] Pin[ref T] {
    /// Creates a Pin from a reference.
    /// Safe because T: Unpin means it can be moved anyway.
    pub func new(pointer: ref T) -> Pin[ref T] {
        return Pin { pointer: pointer }
    }
}

impl[T: Unpin] Pin[mut ref T] {
    /// Creates a Pin from a mutable reference.
    /// Safe because T: Unpin means it can be moved anyway.
    pub func new(pointer: mut ref T) -> Pin[mut ref T] {
        return Pin { pointer: pointer }
    }

    /// Gets a mutable reference to the value.
    /// Safe because T: Unpin.
    pub func get_mut(mut this) -> mut ref T {
        return this.pointer
    }
}

impl[P: Deref] Pin[P] where P::Target: Unpin {
    /// Unwraps the Pin, returning the pointer.
    /// Safe because the target is Unpin.
    pub func into_inner(this) -> P {
        return this.pointer
    }
}

// Deref implementation
impl[P: Deref] Deref for Pin[P] {
    type Target = P::Target

    pub func deref(this) -> ref P::Target {
        return this.pointer.deref()
    }
}

impl[P: DerefMut] DerefMut for Pin[P] where P::Target: Unpin {
    pub func deref_mut(mut this) -> mut ref P::Target {
        return this.pointer.deref_mut()
    }
}
```

---

## 5. Future/Task Types

### 5.1 Poll (future/poll.tml)

```tml
/// The result of an async operation poll.
pub type Poll[T] {
    /// Operation completed with value.
    Ready(T),
    /// Operation not yet complete.
    Pending
}

impl[T] Poll[T] {
    /// Returns true if Ready.
    pub func is_ready(this) -> Bool {
        when this {
            Ready(_) => return true,
            Pending => return false
        }
    }

    /// Returns true if Pending.
    pub func is_pending(this) -> Bool {
        return not this.is_ready()
    }

    /// Maps Ready value.
    pub func map[U](this, f: func(T) -> U) -> Poll[U] {
        when this {
            Ready(t) => return Ready(f(t)),
            Pending => return Pending
        }
    }
}

impl[T, E] Poll[Outcome[T, E]] {
    /// Maps the Ok value.
    pub func map_ok[U](this, f: func(T) -> U) -> Poll[Outcome[U, E]] {
        when this {
            Ready(Ok(t)) => return Ready(Ok(f(t))),
            Ready(Err(e)) => return Ready(Err(e)),
            Pending => return Pending
        }
    }

    /// Maps the Err value.
    pub func map_err[F](this, f: func(E) -> F) -> Poll[Outcome[T, F]] {
        when this {
            Ready(Ok(t)) => return Ready(Ok(t)),
            Ready(Err(e)) => return Ready(Err(f(e))),
            Pending => return Pending
        }
    }
}
```

### 5.2 Future (future/mod.tml)

```tml
//! Async computation primitives.

pub mod poll
pub use poll::{Poll, Ready, Pending}

use core::pin::Pin
use core::task::Context

/// An async computation that produces a value.
///
/// Futures are lazy - they do nothing until polled.
pub behavior Future {
    /// The type produced when complete.
    type Output

    /// Attempts to complete the future.
    ///
    /// Returns:
    /// - `Ready(value)` if complete
    /// - `Pending` if not ready (task will be woken later)
    pub func poll(self: Pin[mut ref Self], cx: mut ref Context) -> Poll[This::Output]
}

/// Conversion into a Future.
pub behavior IntoFuture {
    type Output
    type IntoFuture: Future where This::IntoFuture::Output = This::Output

    pub func into_future(this) -> This::IntoFuture
}

// Every Future is IntoFuture
impl[F: Future] IntoFuture for F {
    type Output = F::Output
    type IntoFuture = F

    pub func into_future(this) -> F {
        return this
    }
}
```

### 5.3 Task Context (task/context.tml)

```tml
use core::task::Waker

/// Context for an async task.
///
/// Provides access to the waker for the current task.
pub type Context {
    waker: ref Waker
}

impl Context {
    /// Creates a Context from a waker reference.
    pub func from_waker(waker: ref Waker) -> Context {
        return Context { waker: waker }
    }

    /// Returns the waker for this context.
    pub func waker(this) -> ref Waker {
        return this.waker
    }
}
```

### 5.4 Waker (task/wake.tml)

```tml
/// Handle to wake a task.
pub type Waker {
    raw: RawWaker
}

/// Low-level waker data.
pub type RawWaker {
    data: RawPtr[Unit],
    vtable: ref RawWakerVTable
}

/// Virtual function table for wakers.
pub type RawWakerVTable {
    clone: func(RawPtr[Unit]) -> RawWaker,
    wake: func(RawPtr[Unit]),
    wake_by_ref: func(RawPtr[Unit]),
    drop: func(RawPtr[Unit])
}

impl Waker {
    /// Wakes the task (consumes self).
    pub func wake(this) {
        let f = this.raw.vtable.wake
        f(this.raw.data)
    }

    /// Wakes the task without consuming self.
    pub func wake_by_ref(this) {
        let f = this.raw.vtable.wake_by_ref
        f(this.raw.data)
    }
}

impl Duplicate for Waker {
    pub func duplicate(this) -> Waker {
        let f = this.raw.vtable.clone
        return Waker { raw: f(this.raw.data) }
    }
}

impl Drop for Waker {
    pub func drop(mut this) {
        let f = this.raw.vtable.drop
        f(this.raw.data)
    }
}
```

---

## 6. Numeric Traits

### 6.1 Core Behaviors (num/mod.tml)

```tml
/// A type with a zero value.
pub behavior Zero {
    /// Returns the zero value.
    pub func zero() -> Self

    /// Tests if this is zero.
    pub func is_zero(this) -> Bool
}

/// A type with a one/identity value.
pub behavior One {
    /// Returns the multiplicative identity.
    pub func one() -> Self
}

/// Saturating arithmetic (clamps at MIN/MAX).
pub behavior Saturating {
    pub func saturating_add(this, rhs: Self) -> Self
    pub func saturating_sub(this, rhs: Self) -> Self
    pub func saturating_mul(this, rhs: Self) -> Self
}

/// Wrapping arithmetic (wraps at overflow).
pub behavior Wrapping {
    pub func wrapping_add(this, rhs: Self) -> Self
    pub func wrapping_sub(this, rhs: Self) -> Self
    pub func wrapping_mul(this, rhs: Self) -> Self
}

/// Checked arithmetic (returns None on overflow).
pub behavior Checked {
    pub func checked_add(this, rhs: Self) -> Maybe[Self]
    pub func checked_sub(this, rhs: Self) -> Maybe[Self]
    pub func checked_mul(this, rhs: Self) -> Maybe[Self]
    pub func checked_div(this, rhs: Self) -> Maybe[Self]
}

/// Overflowing arithmetic (returns result + overflow flag).
pub behavior Overflowing {
    pub func overflowing_add(this, rhs: Self) -> (Self, Bool)
    pub func overflowing_sub(this, rhs: Self) -> (Self, Bool)
    pub func overflowing_mul(this, rhs: Self) -> (Self, Bool)
}

// Implementation for I64
impl Zero for I64 {
    pub func zero() -> I64 { return 0 }
    pub func is_zero(this) -> Bool { return this == 0 }
}

impl One for I64 {
    pub func one() -> I64 { return 1 }
}

impl Checked for I64 {
    pub func checked_add(this, rhs: I64) -> Maybe[I64] {
        return lowlevel { i64_checked_add(this, rhs) }
    }
    pub func checked_sub(this, rhs: I64) -> Maybe[I64] {
        return lowlevel { i64_checked_sub(this, rhs) }
    }
    pub func checked_mul(this, rhs: I64) -> Maybe[I64] {
        return lowlevel { i64_checked_mul(this, rhs) }
    }
    pub func checked_div(this, rhs: I64) -> Maybe[I64] {
        if rhs == 0 {
            return Nothing
        }
        return Just(this / rhs)
    }
}

// ... similar for other integer types
```

---

## 7. Memory Layout

### 7.1 Layout Type (alloc/layout.tml)

```tml
/// Describes memory layout: size and alignment.
pub type Layout {
    size: I64,
    align: I64
}

/// Error when creating invalid layout.
pub type LayoutError {}

impl Layout {
    /// Creates a Layout, validating alignment.
    pub func from_size_align(size: I64, align: I64) -> Outcome[Layout, LayoutError] {
        // Align must be power of two
        if align <= 0 or (align & (align - 1)) != 0 {
            return Err(LayoutError {})
        }
        // Size must not overflow when rounded up
        if size < 0 {
            return Err(LayoutError {})
        }
        return Ok(Layout { size: size, align: align })
    }

    /// Creates a Layout without validation.
    pub func from_size_align_unchecked(size: I64, align: I64) -> Layout {
        return Layout { size: size, align: align }
    }

    /// Gets layout of type T.
    pub func new[T]() -> Layout {
        return Layout {
            size: size_of[T](),
            align: align_of[T]()
        }
    }

    /// Gets layout of a value.
    pub func for_value[T](t: ref T) -> Layout {
        return Layout::new[T]()
    }

    /// Returns size in bytes.
    pub func size(this) -> I64 {
        return this.size
    }

    /// Returns alignment in bytes.
    pub func align(this) -> I64 {
        return this.align
    }

    /// Creates layout for array of n elements.
    pub func repeat(this, n: I64) -> Outcome[(Layout, I64), LayoutError] {
        let padded = this.pad_to_align()
        let alloc_size = padded.size * n
        if alloc_size < 0 {
            return Err(LayoutError {})
        }
        return Ok((Layout { size: alloc_size, align: this.align }, padded.size))
    }

    /// Pads size up to alignment.
    pub func pad_to_align(this) -> Layout {
        let mask = this.align - 1
        let padded = (this.size + mask) & !mask
        return Layout { size: padded, align: this.align }
    }

    /// Layout for array of T with n elements.
    pub func array[T](n: I64) -> Outcome[Layout, LayoutError] {
        let elem = Layout::new[T]()
        let (layout, _) = elem.repeat(n)?
        return Ok(layout)
    }
}

impl Display for LayoutError {
    pub func to_string(this) -> Str {
        return "invalid layout parameters"
    }
}

impl Debug for LayoutError {
    pub func debug_string(this) -> Str {
        return "LayoutError"
    }
}

impl Error for LayoutError {}
```

---

## Testing Requirements

Each module must have comprehensive tests:

1. **Iterator tests**: All consumer methods with various iterator types
2. **Operator tests**: Deref coercion, range iteration, bitwise assign
3. **Marker tests**: PhantomData with different type params
4. **Pin tests**: Safe/unsafe creation, Unpin behavior
5. **Future tests**: Poll mapping, basic future implementation
6. **Numeric tests**: Overflow behavior, edge cases
7. **Layout tests**: Alignment validation, array layouts

Test coverage target: ≥95% for all new code.
