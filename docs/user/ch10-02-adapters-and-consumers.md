# Iterator Adapters and Consumers

Iterator methods fall into two categories:

- **Adapters** return a new iterator. They are lazy: no work is done until an element is pulled through.
- **Consumers** drive the iteration to completion and produce a final value or side effect.

A typical pipeline ends with exactly one consumer. Everything before it is composed of adapters.

```tml
let result = data
    .iter()          // obtain iterator
    .filter(...)     // adapter
    .map(...)        // adapter
    .take(5)         // adapter
    .collect()       // consumer — drives everything above
```

## Adapters

### `map`

Transform each element using a closure:

```tml
let numbers = [1, 2, 3, 4, 5]
let squares = numbers.iter().map(do(x) x * x).collect()
// [1, 4, 9, 16, 25]

let words = ["hello", "world"]
let upper = words.iter().map(do(w) w.to_uppercase()).collect()
// ["HELLO", "WORLD"]
```

The closure receives one element and returns the transformed value. The output element type can differ from the input type.

### `filter`

Keep only elements for which a predicate returns `true`:

```tml
let values = [1, 2, 3, 4, 5, 6, 7, 8]
let evens = values.iter().filter(do(x) x % 2 == 0).collect()
// [2, 4, 6, 8]

let words = ["cat", "elephant", "ant", "giraffe", "ox"]
let long_words = words.iter().filter(do(w) w.len() > 3).collect()
// ["elephant", "giraffe"]
```

Elements for which the predicate returns `false` are discarded.

### `take`

Produce at most the first `n` elements, then stop:

```tml
let numbers = [10, 20, 30, 40, 50, 60]
let first_three = numbers.iter().take(3).collect()
// [10, 20, 30]
```

`take` is the standard way to bound an infinite iterator:

```tml
let fibs = Fibonacci { a: 0, b: 1 }
let first_eight = fibs.take(8).collect()
// [0, 1, 1, 2, 3, 5, 8, 13]
```

### `skip`

Discard the first `n` elements, then yield the rest:

```tml
let data = [1, 2, 3, 4, 5, 6]
let tail = data.iter().skip(2).collect()
// [3, 4, 5, 6]
```

Combine `skip` and `take` to extract a window from a sequence:

```tml
let page = data.iter().skip(2).take(3).collect()
// [3, 4, 5]  (elements at positions 2, 3, 4)
```

### `enumerate`

Pair each element with its zero-based index:

```tml
let colors = ["red", "green", "blue"]
let indexed = colors.iter().enumerate().collect()
// [(0, "red"), (1, "green"), (2, "blue")]

for (i, color) in colors.iter().enumerate() {
    println(i.to_string() + ": " + color)
}
// 0: red
// 1: green
// 2: blue
```

### `zip`

Pair elements from two iterators together, stopping when either is exhausted:

```tml
let names  = ["Alice", "Bob", "Carol"]
let scores = [95, 82, 78]

let pairs = names.iter().zip(scores.iter()).collect()
// [("Alice", 95), ("Bob", 82), ("Carol", 78)]

for (name, score) in names.iter().zip(scores.iter()) {
    println(name + ": " + score.to_string())
}
// Alice: 95
// Bob: 82
// Carol: 78
```

If the two iterators have different lengths, the shorter one determines the result length.

### `chain`

Concatenate two iterators into a single sequence:

```tml
let first  = [1, 2, 3]
let second = [4, 5, 6]
let combined = first.iter().chain(second.iter()).collect()
// [1, 2, 3, 4, 5, 6]
```

### `flat_map`

Apply a function that returns an iterator to each element, then flatten one level:

```tml
let sentences = ["hello world", "foo bar baz"]
let all_words = sentences
    .iter()
    .flat_map(do(s) s.split(" ").iter())
    .collect()
// ["hello", "world", "foo", "bar", "baz"]
```

`flat_map` is equivalent to `.map(f).flatten()`.

### `flatten`

Remove one level of nesting from an iterator of iterators:

```tml
let nested = [[1, 2], [3, 4], [5, 6]]
let flat = nested.iter().flatten().collect()
// [1, 2, 3, 4, 5, 6]
```

### `take_while`

Yield elements as long as a predicate holds; stop at the first element that does not:

