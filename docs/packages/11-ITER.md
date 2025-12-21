# TML Standard Library: Iterators

> `std.iter` — Composable, lazy iteration over sequences.

## Overview

The iterator package provides traits and adapters for lazy, composable iteration over collections and sequences. Iterators are zero-cost abstractions that compile to efficient loops.

## Import

```tml
import std.iter
import std.iter.{Iterator, IntoIterator, repeat, once}
```

---

## Core Traits

### Iterator

The fundamental trait for all iterators.

```tml
/// An iterator over a sequence of values
public behaviorIterator {
    /// The type of elements being iterated
    type Item

    /// Advances the iterator and returns the next value
    func next(mut this) -> Maybe[This.Item]

    /// Returns a size hint (lower, upper)
    func size_hint(this) -> (U64, Maybe[U64]) {
        return (0, None)
    }

    /// Returns the exact length if known
    func count(mut this) -> U64 {
        var n: U64 = 0
        loop this.next().is_some() {
            n = n + 1
        }
        return n
    }
}
```

### IntoIterator

Trait for types that can be converted into an iterator.

```tml
/// Conversion into an iterator
public behaviorIntoIterator {
    /// The type of elements being iterated
    type Item

    /// The iterator type
    type Iter: Iterator[Item = This.Item]

    /// Creates an iterator from this value
    func into_iter(this) -> This.Iter
}

/// Every Iterator is IntoIterator for itself
implement IntoIterator for I where I: Iterator {
    type Item = I.Item
    type Iter = I

    func into_iter(this) -> I {
        return this
    }
}
```

### ExactSizeIterator

Iterator with known exact size.

```tml
/// Iterator with exact size known
public behaviorExactSizeIterator: Iterator {
    /// Returns the exact number of remaining elements
    func len(this) -> U64

    /// Returns true if no elements remain
    func is_empty(this) -> Bool {
        return this.len() == 0
    }
}
```

### DoubleEndedIterator

Iterator that can iterate from both ends.

```tml
/// Iterator that can be reversed
public behaviorDoubleEndedIterator: Iterator {
    /// Advances the iterator from the back
    func next_back(mut this) -> Maybe[This.Item]
}
```

### FusedIterator

Iterator that always returns None after returning None once.

```tml
/// Iterator that is fused (None stays None)
public behaviorFusedIterator: Iterator {}
```

---

## Iterator Adapters

All adapters are lazy and don't consume elements until the final iterator is consumed.

### Transforming Elements

```tml
extend Iterator {
    /// Transforms each element using a function
    public func map[B](this, f: func(This.Item) -> B) -> Map[This, B] {
        return Map { iter: this, f: f }
    }

    /// Transforms each element to an iterator and flattens
    public func flat_map[B, I](this, f: func(This.Item) -> I) -> FlatMap[This, I]
        where I: IntoIterator[Item = B]
    {
        return FlatMap { iter: this, f: f, current: None }
    }

    /// Flattens nested iterators
    public func flatten(this) -> Flatten[This]
        where This.Item: IntoIterator
    {
        return Flatten { iter: this, current: None }
    }

    /// Calls a function on each element, passing through the element
    public func inspect(this, f: func(ref This.Item)) -> Inspect[This] {
        return Inspect { iter: this, f: f }
    }
}
```

### Filtering Elements

```tml
extend Iterator {
    /// Filters elements based on a predicate
    public func filter(this, predicate: func(ref This.Item) -> Bool) -> Filter[This] {
        return Filter { iter: this, predicate: predicate }
    }

    /// Filters and maps in one step
    public func filter_map[B](this, f: func(This.Item) -> Maybe[B]) -> FilterMap[This, B] {
        return FilterMap { iter: this, f: f }
    }

    /// Takes elements while predicate is true
    public func take_while(this, predicate: func(ref This.Item) -> Bool) -> TakeWhile[This] {
        return TakeWhile { iter: this, predicate: predicate, done: false }
    }

    /// Skips elements while predicate is true
    public func skip_while(this, predicate: func(ref This.Item) -> Bool) -> SkipWhile[This] {
        return SkipWhile { iter: this, predicate: predicate, done: false }
    }
}
```

