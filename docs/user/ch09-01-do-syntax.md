# The Do Syntax

TML uses the `do` keyword to create closures. The syntax covers a range of forms: simple one-liners, multi-parameter closures, closures with explicit return types, and closures with full block bodies.

## Basic Form

The simplest closure takes a parameter list and a single expression:

```tml
let square = do(x: I32) x * x
println(square(9).to_string())  // 81
```

The expression after the parameter list is the return value. No `return` keyword is needed for this form.

## Explicit Return Type

You can annotate the return type after `->`:

```tml
let clamp = do(x: I32, lo: I32, hi: I32) -> I32 {
    if x < lo { return lo }
    if x > hi { return hi }
    return x
}

println(clamp(15, 0, 10).to_string())  // 10
println(clamp(-5, 0, 10).to_string())  // 0
println(clamp(7, 0, 10).to_string())   // 7
```

The return type annotation is optional when the compiler can infer it from context, but it can improve readability for complex closures.

## Block Bodies

When a closure requires multiple statements, use a block body enclosed in `{ }`:

```tml
let describe = do(n: I32) -> Str {
    if n < 0 {
        return "negative"
    }
    if n == 0 {
        return "zero"
    }
    return "positive"
}

println(describe(-3))  // negative
println(describe(0))   // zero
println(describe(7))   // positive
```

The last expression in a block body is returned implicitly if no explicit `return` is present:

```tml
let compute = do(x: I32) -> I32 {
    let doubled = x * 2
    let shifted = doubled + 1
    shifted  // implicitly returned
}

println(compute(5).to_string())  // 11
```

## Multiple Parameters

Closures accept any number of parameters, separated by commas:

```tml
let add = do(a: I32, b: I32) a + b
let mul = do(a: I32, b: I32) a * b

println(add(3, 4).to_string())  // 7
println(mul(3, 4).to_string())  // 12
```

## No Parameters

A closure that takes no arguments uses an empty parameter list:

```tml
let greet = do() "Hello, World!"
println(greet())  // Hello, World!
```

## Type Inference in Context

When a closure is passed directly to a function, the compiler infers parameter types from the function's signature. You can omit the type annotations:

```tml
let numbers = [1, 2, 3, 4, 5]

// Types inferred from numbers: [I32] — no annotations needed
let doubled = numbers.map(do(x) x * 2)
let evens   = numbers.filter(do(x) x % 2 == 0)
```

Type inference works in both directions. If the closure's body makes the return type clear, the `->` annotation can be omitted even when the parameter types are written explicitly.

## Capturing Variables from the Environment

Closures can read variables defined in the surrounding scope. This is called *capturing*:

```tml
let factor = 3
let multiply = do(x: I32) x * factor

println(multiply(7).to_string())   // 21
println(multiply(10).to_string())  // 30
```

The closure holds a reference to `factor`. If `factor` is a `var`, the closure reads its current value at the time of the call:

```tml
var base = 100
let add_base = do(x: I32) x + base

println(add_base(5).to_string())   // 105
base = 200
println(add_base(5).to_string())   // 205
```

### Mutable Capture

A closure can mutate a captured `var` variable:

```tml
var count = 0
let increment = do() {
    count = count + 1
}

increment()
increment()
increment()
println(count.to_string())  // 3
```

Each call to `increment` modifies the same `count` variable in the enclosing scope. This is only allowed when the variable is declared with `var`.

### Move Capture

By default, closures capture variables by reference. To transfer ownership of a value into the closure, prefix the closure with `move`:

```tml
let message = "important data"
let show = move do() {
    println(message)
}

// message has been moved into show — it is no longer accessible here
show()  // important data
```

Move closures are useful when a closure needs to outlive the scope in which it was created, for example when passing it to a thread or storing it for later use. After a move, the original variable is consumed and cannot be used.

## Closures as Variables

Closures assigned to variables have function pointer types when they do not capture anything:

```tml
// Non-capturing: compatible with func(I32) -> I32
let double: func(I32) -> I32 = do(x) x * 2
```

Capturing closures have a closure type managed by the compiler. The type can be annotated explicitly or inferred:

```tml
let offset = 42
let shift: func(I32) -> I32 = do(x) x + offset
```

## Stored Closures in Structs

Structs can store non-capturing closures as function-typed fields. The function pointer type is used:

```tml
type Processor {
    transform: func(I32) -> I32,
}

let p = Processor {
    transform: do(x) x * x,
}
println(p.transform(6).to_string())  // 36
```

Capturing closures cannot be stored directly in struct fields. If the closure needs access to external state, pass that state as an explicit field alongside the function pointer, or restructure the design to pass the closure as a parameter at call time.

## Closures and the `func` Type

The `func(Params) -> Return` type notation describes both named functions and non-capturing closures. They are interchangeable anywhere a `func` type is expected:

```tml
func apply(f: func(I32) -> I32, x: I32) -> I32 {
    return f(x)
}

func triple(x: I32) -> I32 {
    return x * 3
}

// Named function passed by reference
println(apply(triple, 7).to_string())  // 21

// Closure passed inline
println(apply(do(x) x + 100, 7).to_string())  // 107
```

## Summary

| Feature | Syntax |
|---|---|
| Expression body | `do(x: I32) x * 2` |
| Block body | `do(x: I32) -> I32 { ... }` |
| No parameters | `do() expr` |
| Multiple parameters | `do(a: I32, b: I32) expr` |
| Inferred types | `do(x) x + 1` (in context) |
| Move capture | `move do(x) ...` |
| Mutable capture | reads/writes `var` in scope |

The `do` keyword is the single entry point for all closure forms in TML. Its consistent shape — `do(params) body` — keeps closures readable regardless of their complexity.