```tml
let data = [2, 4, 6, 7, 8, 10]
let prefix = data.iter().take_while(do(x) x % 2 == 0).collect()
// [2, 4, 6]  — stops at 7 (odd)
```

### `skip_while`

Skip elements while a predicate holds; yield the rest from the first non-matching element onward:

```tml
let data = [1, 2, 3, 10, 11, 12]
let tail = data.iter().skip_while(do(x) x < 10).collect()
// [10, 11, 12]
```

### `inspect`

Peek at each element for debugging without affecting the sequence:

```tml
let result = numbers
    .iter()
    .inspect(do(x) println("before filter: " + x.to_string()))
    .filter(do(x) x > 3)
    .inspect(do(x) println("after filter: " + x.to_string()))
    .collect()
```

`inspect` is intended for debugging pipelines. Remove it before shipping production code.

## Consumers

Consumers drive the pipeline, pulling elements through all the adapters until the sequence is exhausted (or a termination condition is met).

### `collect`

Gather all elements into a collection:

```tml
let numbers = [1, 2, 3, 4, 5]
let doubled: [I32] = numbers.iter().map(do(x) x * 2).collect()
// [2, 4, 6, 8, 10]
```

The target type is inferred from context. `collect` allocates a new collection to hold the results.

### `for_each`

Execute a side-effecting closure for each element without producing a return value:

```tml
let items = ["apple", "banana", "cherry"]
items.iter().for_each(do(item) println(item))
// apple
// banana
// cherry
```

Prefer `for` loops for simple cases; use `for_each` when building pipelines where the final step is a side effect.

### `fold`

Accumulate elements into a single value using a starting value and a combining closure:

```tml
let numbers = [1, 2, 3, 4, 5]

let sum = numbers.iter().fold(0, do(acc, x) acc + x)
// 15

let product = numbers.iter().fold(1, do(acc, x) acc * x)
// 120

let concatenated = ["a", "b", "c"].iter().fold("", do(acc, s) acc + s)
// "abc"
```

`fold` starts with `init`, then repeatedly calls `f(accumulator, next_element)` and stores the result as the new accumulator.

### `reduce`

Like `fold`, but uses the first element as the initial accumulator. Returns `Maybe` because an empty iterator has no first element:

```tml
let numbers = [3, 1, 4, 1, 5, 9, 2, 6]
let max = numbers.iter().reduce(do(a, b) if a > b then a else b)
// Just(9)

let empty: [I32] = []
let result = empty.iter().reduce(do(a, b) a + b)
// Nothing
```

### `sum`

Sum all numeric elements:

```tml
let values = [10, 20, 30, 40]
let total = values.iter().sum()
println(total.to_string())  // 100
```

`sum` is equivalent to `.fold(0, do(acc, x) acc + x)` but reads more clearly for the common case.

### `product`

Multiply all numeric elements:

```tml
let factors = [1, 2, 3, 4, 5]
let result = factors.iter().product()
println(result.to_string())  // 120
```

### `count`

Count how many elements the iterator produces:

```tml
let data = [1, 2, 3, 4, 5, 6]
let even_count = data.iter().filter(do(x) x % 2 == 0).count()
println(even_count.to_string())  // 3
```

### `any`

Return `true` if at least one element satisfies the predicate. Short-circuits on the first match:

```tml
let numbers = [1, 3, 5, 6, 7]
let has_even = numbers.iter().any(do(x) x % 2 == 0)
// true  (stops after reaching 6)

let all_odd = numbers.iter().any(do(x) x % 2 == 0)
// true
```

### `all`

Return `true` if every element satisfies the predicate. Short-circuits on the first failure:

```tml
let numbers = [2, 4, 6, 8]
let all_even = numbers.iter().all(do(x) x % 2 == 0)
// true

let mixed = [2, 4, 5, 8]
let mixed_even = mixed.iter().all(do(x) x % 2 == 0)
// false  (stops after reaching 5)
```

### `find`

Return the first element matching a predicate, or `Nothing`:

```tml
let users = ["alice", "bob", "carol", "dave"]
let found = users.iter().find(do(u) u.starts_with("c"))
// Just("carol")

let missing = users.iter().find(do(u) u.starts_with("z"))
// Nothing
```

### `position`

Return the index of the first matching element, or `Nothing`:

```tml
let items = [10, 20, 30, 40, 50]
let pos = items.iter().position(do(x) x == 30)
// Just(2)

let absent = items.iter().position(do(x) x == 99)
// Nothing
```

### `last`

Return the last element, or `Nothing` for an empty iterator:

```tml
let numbers = [1, 2, 3, 4, 5]
let final_val = numbers.iter().last()
// Just(5)

let empty: [I32] = []
let empty_last = empty.iter().last()
// Nothing
```

`last` must consume the entire iterator to find the final element.

### `min` and `max`

Return the smallest or largest element, or `Nothing` for an empty iterator:

```tml
let values = [3, 1, 4, 1, 5, 9, 2, 6]
let smallest = values.iter().min()  // Just(1)
let largest  = values.iter().max()  // Just(9)
```

Elements must implement the `Ord` behavior for comparison.

### `nth`

Return the element at a given zero-based index, or `Nothing`:

```tml
let letters = ["a", "b", "c", "d", "e"]
let third = letters.iter().nth(2)
// Just("c")
```

`nth` consumes and discards the first `n` elements.

## Building Data-Processing Pipelines

Iterator adapters compose cleanly. Each step describes what to do; the compiler handles how:

```tml
// Find the sum of squares of all even numbers between 1 and 20
let result = (1 to 21)
    .iter()
    .filter(do(x) x % 2 == 0)
    .map(do(x) x * x)
    .sum()
println(result.to_string())  // 1540
```

```tml
// Extract the top 3 scores above 70, formatted as strings
let scores = [45, 82, 91, 67, 78, 95, 53, 88]
let report = scores
    .iter()
    .filter(do(s) s > 70)
    .map(do(s) s.to_string() + " points")
    .take(3)
    .collect()
// ["82 points", "91 points", "78 points"]
```

```tml
// Count unique word lengths in a list of words
let words = ["cat", "dog", "elephant", "ox", "ant", "bee"]
let total_chars = words.iter().map(do(w) w.len()).sum()
println(total_chars.to_string())  // 22
```

## Adapter and Consumer Reference

### Adapters (lazy — return a new iterator)

| Method | Description |
|---|---|
| `.map(do(x) expr)` | Transform each element |
| `.filter(do(x) bool)` | Keep elements matching predicate |
| `.take(n)` | Yield at most `n` elements |
| `.skip(n)` | Skip first `n` elements |
| `.take_while(do(x) bool)` | Yield while predicate holds |
| `.skip_while(do(x) bool)` | Skip while predicate holds |
| `.enumerate()` | Pair each element with its index `(i, x)` |
| `.zip(other)` | Pair elements from two iterators |
| `.chain(other)` | Concatenate two iterators |
| `.flat_map(do(x) iter)` | Map then flatten one level |
| `.flatten()` | Flatten one level of nesting |
| `.inspect(do(x) action)` | Peek at elements for debugging |

### Consumers (eager — drive evaluation)

| Method | Returns | Description |
|---|---|---|
| `.collect()` | `[T]` | Gather all elements into a collection |
| `.for_each(do(x) action)` | `Unit` | Execute action for each element |
| `.fold(init, do(acc, x) expr)` | `T` | Accumulate into a value with starting point |
| `.reduce(do(a, b) expr)` | `Maybe[T]` | Accumulate using first element as start |
| `.sum()` | `T` | Sum all elements |
| `.product()` | `T` | Multiply all elements |
| `.count()` | `I64` | Count elements |
| `.any(do(x) bool)` | `Bool` | True if any element matches |
| `.all(do(x) bool)` | `Bool` | True if all elements match |
| `.find(do(x) bool)` | `Maybe[T]` | First element matching predicate |
| `.position(do(x) bool)` | `Maybe[I64]` | Index of first matching element |
| `.last()` | `Maybe[T]` | Last element |
| `.min()` | `Maybe[T]` | Smallest element |
| `.max()` | `Maybe[T]` | Largest element |
| `.nth(n)` | `Maybe[T]` | Element at index `n` |

## Performance Notes

Iterator chains are zero-cost abstractions. The compiler inlines each adapter and eliminates the overhead of virtual dispatch and intermediate allocations. The generated machine code is equivalent to a hand-written loop performing all the same steps. You can chain as many adapters as you need without paying a per-adapter allocation cost.

The one place where memory is allocated is `.collect()`, which produces a new collection. All adapters before it operate without allocation.