### Limiting Elements

```tml
extend Iterator {
    /// Takes at most n elements
    public func take(this, n: U64) -> Take[This] {
        return Take { iter: this, remaining: n }
    }

    /// Skips the first n elements
    public func skip(this, n: U64) -> Skip[This] {
        return Skip { iter: this, remaining: n }
    }

    /// Takes every nth element
    public func step_by(this, step: U64) -> StepBy[This] {
        assert(step > 0, "step must be positive")
        return StepBy { iter: this, step: step, first: true }
    }
}
```

### Combining Iterators

```tml
extend Iterator {
    /// Chains two iterators
    public func chain[U](this, other: U) -> Chain[This, U.Iter]
        where U: IntoIterator[Item = This.Item]
    {
        return Chain { first: Just(this), second: other.into_iter() }
    }

    /// Zips two iterators into pairs
    public func zip[U](this, other: U) -> Zip[This, U.Iter]
        where U: IntoIterator
    {
        return Zip { a: this, b: other.into_iter() }
    }

    /// Interleaves two iterators
    public func interleave[U](this, other: U) -> Interleave[This, U.Iter]
        where U: IntoIterator[Item = This.Item]
    {
        return Interleave { a: this, b: other.into_iter(), flag: false }
    }

    /// Adds index to each element
    public func enumerate(this) -> Enumerate[This] {
        return Enumerate { iter: this, count: 0 }
    }
}
```

### Peeking

```tml
extend Iterator {
    /// Creates an iterator that can peek ahead
    public func peekable(this) -> Peekable[This] {
        return Peekable { iter: this, peeked: None }
    }
}

/// An iterator that can look at the next element without consuming it
public type Peekable[I: Iterator] {
    iter: I,
    peeked: Maybe[Maybe[I.Item]],
}

extend Peekable[I] where I: Iterator {
    /// Returns a reference to the next element without consuming it
    public func peek(mut this) -> Maybe[ref I.Item] {
        if this.peeked.is_none() then {
            this.peeked = Just(this.iter.next())
        }
        return this.peeked.as_ref().unwrap().as_ref()
    }

    /// Returns a mutable reference to the next element
    public func peek_mut(mut this) -> Maybe[mut ref I.Item] {
        if this.peeked.is_none() then {
            this.peeked = Just(this.iter.next())
        }
        return this.peeked.as_mut().unwrap().as_mut()
    }

    /// Consumes the next element if it matches the predicate
    public func next_if(mut this, predicate: func(ref I.Item) -> Bool) -> Maybe[I.Item] {
        when this.peek() {
            Just(item) if predicate(item) -> return this.next(),
            _ -> return None,
        }
    }
}
```

### Reversing and Cycling

```tml
extend DoubleEndedIterator {
    /// Reverses the iterator
    public func rev(this) -> Rev[This] {
        return Rev { iter: this }
    }
}

extend Iterator where This.Item: Duplicate {
    /// Cycles through the iterator infinitely
    public func cycle(this) -> Cycle[This] {
        return Cycle { original: this.duplicate(), current: this }
    }
}
```

---

## Consuming Adapters

These methods consume the iterator and produce a result.

### Collecting

```tml
extend Iterator {
    /// Collects all elements into a collection
    public func collect[C](mut this) -> C
        where C: FromIterator[This.Item]
    {
        return C.from_iter(this)
    }

    /// Collects into a Vec
    public func to_vec(mut this) -> Vec[This.Item] {
        return this.collect()
    }
}
```

### Folding and Reducing

