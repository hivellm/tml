# TML Standard Library: Iterators

> `core::iter` — Iterator types and combinators for lazy iteration.

## Overview

The iterator module provides the fundamental `Iterator` behavior, source iterators, and adapter types for lazy, composable iteration over sequences. The implementation is split between `core::iter` (fundamental types) and `std::iter` (higher-level utilities).

## Import

```tml
use core::iter::*
```

---

## Current Implementation Status (v0.3)

### ✅ Implemented (core::iter)
- `Iterator` behavior with `next()` method and associated `Item` type
- `IntoIterator` and `FromIterator` behaviors
- `DoubleEndedIterator`, `ExactSizeIterator`, `FusedIterator` behaviors
- Source iterators: `EmptyI32`, `EmptyI64`, `OnceI32`, `OnceI64`, `RepeatNI32`, `RepeatNI64`
- Factory functions: `empty_i32()`, `once_i32()`, `repeat_n_i32()`, etc.
- Adapter types: `Take`, `Skip`, `Chain`, `Enumerate`, `Zip`, `StepBy`, `Fuse`
- All adapters implement `Iterator` with proper type associations

### ✅ Implemented (std::iter)
- `Range` type for integer iteration
- Basic combinators: `take()`, `skip()`, `sum()`, `count()`
- Range constructors: `range()`, `range_inclusive()`, `range_step()`

### 🚧 Planned (Future Releases)
- Advanced combinators: `map()`, `filter()`, `flat_map()`
- Closure-based combinators: `fold()`, `any()`, `all()`
- Collection conversion: `collect()`, `to_vec()`
- Generic iterator constructors

**Note**: Some combinators (`fold`, `any`, `all`) are implemented but temporarily disabled due to compiler limitations with function pointer type inference. They will be enabled when the compiler's closure support is complete.

---

## Source Iterators (core::iter)

### EmptyI32 / EmptyI64

An iterator that yields nothing.

```tml
use core::iter::*

func empty_example() {
    var e: EmptyI32 = empty_i32()
    when e.next() {
        Just(_) => println("unexpected"),
        Nothing => println("empty as expected")
    }
}
```

### OnceI32 / OnceI64

An iterator that yields exactly one element.

```tml
use core::iter::*

func once_example() {
    var o: OnceI32 = once_i32(42)
    when o.next() {
        Just(n) => println(n),  // Prints: 42
        Nothing => {}
    }
    when o.next() {
        Just(_) => {},
        Nothing => println("exhausted")  // Prints: exhausted
    }
}
```

### RepeatNI32 / RepeatNI64

An iterator that repeats a value a fixed number of times.

```tml
use core::iter::*

func repeat_example() {
    var r: RepeatNI32 = repeat_n_i32(7, 3)  // Repeat 7 three times
    loop {
        when r.next() {
            Just(n) => println(n),  // Prints: 7, 7, 7
            Nothing => break
        }
    }
}
```

---

## Iterator Adapters (core::iter)

### Take

Yields at most `n` elements from the underlying iterator.

```tml
pub type Take[I] { iter: I, remaining: I64 }

pub func take[I: Iterator](iter: I, n: I64) -> Take[I]
```

### Skip

Skips the first `n` elements from the underlying iterator.

```tml
pub type Skip[I] { iter: I, remaining: I64 }

pub func skip[I: Iterator](iter: I, n: I64) -> Skip[I]
```

### Chain

Chains two iterators together, yielding all elements from the first, then all from the second.

```tml
pub type Chain[A, B] { first: Maybe[A], second: B }

pub func chain[A: Iterator, B: Iterator](first: A, second: B) -> Chain[A, B]
    where A::Item = B::Item
```

### Enumerate

Yields pairs of (index, element) where index starts at 0.

```tml
pub type Enumerate[I] { iter: I, index: I64 }

pub func enumerate[I: Iterator](iter: I) -> Enumerate[I]
```

### Zip

Zips two iterators together, yielding pairs until either is exhausted.

```tml
pub type Zip[A, B] { first: A, second: B }

pub func zip[A: Iterator, B: Iterator](first: A, second: B) -> Zip[A, B]
```

### StepBy

Yields every `step`-th element from the underlying iterator.

```tml
pub type StepBy[I] { iter: I, step: I64, first_take: Bool }

pub func step_by[I: Iterator](iter: I, step: I64) -> StepBy[I]
```

### Fuse

Ensures the iterator returns `Nothing` forever after the first `Nothing`.

```tml
pub type Fuse[I] { iter: Maybe[I] }

pub func fuse[I: Iterator](iter: I) -> Fuse[I]
```

---

## Core Behaviors

### Iterator

The fundamental behavior for all iterators.

