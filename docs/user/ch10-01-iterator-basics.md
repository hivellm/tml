# Iterator Basics

This section explains how iterators work in TML, how to obtain them from collections, and how to implement the `Iterator` behavior for your own types.

## Iterating with `for`

The simplest way to consume an iterator is the `for` loop. It works directly over collections and ranges:

```tml
// Over an array
let fruits = ["apple", "banana", "cherry"]
for fruit in fruits {
    println(fruit)
}

// Over a range (exclusive end)
for i in 0 to 5 {
    println(i.to_string())
}
// 0 1 2 3 4

// Over a range (inclusive end)
for i in 1 through 5 {
    println(i.to_string())
}
// 1 2 3 4 5
```

The `for` loop calls the collection's iterator internally. It is equivalent to calling `.iter()` and repeatedly invoking `.next()` until `Nothing` is returned.

## Calling `.iter()` Explicitly

When you want to apply adapter methods before consuming, call `.iter()` to obtain the iterator explicitly:

```tml
let scores = [45, 82, 91, 67, 78, 95, 53]

// Count how many scores are above 80
let high_count = scores
    .iter()
    .filter(do(s) s > 80)
    .count()

println(high_count.to_string())  // 3
```

Calling `.iter()` on most collection types returns an iterator that yields immutable references to the collection's elements.

## The `Iterator` Behavior

The `Iterator` behavior requires a single method: `next`. It returns `Just(item)` for each successive element and `Nothing` when the sequence is exhausted:

```tml
behavior Iterator {
    type Item
    func next(mut this) -> Maybe[Item]
}
```

The `Item` associated type declares what kind of value the iterator produces. An iterator over integers has `type Item = I32`. An iterator over strings has `type Item = Str`.

## Implementing a Custom Iterator

Any struct can become an iterator by implementing the `Iterator` behavior. The struct holds whatever state is needed to track progress through the sequence.

### Example: Countdown

```tml
type Countdown {
    current: I32,
}

extend Countdown with Iterator {
    type Item = I32

    func next(mut this) -> Maybe[I32] {
        if this.current <= 0 {
            return Nothing
        }
        let value = this.current
        this.current = this.current - 1
        return Just(value)
    }
}

func main() -> I32 {
    let cd = Countdown { current: 5 }
    for n in cd {
        println(n.to_string())
    }
    // 5 4 3 2 1
    return 0
}
```

### Example: Fibonacci Sequence

An infinite iterator produces elements indefinitely. Combine it with `.take(n)` to consume a bounded prefix:

```tml
type Fibonacci {
    a: I64,
    b: I64,
}

extend Fibonacci with Iterator {
    type Item = I64

    func next(mut this) -> Maybe[I64] {
        let current = this.a
        this.a = this.b
        this.b = current + this.b
        return Just(current)
    }
}

func main() -> I32 {
    let fibs = Fibonacci { a: 0, b: 1 }
    let first_ten = fibs.take(10).collect()
    // [0, 1, 1, 2, 3, 5, 8, 13, 21, 34]

    for n in first_ten {
        println(n.to_string())
    }
    return 0
}
```

Because `Fibonacci` never returns `Nothing`, it is an infinite iterator. Calling `.collect()` directly on it would loop forever. Always pair infinite iterators with a bounding adapter such as `.take(n)`.

### Example: Stepping Range

A custom range that steps by a user-defined amount:

```tml
type StepRange {
    current: I32,
    end: I32,
    step: I32,
}

extend StepRange with Iterator {
    type Item = I32

    func next(mut this) -> Maybe[I32] {
        if this.current >= this.end {
            return Nothing
        }
        let value = this.current
        this.current = this.current + this.step
        return Just(value)
    }
}

func step_from(start: I32, end: I32, step: I32) -> StepRange {
    return StepRange { current: start, end: end, step: step }
}

func main() -> I32 {
    // 0, 2, 4, 6, 8
    for n in step_from(0, 10, 2) {
        println(n.to_string())
    }
    return 0
}
```

## Iterating with Indices

When you need both the element and its position, use `.enumerate()`. It transforms each element `x` into a pair `(index, x)`:

```tml
let items = ["first", "second", "third"]

for (i, item) in items.iter().enumerate() {
    println(i.to_string() + ": " + item)
}
// 0: first
// 1: second
// 2: third
```

Indices are zero-based and increment by one for each element.

## Consuming an Iterator Manually

You can drive iteration by calling `.next()` in a loop. This is the fundamental mechanism; all other iteration forms are built on top of it:

```tml
var iter = [10, 20, 30].iter()

loop {
    let item = iter.next()
    when item {
        Just(value) => println(value.to_string()),
        Nothing => break
    }
}
// 10
// 20
// 30
```

In practice, use `for` or adapter chains instead of writing this loop manually. The manual form is useful when you need to interleave iteration with other logic.

## Iterator State and Ownership

Each iterator value tracks its own position independently. Two iterators over the same collection do not share state:

```tml
let data = [1, 2, 3]
var iter_a = data.iter()
var iter_b = data.iter()

let a = iter_a.next()  // Just(1)
let b = iter_b.next()  // Just(1) — independent from iter_a
let a2 = iter_a.next() // Just(2)
```

Once an iterator is consumed (returns `Nothing`), it cannot be reset. Obtain a fresh iterator from the collection to iterate again.

## Lazy Evaluation

Iterators do not compute elements until they are requested. Creating an adapter chain performs no work:

```tml
// No computation happens here
let pipeline = numbers.iter().filter(do(x) x > 0).map(do(x) x * 2)

// Computation happens here, driven by for
for value in pipeline {
    println(value.to_string())
}
```

This is important to keep in mind: if you build a pipeline and never consume it, no work is done at all. The laziness also means infinite iterators are safe to construct; they only produce elements on demand.

## Summary

| Concept | How |
|---|---|
| Iterate over a collection | `for item in collection { }` |
| Iterate over a range | `for i in 0 to n { }` |
| Explicit iterator | `collection.iter()` |
| Custom iterator | `extend MyType with Iterator { type Item = T; func next ... }` |
| Index + element | `.enumerate()` → `(i, item)` |
| Manual iteration | `iter.next()` returns `Maybe[Item]` |
| Infinite iterator | Pair with `.take(n)` before consuming |

The next section covers the full set of adapter and consumer methods available on any iterator.