```tml
extend Iterator {
    /// Folds all elements into an accumulator
    public func fold[B](mut this, init: B, f: func(B, This.Item) -> B) -> B {
        var acc = init
        loop item in this {
            acc = f(acc, item)
        }
        return acc
    }

    /// Reduces elements using a binary operation
    public func reduce(mut this, f: func(This.Item, This.Item) -> This.Item) -> Maybe[This.Item] {
        let first = this.next()!
        return Just(this.fold(first, f))
    }

    /// Applies a function producing a Result, collecting results
    public func try_fold[B, E](mut this, init: B, f: func(B, This.Item) -> Outcome[B, E]) -> Outcome[B, E] {
        var acc = init
        loop item in this {
            acc = f(acc, item)!
        }
        return Success(acc)
    }
}
```

### Finding

```tml
extend Iterator {
    /// Finds the first element matching a predicate
    public func find(mut this, predicate: func(ref This.Item) -> Bool) -> Maybe[This.Item] {
        loop item in this {
            if predicate(ref item) then return Just(item)
        }
        return None
    }

    /// Finds and transforms in one step
    public func find_map[B](mut this, f: func(This.Item) -> Maybe[B]) -> Maybe[B] {
        loop item in this {
            when f(item) {
                Just(b) -> return Just(b),
                Nothing -> continue,
            }
        }
        return None
    }

    /// Returns the position of the first matching element
    public func position(mut this, predicate: func(This.Item) -> Bool) -> Maybe[U64] {
        var i: U64 = 0
        loop item in this {
            if predicate(item) then return Just(i)
            i = i + 1
        }
        return None
    }
}
```

### Aggregating

```tml
extend Iterator {
    /// Returns the number of elements
    public func count(mut this) -> U64 {
        var n: U64 = 0
        loop _ in this {
            n = n + 1
        }
        return n
    }

    /// Returns the last element
    public func last(mut this) -> Maybe[This.Item] {
        var result: Maybe[This.Item] = None
        loop item in this {
            result = Just(item)
        }
        return result
    }

    /// Returns the nth element
    public func nth(mut this, n: U64) -> Maybe[This.Item] {
        var i: U64 = 0
        loop item in this {
            if i == n then return Just(item)
            i = i + 1
        }
        return None
    }
}

extend Iterator where This.Item: Ord {
    /// Returns the minimum element
    public func min(mut this) -> Maybe[This.Item] {
        this.reduce(do(a, b) if a < b then a else b)
    }

    /// Returns the maximum element
    public func max(mut this) -> Maybe[This.Item] {
        this.reduce(do(a, b) if a > b then a else b)
    }

    /// Returns both min and max
    public func min_max(mut this) -> Maybe[(This.Item, This.Item)]
        where This.Item: Duplicate
    {
        let first = this.next()!
        var min = first.duplicate()
        var max = first

        loop item in this {
            if item < min then min = item.duplicate()
            if item > max then max = item.duplicate()
        }

        return Just((min, max))
    }
}

extend Iterator where This.Item: Numeric {
    /// Returns the sum of all elements
    public func sum(mut this) -> This.Item {
        this.fold(This.Item.zero(), do(a, b) a + b)
    }

    /// Returns the product of all elements
    public func product(mut this) -> This.Item {
        this.fold(This.Item.one(), do(a, b) a * b)
    }
}
```

### Testing

```tml
extend Iterator {
    /// Returns true if any element matches the predicate
    public func any(mut this, predicate: func(This.Item) -> Bool) -> Bool {
        loop item in this {
            if predicate(item) then return true
        }
        return false
    }

    /// Returns true if all elements match the predicate
    public func all(mut this, predicate: func(This.Item) -> Bool) -> Bool {
        loop item in this {
            if not predicate(item) then return false
        }
        return true
    }
}
```

### Comparison