```tml
/// The core iteration behavior. Types that implement Iterator can be used in for-in loops.
pub behavior Iterator {
    /// The type of elements being iterated
    type Item

    /// Advances the iterator and returns the next value.
    /// Returns Nothing when iteration is complete.
    func next(mut this) -> Maybe[This::Item]
}
```

### IntoIterator

Behavior for types that can be converted into an iterator.

```tml
/// Types that can be converted into an Iterator.
pub behavior IntoIterator {
    /// The type of iterator this converts into
    type Iter

    /// Converts this value into an iterator
    func into_iter(this) -> This::Iter
}
```

---

## Range Type

### Range (Exclusive End)

An iterator over a range of integers from `start` to `end` (exclusive).

```tml
/// An iterator over a range of integers
pub type Range {
    current: I32,
    end: I32,
    step: I32,
    inclusive: Bool
}

impl Iterator for Range {
    type Item = I32

    pub func next(mut this) -> Maybe[I32] {
        if this.inclusive {
            if this.current <= this.end {
                let value: I32 = this.current
                this.current = this.current + this.step
                return Just(value)
            } else {
                return Nothing
            }
        } else {
            if this.current < this.end {
                let value: I32 = this.current
                this.current = this.current + this.step
                return Just(value)
            } else {
                return Nothing
            }
        }
    }
}
```

### Range Constructors

```tml
/// Creates a range from start to end (exclusive)
pub func range(start: I32, end: I32) -> Range {
    return Range { current: start, end: end, step: 1, inclusive: false }
}

/// Creates a range from start through end (inclusive)
pub func range_inclusive(start: I32, end: I32) -> Range {
    return Range { current: start, end: end, step: 1, inclusive: true }
}

/// Creates a range with custom step
pub func range_step(start: I32, end: I32, step: I32) -> Range {
    return Range { current: start, end: end, step: step, inclusive: false }
}
```

---

## Iterator Combinators

### Basic Combinators (Implemented)

```tml
impl Range {
    /// Takes the first n elements from the iterator.
    /// Returns a new Range limited to n elements.
    pub func take(this, n: I32) -> Range {
        let actual_end: I32 = this.current + (n * this.step)
        let capped_end: I32 = if actual_end < this.end then actual_end else this.end
        return Range {
            current: this.current,
            end: capped_end,
            step: this.step,
            inclusive: false
        }
    }

    /// Skips the first n elements from the iterator.
    /// Returns a new Range starting n elements ahead.
    pub func skip(this, n: I32) -> Range {
        let new_current: I32 = this.current + (n * this.step)
        return Range {
            current: new_current,
            end: this.end,
            step: this.step,
            inclusive: this.inclusive
        }
    }

    /// Sums all elements in the iterator.
    pub func sum(mut this) -> I32 {
        let mut total: I32 = 0
        loop {
            when this.next() {
                Just(value) => total = total + value,
                Nothing => break
            }
        }
        return total
    }

    /// Counts the number of elements in the iterator.
    pub func count(mut this) -> I32 {
        let mut n: I32 = 0
        loop {
            when this.next() {
                Just(_) => n = n + 1,
                Nothing => break
            }
        }
        return n
    }
}
```

### Disabled Combinators (Pending Compiler Fixes)

The following combinators are implemented but disabled due to compiler limitations:

```tml
// Note: fold, any, and all are temporarily disabled due to closure type inference bugs
// They will be re-enabled when the codegen properly handles function pointer types
```

These include:
- `fold(init, f)` - Requires function pointer type inference
- `any(predicate)` - Requires function pointer type inference
- `all(predicate)` - Requires function pointer type inference

---

## Examples

### Basic Range Iteration

```tml
use std::iter

func basic_iteration() {
    // Simple range
    let mut r: Range = range(0, 5)
    loop {
        when r.next() {
            Just(value) => println(value),
            Nothing => break
        }
    }
    // Output: 0, 1, 2, 3, 4
}
```

### Using Combinators

```tml
use std::iter

func combinator_examples() {
    // Sum of first 10 numbers
    let mut r1: Range = range(0, 10)
    let sum: I32 = r1.sum()
    // sum = 45

    // Count elements
    let mut r2: Range = range(0, 100)
    let count: I32 = r2.count()
    // count = 100

    // Take first 5 elements
    let r3: Range = range(0, 100)
    let mut taken: Range = r3.take(5)
    let sum_taken: I32 = taken.sum()
    // sum_taken = 10

    // Skip first 5, sum rest
    let r4: Range = range(0, 10)
    let mut skipped: Range = r4.skip(5)
    let sum_skipped: I32 = skipped.sum()
    // sum_skipped = 35
}
```

