# Closures as Arguments

Closures become most powerful when passed to and returned from other functions. This section covers how to write functions that accept closures, how to return closures, and practical patterns for building reusable higher-order utilities.

## Accepting a Closure

A function that accepts a closure declares its parameter with a `func` type:

```tml
func apply(f: func(I32) -> I32, x: I32) -> I32 {
    return f(x)
}

let result = apply(do(x) x * 2, 21)
println(result.to_string())  // 42
```

The type `func(I32) -> I32` means "a callable that takes one `I32` and returns an `I32`". Any closure or named function with that signature is accepted.

## Calling the Closure Inside a Function

Once a closure parameter is bound, calling it looks identical to calling a named function:

```tml
func transform_all(items: [I32], f: func(I32) -> I32) -> [I32] {
    var result: [I32] = []
    for item in items {
        result.push(f(item))
    }
    return result
}

let numbers = [1, 2, 3, 4, 5]
let squares = transform_all(numbers, do(x) x * x)
// squares = [1, 4, 9, 16, 25]
```

## Common Higher-Order Patterns

### Map

Apply a transformation to every element of a collection:

```tml
func map_list(items: [I32], f: func(I32) -> I32) -> [I32] {
    var out: [I32] = []
    for item in items {
        out.push(f(item))
    }
    return out
}

let data = [10, 20, 30]
let halved = map_list(data, do(x) x / 2)
// halved = [5, 10, 15]
```

### Filter

Keep only elements that satisfy a predicate:

```tml
func keep_if(items: [I32], pred: func(I32) -> Bool) -> [I32] {
    var out: [I32] = []
    for item in items {
        if pred(item) {
            out.push(item)
        }
    }
    return out
}

let values = [1, 2, 3, 4, 5, 6]
let odds = keep_if(values, do(x) x % 2 != 0)
// odds = [1, 3, 5]
```

### Fold (Reduce)

Accumulate a collection into a single value:

```tml
func fold(items: [I32], init: I32, f: func(I32, I32) -> I32) -> I32 {
    var acc = init
    for item in items {
        acc = f(acc, item)
    }
    return acc
}

let numbers = [1, 2, 3, 4, 5]
let sum     = fold(numbers, 0,  do(acc, x) acc + x)  // 15
let product = fold(numbers, 1,  do(acc, x) acc * x)  // 120
let max_val = fold(numbers, numbers[0], do(acc, x) if x > acc then x else acc)  // 5
```

### Sort with Custom Comparator

Pass a comparison function to sort in different orders:

```tml
// sort_with is a built-in method on List[T]
var words = ["banana", "apple", "cherry", "date"]

// Alphabetical
words.sort_with(do(a, b) a < b)

// By length, shortest first
words.sort_with(do(a, b) a.len() < b.len())
```

## Chaining Multiple Closures

Pass closures that call other closures to compose behavior:

```tml
let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

// Keep even numbers, then double them, then sum
let evens   = keep_if(numbers, do(x) x % 2 == 0)
let doubled = map_list(evens, do(x) x * 2)
let total   = fold(doubled, 0, do(acc, x) acc + x)
println(total.to_string())  // 60
```

## Returning Closures from Functions

A function can return a closure. The return type is written as a `func` type:

```tml
func make_adder(n: I32) -> func(I32) -> I32 {
    return do(x) x + n
}

let add5  = make_adder(5)
let add10 = make_adder(10)

println(add5(3).to_string())   // 8
println(add10(3).to_string())  // 13
```

`make_adder` returns a different closure for each value of `n`. The returned closure captures `n` from the parameter, so each closure carries its own independent copy.

### Factory Functions

This pattern is useful for generating specialized behavior at runtime:

```tml
func make_multiplier(factor: I32) -> func(I32) -> I32 {
    return do(x) x * factor
}

func make_threshold_filter(min_val: I32) -> func(I32) -> Bool {
    return do(x) x >= min_val
}

let double   = make_multiplier(2)
let triple   = make_multiplier(3)
let positive = make_threshold_filter(1)

let data = [-2, -1, 0, 1, 2, 3]
let pos_data  = keep_if(data, positive)     // [1, 2, 3]
let doubled   = map_list(pos_data, double)  // [2, 4, 6]
let tripled   = map_list(pos_data, triple)  // [3, 6, 9]
```