```tml
extend Iterator where This.Item: Eq {
    /// Compares two iterators for equality
    public func eq[I](mut this, other: mut I) -> Bool
        where I: Iterator[Item = This.Item]
    {
        loop {
            when (this.next(), other.next()) {
                (Just(a), Just(b)) -> {
                    if a != b then return false
                },
                (None, None) -> return true,
                _ -> return false,
            }
        }
    }
}

extend Iterator where This.Item: Ord {
    /// Lexicographically compares two iterators
    public func cmp[I](mut this, other: mut I) -> Ordering
        where I: Iterator[Item = This.Item]
    {
        loop {
            when (this.next(), other.next()) {
                (Just(a), Just(b)) -> {
                    let ord = a.cmp(ref b)
                    if ord != Ordering.Equal then return ord
                },
                (Just(_), None) -> return Ordering.Greater,
                (None, Just(_)) -> return Ordering.Less,
                (None, None) -> return Ordering.Equal,
            }
        }
    }
}
```

### Partitioning

```tml
extend Iterator {
    /// Partitions elements into two collections based on predicate
    public func partition[C](mut this, predicate: func(ref This.Item) -> Bool) -> (C, C)
        where C: Default + Extend[This.Item]
    {
        var left = C.default()
        var right = C.default()

        loop item in this {
            if predicate(ref item) then {
                left.extend(once(item))
            } else {
                right.extend(once(item))
            }
        }

        return (left, right)
    }

    /// Unzips an iterator of pairs
    public func unzip[A, B, CA, CB](mut this) -> (CA, CB)
        where
            This.Item = (A, B),
            CA: Default + Extend[A],
            CB: Default + Extend[B]
    {
        var left = CA.default()
        var right = CB.default()

        loop (a, b) in this {
            left.extend(once(a))
            right.extend(once(b))
        }

        return (left, right)
    }
}
```

---

## Iterator Constructors

### Standard Constructors

```tml
/// Creates an empty iterator
public func empty[T]() -> Empty[T] {
    return Empty {}
}

/// Creates an iterator that yields one element
public func once[T](value: T) -> Once[T] {
    return Once { value: Just(value) }
}

/// Creates an iterator that repeats a value forever
public func repeat[T](value: T) -> Repeat[T]
    where T: Duplicate
{
    return Repeat { value: value }
}

/// Creates an iterator that repeats a value n times
public func repeat_n[T](value: T, n: U64) -> RepeatN[T]
    where T: Duplicate
{
    return RepeatN { value: value, remaining: n }
}

/// Creates an iterator from a function
public func from_fn[T](f: func() -> Maybe[T]) -> FromFn[T] {
    return FromFn { f: f }
}

/// Creates an iterator by repeatedly applying a function
public func successors[T](first: Maybe[T], succ: func(ref T) -> Maybe[T]) -> Successors[T] {
    return Successors { next: first, succ: succ }
}
```

### Range Iterators

```tml
/// Range iterator (start..end)
public type Range[T: Numeric] {
    start: T,
    end: T,
}

implement Iterator for Range[T] where T: Numeric {
    type Item = T

    func next(mut this) -> Maybe[T] {
        if this.start >= this.end then return None
        let value = this.start
        this.start = this.start + T.one()
        return Just(value)
    }

    func size_hint(this) -> (U64, Maybe[U64]) {
        let len = if this.end > this.start then {
            (this.end - this.start).to_u64()
        } else {
            0
        }
        return (len, Just(len))
    }
}

implement DoubleEndedIterator for Range[T] where T: Numeric {
    func next_back(mut this) -> Maybe[T] {
        if this.start >= this.end then return None
        this.end = this.end - T.one()
        return Just(this.end)
    }
}

/// Inclusive range iterator (start..=end)
public type RangeInclusive[T: Numeric] {
    start: T,
    end: T,
    exhausted: Bool,
}

implement Iterator for RangeInclusive[T] where T: Numeric {
    type Item = T

    func next(mut this) -> Maybe[T] {
        if this.exhausted then return None
        if this.start > this.end then return None

        let value = this.start
        if this.start == this.end then {
            this.exhausted = true
        } else {
            this.start = this.start + T.one()
        }
        return Just(value)
    }
}
```