### Inclusive Ranges

```tml
use std::iter

func inclusive_example() {
    // Range including the end value
    let mut r: Range = range_inclusive(1, 5)
    let sum: I32 = r.sum()
    // sum = 15 (1+2+3+4+5)
}
```

### Custom Step

```tml
use std::iter

func step_example() {
    // Every second number
    let mut r: Range = range_step(0, 10, 2)
    loop {
        when r.next() {
            Just(value) => println(value),
            Nothing => break
        }
    }
    // Output: 0, 2, 4, 6, 8
}
```

### Chaining Combinators

```tml
use std::iter

func chaining_example() {
    // Skip first 10, take next 5, sum them
    let r: Range = range(0, 100)
    let mut iter: Range = r.skip(10).take(5)
    let result: I32 = iter.sum()
    // result = 60 (10+11+12+13+14)
}
```

---

## Implementation Notes

### Zero-Cost Abstractions

The current implementation compiles iterator operations to efficient loops with minimal overhead:

```tml
// This:
let r: Range = range(0, 100)
let mut taken: Range = r.take(10)
let sum: I32 = taken.sum()

// Compiles to roughly:
var total: I32 = 0
var i: I32 = 0
loop {
    if i >= 10 then break
    total = total + i
    i = i + 1
}
```

### Lazy Evaluation

Range combinators like `take()` and `skip()` are lazy - they create new Range values without consuming elements:

```tml
let r: Range = range(0, 1000000)
let limited: Range = r.take(10)  // No iteration happens here
let sum: I32 = limited.sum()      // Only 10 elements are processed
```

### Module Method Resolution

The iterator combinators work correctly thanks to the module method lookup system, which resolves method calls like `Range::next()` to the fully qualified `std::iter::Range::next()`.

---

## Known Limitations

### Compiler Bugs (Being Fixed)

1. **Generic Enum Redefinition**: The `Maybe[I32]` type is sometimes emitted multiple times in LLVM IR, causing compilation errors. This is a codegen bug that doesn't affect the iterator design.

2. **Function Pointer Types**: Closures and function pointers don't have complete type inference, preventing `fold()`, `any()`, and `all()` from compiling. The implementation is correct, just waiting for compiler support.

### Language Limitations

1. **Generic Iterators**: The current `Range` type only supports `I32`. Generic ranges will be added when the compiler's generic type system is more mature.

2. **Behavior Extensions**: The full `extend Iterator` syntax for adding methods to the Iterator behavior is planned but not yet implemented.

3. **For-in Loops**: Direct `for item in range(0, 10)` syntax requires additional compiler support. Use explicit `loop` with `next()` for now.

---

## Future Roadmap

### Phase 1 (Complete) - Basic Iteration
- ✅ Iterator, IntoIterator, FromIterator behaviors
- ✅ DoubleEndedIterator, ExactSizeIterator, FusedIterator behaviors
- ✅ Range type with I32 (std::iter)
- ✅ Basic combinators (take, skip, sum, count)

### Phase 2 (Complete) - Source Iterators
- ✅ `empty_i32()`, `empty_i64()` - Empty iterators
- ✅ `once_i32()`, `once_i64()` - Single-element iterators
- ✅ `repeat_n_i32()`, `repeat_n_i64()` - Fixed-count repetition

### Phase 3 (Complete) - Core Adapters
- ✅ `Take`, `Skip` - Limiting adapters
- ✅ `Chain` - Concatenation adapter
- ✅ `Enumerate` - Index pairing adapter
- ✅ `Zip` - Pair combining adapter
- ✅ `StepBy` - Stepping adapter
- ✅ `Fuse` - Safety adapter

### Phase 4 - Transforming Adapters (Planned)
- ⏳ `Map`, `Filter`, `FilterMap` - Requires closure support
- ⏳ `FlatMap`, `Flatten` - Nested iteration
- ⏳ `Inspect` - Side-effect adapter

### Phase 5 - Advanced Features (Planned)
- ⏳ Consuming: `fold()`, `reduce()`, `collect()`, `find()`, `position()`
- ⏳ Aggregating: `min()`, `max()`, `sum()` (generic), `product()`
- ⏳ Testing: `any()`, `all()`, `eq()`, `cmp()`
- ⏳ Generic iterator constructors

---

## See Also

- [10-COLLECTIONS.md](./10-COLLECTIONS.md) — Collection types
- [../specs/03-GRAMMAR.md](../specs/03-GRAMMAR.md) — Loop syntax
- [../specs/05-SEMANTICS.md](../specs/05-SEMANTICS.md) — Behavior semantics
- [CHANGELOG.md](../../CHANGELOG.md) — Recent iterator updates