## Closures That Capture Mutable State

A closure can maintain state across calls by capturing a `var` variable:

```tml
var call_count = 0
let counted_add = do(a: I32, b: I32) -> I32 {
    call_count = call_count + 1
    return a + b
}

println(counted_add(1, 2).to_string())  // 3
println(counted_add(4, 5).to_string())  // 9
println(call_count.to_string())         // 2
```

Each call to `counted_add` increments `call_count` in the surrounding scope. This is a simple form of stateful behavior without defining a struct.

## Storing Closures in Structs

Structs can store non-capturing closures as function-typed fields. This lets you build components with pluggable behavior:

```tml
type Validator {
    check: func(I32) -> Bool,
    label: Str,
}

func validate_all(values: [I32], v: Validator) {
    for val in values {
        if not v.check(val) {
            println("Failed " + v.label + ": " + val.to_string())
        }
    }
}

let positive_check = Validator {
    check: do(x) x > 0,
    label: "positive",
}

let even_check = Validator {
    check: do(x) x % 2 == 0,
    label: "even",
}

let numbers = [2, -1, 4, 3, 6]
validate_all(numbers, positive_check)  // Failed positive: -1
validate_all(numbers, even_check)      // Failed even: -1, Failed even: 3
```

Note that struct fields using `func` types only accept non-capturing closures and named functions. If you need to store a capturing closure, pass the captured state as separate struct fields and combine them at call time.

## Generic Functions with Closure Bounds

For functions intended to work with many element types, use generics alongside closure parameters:

```tml
func find_first[T](items: [T], pred: func(T) -> Bool) -> Maybe[T] {
    for item in items {
        if pred(item) {
            return Just(item)
        }
    }
    return Nothing
}

let words  = ["cat", "dog", "elephant", "ant"]
let long   = find_first(words, do(w) w.len() > 4)
let scores = [45, 72, 83, 91, 68]
let pass   = find_first(scores, do(s) s >= 90)

when long {
    Just(w) => println("First long word: " + w),
    Nothing => println("None found")
}
// First long word: elephant

when pass {
    Just(s) => println("First passing score: " + s.to_string()),
    Nothing => println("No passing score")
}
// First passing score: 91
```

## Event Callbacks and Handlers

Closures are natural for event-driven patterns where behavior is registered ahead of time and invoked later:

```tml
type Button {
    label: Str,
    on_click: func() -> Unit,
}

func make_button(label: Str, handler: func() -> Unit) -> Button {
    return Button {
        label: label,
        on_click: handler,
    }
}

var click_count = 0
let btn = make_button("Submit", do() {
    click_count = click_count + 1
    println("Button clicked " + click_count.to_string() + " time(s)")
})

btn.on_click()  // Button clicked 1 time(s)
btn.on_click()  // Button clicked 2 time(s)
```

## Named Functions vs. Closures as Arguments

Named functions can be passed wherever a closure is expected, as long as their signatures match:

```tml
func is_even(x: I32) -> Bool {
    return x % 2 == 0
}

let numbers = [1, 2, 3, 4, 5, 6]
let evens   = keep_if(numbers, is_even)  // pass by name, no `do` needed
```

Prefer named functions when:
- The logic is reusable across many call sites
- The function is complex enough to warrant a descriptive name and possible documentation
- You want to test the function independently

Prefer closures when:
- The logic is specific to one call site
- The function needs access to local variables
- The inline form reads more clearly than defining a separate named function

## Summary

| Pattern | Example |
|---|---|
| Accept closure | `func f(cb: func(I32) -> I32)` |
| Call closure | `cb(value)` |
| Return closure | `-> func(I32) -> I32 { return do(x) ... }` |
| Factory function | `make_adder(n)` returns `do(x) x + n` |
| Named function as argument | `keep_if(list, is_even)` |
| Struct with function field | `type T { f: func(I32) -> Bool }` |

Closures passed as arguments are a foundation for the iterator adapters covered in the next chapter, where method chaining replaces explicit loop writing entirely.
