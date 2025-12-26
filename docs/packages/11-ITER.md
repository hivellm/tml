# TML Standard Library: Iterators

> `std::iter` — Iterator types and basic combinators for lazy iteration.

## Overview

The iterator module provides the fundamental `Iterator` behavior and basic range types for lazy, composable iteration over sequences. The current implementation focuses on core functionality with range-based iteration and essential combinators.

## Import

```tml
use std::iter
```

---

## Current Implementation Status (v0.1)

### ✅ Implemented
- `Iterator` behavior with `next()` method
- `IntoIterator` behavior for type conversion
- `Range` type for integer iteration
- Basic combinators: `take()`, `skip()`, `sum()`, `count()`
- Range constructors: `range()`, `range_inclusive()`, `range_step()`

### 🚧 Planned (Future Releases)
- Advanced combinators: `map()`, `filter()`, `flat_map()`, `enumerate()`, `zip()`
- Closure-based combinators: `fold()`, `any()`, `all()`
- Collection conversion: `collect()`, `to_vec()`
- Adapter types: `Map`, `Filter`, `Chain`, `Peekable`
- Iterator constructors: `empty()`, `once()`, `repeat()`, `from_fn()`

**Note**: Some combinators (`fold`, `any`, `all`) are implemented but temporarily disabled due to compiler limitations with function pointer type inference. They will be enabled when the compiler's closure support is complete.

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

### Phase 1 (Current) - Basic Iteration
- ✅ Iterator and IntoIterator behaviors
- ✅ Range type with I32
- ✅ Basic combinators (take, skip, sum, count)

### Phase 2 - Core Combinators
- ⏳ Transforming: `map()`, `flat_map()`, `flatten()`, `inspect()`
- ⏳ Filtering: `filter()`, `filter_map()`, `take_while()`, `skip_while()`
- ⏳ Combining: `chain()`, `zip()`, `enumerate()`

### Phase 3 - Advanced Features
- ⏳ Consuming: `fold()`, `reduce()`, `collect()`, `find()`, `position()`
- ⏳ Aggregating: `min()`, `max()`, `sum()` (generic), `product()`
- ⏳ Testing: `any()`, `all()`, `eq()`, `cmp()`

### Phase 4 - Adapter Types
- ⏳ `Map`, `Filter`, `Take`, `Skip`, `Chain`, `Zip`
- ⏳ `Peekable`, `Enumerate`, `Cycle`, `Rev`
- ⏳ Double-ended iterators

### Phase 5 - Constructors
- ⏳ `empty()`, `once()`, `repeat()`, `repeat_n()`
- ⏳ `from_fn()`, `successors()`
- ⏳ Generic range types: `Range[T: Numeric]`

---

## See Also

- [10-COLLECTIONS.md](./10-COLLECTIONS.md) — Collection types
- [../specs/03-GRAMMAR.md](../specs/03-GRAMMAR.md) — Loop syntax
- [../specs/05-SEMANTICS.md](../specs/05-SEMANTICS.md) — Behavior semantics
- [CHANGELOG.md](../../CHANGELOG.md) — Recent iterator updates
