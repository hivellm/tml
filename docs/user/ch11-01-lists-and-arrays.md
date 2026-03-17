# Lists and Arrays

TML provides two sequence types: fixed-size arrays that live on the stack, and `List[T]`, a heap-allocated growable sequence. Understanding when to use each is central to writing efficient TML code.

## Fixed-Size Arrays

An array with a compile-time-known size is written `[T; N]`. The size is part of the type — `[I32; 3]` and `[I32; 5]` are different types. Arrays are stack-allocated, which makes them fast to create and access.

### Creating Arrays

```tml
// Array literal — type is inferred as [I32; 5]
let primes: [I32; 5] = [2, 3, 5, 7, 11]

// Repeated-value syntax — 8 zeros
let zeros: [I32; 8] = [0; 8]

// Let the compiler infer the size
let flags = [true, false, true, false]  // [Bool; 4]
```

### Indexing

Elements are accessed with square brackets. Indices are zero-based. Accessing an out-of-bounds index panics at runtime.

```tml
let arr: [I32; 5] = [10, 20, 30, 40, 50]

let first = arr[0]   // 10
let middle = arr[2]  // 30
let last = arr[4]    // 50
```

### Array Methods

Fixed-size arrays have a small set of built-in methods:

```tml
let arr: [I32; 5] = [3, 1, 4, 1, 5]

let n = arr.len()      // 5 — always the compile-time size
let is_e = arr.is_empty()  // false (always false for N > 0)
```

### Iterating Over an Array

Use a `for` loop with a range to iterate by index:

```tml
let arr: [I32; 5] = [2, 4, 6, 8, 10]
var sum: I32 = 0
for i in 0 to arr.len() {
    sum = sum + arr[i]
}
// sum == 30
```

### Transforming with `map`

`map` applies a closure to every element and returns a new array of the same size:

```tml
let arr: [I32; 4] = [1, 2, 3, 4]
let doubled: [I32; 4] = arr.map(do(x: I32) -> I32 { x * 2 })
// doubled == [2, 4, 6, 8]
```

### Arrays as Function Parameters

When you pass an array to a function, the size must match exactly:

```tml
func sum_five(arr: [I32; 5]) -> I32 {
    return arr[0] + arr[1] + arr[2] + arr[3] + arr[4]
}

func main() -> I32 {
    let nums: [I32; 5] = [1, 2, 3, 4, 5]
    let total = sum_five(nums)  // 15
    return 0
}
```

If you need to accept arrays of varying sizes, use a slice parameter instead (see the chapter on ownership).

## List[T] — Dynamic Arrays

`List[T]` is a heap-allocated growable sequence. Use it when the number of elements is not known at compile time or when you need to add and remove elements at runtime.

### Creating a List

```tml
use std::collections::List

// Create with an initial capacity hint
let scores: List[I32] = List[I32].new(16)

// Create with the default capacity (8 elements)
let names: List[Str] = List[Str].default()
```

The capacity is a hint to avoid early reallocations — the list will grow beyond it automatically if needed.

### Pushing and Popping

```tml
use std::collections::List

let items: List[I32] = List[I32].new(8)

items.push(10)
items.push(20)
items.push(30)

let n = items.len()   // 3

let last = items.pop()  // returns 30; list now has 2 elements
```

`pop()` removes and returns the last element. It panics if the list is empty.

### Accessing Elements

```tml
let list: List[I32] = List[I32].new(4)
list.push(100)
list.push(200)
list.push(300)

let v0 = list.get(0)   // 100
let v1 = list.get(1)   // 200
let first = list.first()  // 100
let last = list.last()    // 300

list.set(1, 999)  // replace element at index 1
let updated = list.get(1)  // 999
```

`get()` and `set()` use zero-based indices. Out-of-bounds access panics.

### Querying the List

```tml
let list: List[I32] = List[I32].new(4)

list.is_empty()   // true  (before any pushes)
list.len()        // 0

list.push(42)

list.is_empty()   // false
list.len()        // 1
list.capacity()   // at least 4 (the initial hint)
```

### Clearing and Reusing

`clear()` removes all elements but keeps the allocated memory, so you can reuse the list without a new allocation:

```tml
let list: List[I32] = List[I32].new(8)
list.push(1)
list.push(2)
list.clear()

list.is_empty()  // true
list.push(99)    // works — memory is still allocated
```

### Freeing Memory

When you are done with a list, call `destroy()` to release the heap memory:

```tml
func process() -> I32 {
    let list: List[I32] = List[I32].new(8)
    list.push(1)
    list.push(2)
    // ... work with list ...
    list.destroy()  // free heap memory
    return 0
}
```

### Method Reference

| Method | Description |
|---|---|
| `List[T].new(capacity: I64)` | Create with initial capacity hint |
| `List[T].default()` | Create with default capacity (8) |
| `push(item: T)` | Append element to the end |
| `pop() -> T` | Remove and return the last element (panics if empty) |
| `get(index: I64) -> T` | Return element at index (panics if out of bounds) |
| `set(index: I64, value: T)` | Replace element at index (panics if out of bounds) |
| `first() -> T` | Return the first element (panics if empty) |
| `last() -> T` | Return the last element (panics if empty) |
| `len() -> I64` | Number of elements |
| `is_empty() -> Bool` | True if no elements |
| `capacity() -> I64` | Current allocated capacity |
| `clear()` | Remove all elements, retain allocation |
| `destroy()` | Free all heap memory |

## Common Patterns

### Building a List from Computation

```tml
use std::collections::List

func squares(n: I32) -> List[I32] {
    let result: List[I32] = List[I32].new(n as I64)
    for i in 1 through n {
        result.push(i * i)
    }
    return result
}

func main() -> I32 {
    let sq = squares(5)
    // sq contains [1, 4, 9, 16, 25]
    for i in 0 to sq.len() {
        let v = sq.get(i)
        println(v.to_string())
    }
    sq.destroy()
    return 0
}
```

### Summing a List

```tml
use std::collections::List

func sum(list: ref List[I32]) -> I32 {
    var total: I32 = 0
    for i in 0 to list.len() {
        total = total + list.get(i)
    }
    return total
}
```

### Filtering Elements

```tml
use std::collections::List

func evens(input: ref List[I32]) -> List[I32] {
    let output: List[I32] = List[I32].new(input.len())
    for i in 0 to input.len() {
        let v = input.get(i)
        if v % 2 == 0 {
            output.push(v)
        }
    }
    return output
}
```

### Stack Usage Pattern

`List[T]` works naturally as a stack: push to add, pop to remove:

```tml
use std::collections::List

func reverse(input: ref List[I32]) -> List[I32] {
    let stack: List[I32] = List[I32].new(input.len())
    for i in 0 to input.len() {
        stack.push(input.get(i))
    }
    let output: List[I32] = List[I32].new(input.len())
    loop (not stack.is_empty()) {
        output.push(stack.pop())
    }
    stack.destroy()
    return output
}
```

## Arrays vs. Lists: When to Use Each

| Situation | Choose |
|---|---|
| Size known at compile time, small, no heap allocation | `[T; N]` array |
| Size is dynamic or unbounded | `List[T]` |
| Passing to functions that operate on fixed sizes | `[T; N]` array |
| Returning a variable-length result | `List[T]` |
| Performance-critical inner loop with fixed size | `[T; N]` array |
| General-purpose sequence building | `List[T]` |
