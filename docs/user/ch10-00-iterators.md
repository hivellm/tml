# Iterators

An iterator is a value that produces a sequence of elements one at a time. Iterators in TML are lazy: no element is computed until it is actually needed. This means you can describe complex data transformations as chains of operations without allocating intermediate collections at each step.

```tml
let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

let result = numbers
    .iter()
    .filter(do(x) x % 2 == 0)
    .map(do(x) x * x)
    .sum()

println(result.to_string())  // 220  (4 + 16 + 36 + 64 + 100)
```

Each step in the chain — `.filter(...)`, `.map(...)` — produces a new iterator that wraps the previous one. Nothing is evaluated until `.sum()` pulls elements through the entire chain. This is called *lazy evaluation*.

## The `Iterator` Behavior

Any type that implements the `Iterator` behavior can participate in the iterator system:

```tml
behavior Iterator {
    type Item
    func next(mut this) -> Maybe[Item]
}
```

`next` returns `Just(value)` for the next element, or `Nothing` when the sequence is exhausted. All adapters and consumers in the standard library are built on this single method.

## Zero-Cost Abstraction

Iterator chains compile to the same machine code as hand-written loops. The compiler inlines adapters and eliminates intermediate state, so there is no runtime overhead from using the iterator API versus writing the loop manually. You pay only for what you use.

## What This Chapter Covers

- **Iterator Basics** (ch10-01) — how iterators work, the `for` loop and `.iter()`, implementing a custom iterator, and iterating with indices.
- **Adapters and Consumers** (ch10-02) — the full set of lazy adapter methods (`map`, `filter`, `take`, `zip`, `chain`, and more) and consumer methods that drive evaluation (`collect`, `fold`, `sum`, `any`, `all`, `find`).