---

## Adapter Types

### Map

```tml
public type Map[I: Iterator, B] {
    iter: I,
    f: func(I.Item) -> B,
}

implement Iterator for Map[I, B] where I: Iterator {
    type Item = B

    func next(mut this) -> Maybe[B] {
        this.iter.next().map(this.f)
    }

    func size_hint(this) -> (U64, Maybe[U64]) {
        this.iter.size_hint()
    }
}

implement ExactSizeIterator for Map[I, B] where I: ExactSizeIterator {
    func len(this) -> U64 { this.iter.len() }
}

implement DoubleEndedIterator for Map[I, B] where I: DoubleEndedIterator {
    func next_back(mut this) -> Maybe[B] {
        this.iter.next_back().map(this.f)
    }
}
```

### Filter

```tml
public type Filter[I: Iterator] {
    iter: I,
    predicate: func(ref I.Item) -> Bool,
}

implement Iterator for Filter[I] where I: Iterator {
    type Item = I.Item

    func next(mut this) -> Maybe[I.Item] {
        loop {
            when this.iter.next() {
                Just(item) if (this.predicate)(ref item) -> return Just(item),
                Just(_) -> continue,
                Nothing -> return None,
            }
        }
    }

    func size_hint(this) -> (U64, Maybe[U64]) {
        let (_, upper) = this.iter.size_hint()
        return (0, upper)
    }
}
```

### Enumerate

```tml
public type Enumerate[I: Iterator] {
    iter: I,
    count: U64,
}

implement Iterator for Enumerate[I] where I: Iterator {
    type Item = (U64, I.Item)

    func next(mut this) -> Maybe[(U64, I.Item)] {
        let item = this.iter.next()!
        let index = this.count
        this.count = this.count + 1
        return Just((index, item))
    }

    func size_hint(this) -> (U64, Maybe[U64]) {
        this.iter.size_hint()
    }
}
```

### Zip

```tml
public type Zip[A: Iterator, B: Iterator] {
    a: A,
    b: B,
}

implement Iterator for Zip[A, B] where A: Iterator, B: Iterator {
    type Item = (A.Item, B.Item)

    func next(mut this) -> Maybe[(A.Item, B.Item)] {
        let a = this.a.next()!
        let b = this.b.next()!
        return Just((a, b))
    }

    func size_hint(this) -> (U64, Maybe[U64]) {
        let (a_lower, a_upper) = this.a.size_hint()
        let (b_lower, b_upper) = this.b.size_hint()

        let lower = a_lower.min(b_lower)
        let upper = when (a_upper, b_upper) {
            (Just(a), Just(b)) -> Just(a.min(b)),
            (Just(a), None) -> Just(a),
            (None, Just(b)) -> Just(b),
            (None, None) -> Nothing,
        }

        return (lower, upper)
    }
}
```

### Chain

```tml
public type Chain[A: Iterator, B: Iterator] {
    first: Maybe[A],
    second: B,
}

implement Iterator for Chain[A, B]
    where A: Iterator, B: Iterator[Item = A.Item]
{
    type Item = A.Item

    func next(mut this) -> Maybe[A.Item] {
        when this.first.as_mut() {
            Just(first) -> {
                when first.next() {
                    Just(item) -> return Just(item),
                    Nothing -> this.first = None,
                }
            },
            Nothing -> {},
        }
        return this.second.next()
    }
}
```

### Take / Skip

```tml
public type Take[I: Iterator] {
    iter: I,
    remaining: U64,
}

implement Iterator for Take[I] where I: Iterator {
    type Item = I.Item

    func next(mut this) -> Maybe[I.Item] {
        if this.remaining == 0 then return None
        this.remaining = this.remaining - 1
        return this.iter.next()
    }

    func size_hint(this) -> (U64, Maybe[U64]) {
        let (lower, upper) = this.iter.size_hint()
        let lower = lower.min(this.remaining)
        let upper = upper.map(do(u) u.min(this.remaining)).or(Just(this.remaining))
        return (lower, upper)
    }
}

public type Skip[I: Iterator] {
    iter: I,
    remaining: U64,
}

implement Iterator for Skip[I] where I: Iterator {
    type Item = I.Item

    func next(mut this) -> Maybe[I.Item] {
        loop this.remaining > 0 {
            this.remaining = this.remaining - 1
            this.iter.next()
        }
        return this.iter.next()
    }
}
```

---

## Examples

### Basic Iteration

```tml
import std.iter.{Iterator, IntoIterator}
import std.collections.Vec

func basic_examples() {
    let numbers = Vec.from_slice(ref [1, 2, 3, 4, 5])

    // Map and filter
    let result: Vec[I32] = numbers.iter()
        .map(do(n) n * 2)
        .filter(do(n) n > 4)
        .collect()
    // result: [6, 8, 10]

    // Find
    let first_even = numbers.iter()
        .find(do(n) n % 2 == 0)
    // first_even: Just(2)

    // Fold
    let sum = numbers.iter().fold(0, do(acc, n) acc + n)
    // sum: 15
}
```

### Chaining and Zipping

```tml
func chaining_examples() {
    let a = Vec.from_slice(ref [1, 2, 3])
    let b = Vec.from_slice(ref [4, 5, 6])

    // Chain
    let chained: Vec[I32] = a.iter().chain(b.iter()).collect()
    // chained: [1, 2, 3, 4, 5, 6]

    // Zip
    let zipped: Vec[(I32, I32)] = a.iter().zip(b.iter()).collect()
    // zipped: [(1, 4), (2, 5), (3, 6)]

    // Enumerate
    loop (i, val) in a.iter().enumerate() {
        print("Index " + i.to_string() + ": " + val.to_string())
    }
}
```

### Lazy Evaluation

```tml
func lazy_examples() {
    // Nothing happens until collect()
    let iter = (0 to 1000000)
        .map(do(n) n * 2)
        .filter(do(n) n % 3 == 0)
        .take(10)

    // Only processes 10 elements
    let result: Vec[I64] = iter.collect()
}
```

### Custom Iterator

```tml
/// Fibonacci iterator
type Fibonacci {
    curr: U64,
    next: U64,
}

func fibonacci() -> Fibonacci {
    return Fibonacci { curr: 0, next: 1 }
}

implement Iterator for Fibonacci {
    type Item = U64

    func next(mut this) -> Maybe[U64] {
        let result = this.curr
        this.curr = this.next
        this.next = result + this.next
        return Just(result)
    }
}

func use_fibonacci() {
    let fibs: Vec[U64] = fibonacci().take(10).collect()
    // fibs: [0, 1, 1, 2, 3, 5, 8, 13, 21, 34]
}
```

---

## Performance

### Zero-Cost Abstractions

Iterator chains compile to efficient loops equivalent to hand-written code:

```tml
// This:
let sum = numbers.iter()
    .map(do(n) n * 2)
    .filter(do(n) n > 0)
    .sum()

// Compiles to equivalent of:
var sum = 0
loop n in numbers {
    let doubled = n * 2
    if doubled > 0 then {
        sum = sum + doubled
    }
}
```

### Hints

- Use `size_hint()` to pre-allocate collections
- Prefer `for_each` over `collect` when you don't need the result
- Use `take` to limit infinite iterators
- Chain operations for single-pass iteration

---

## See Also

- [std.collections](./10-COLLECTIONS.md) — Collection types
- [03-GRAMMAR.md](../specs/03-GRAMMAR.md) — Loop syntax
- [05-SEMANTICS.md](../specs/05-SEMANTICS.md) — Trait semantics
