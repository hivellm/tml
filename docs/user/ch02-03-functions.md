# Functions

Functions are the primary unit of reuse in TML. You have already seen `main`. This section
covers how to define your own functions, how parameters and return types work, and how
expression bodies and early returns interact.

## Defining a Function

Use the `func` keyword, followed by the function name, a parameter list in parentheses, an
optional return type, and a body in curly braces:

```tml
func greet() {
    println("Hello!")
}

func main() {
    greet()
}
```

Functions can be defined in any order within a file. TML resolves names across the file, so
calling a function before its definition is valid.

## Parameters

Each parameter has a name and a type, separated by a colon. Multiple parameters are separated
by commas:

```tml
func add(a: I32, b: I32) -> I32 {
    return a + b
}

func main() {
    let result = add(5, 3)
    println(result.to_string())  // 8
}
```

The parameter types are mandatory. TML does not infer parameter types.

## Return Values

Specify the return type with `->` after the parameter list:

```tml
func square(x: I32) -> I32 {
    return x * x
}
```

### Expression Body

The last expression in a function body is implicitly returned when no semicolon follows it and
no explicit `return` is written:

```tml
func square(x: I32) -> I32 {
    x * x   // returned implicitly
}
```

Both forms are valid. Use whichever reads more clearly. For short, single-expression functions,
the implicit form is common. For functions with multiple steps, `return` is often clearer.

### Early Return

`return` exits the function immediately with the given value:

```tml
func classify(n: I32) -> Str {
    if n < 0 {
        return "negative"
    }
    if n == 0 {
        return "zero"
    }
    return "positive"
}
```

### Functions That Return Nothing

A function that performs an action but produces no value has return type `Unit`. You can omit
the return type annotation entirely, and TML infers `Unit`:

```tml
func log_message(msg: Str) {
    println("[LOG] " + msg)
}

// Equivalent explicit annotation:
func log_message_explicit(msg: Str) -> Unit {
    println("[LOG] " + msg)
}
```

## Reference Parameters

By default, function arguments are passed by value — the function receives a copy. For types
that are expensive to copy, or when the function needs to observe the caller's value without
copying it, use a reference parameter:

```tml
func length(s: ref Str) -> U64 {
    return s.len()
}
```

`ref T` is a shared (immutable) reference. The function can read through the reference but
cannot modify the value it points to.

For a function that needs to modify the caller's value, use a mutable reference:

```tml
func append_exclamation(s: mut ref Str) {
    s.push_str("!")
}
```

`mut ref T` is a mutable reference. Only one mutable reference to a value may exist at a time;
the borrow checker enforces this. References and ownership are covered in depth in Chapter 8.

## Multiple Return Values

Return a tuple to hand back more than one value:

```tml
func min_max(a: I32, b: I32) -> (I32, I32) {
    if a < b {
        return (a, b)
    }
    return (b, a)
}

func main() {
    let result = min_max(7, 3)
    let lo = result.0   // 3
    let hi = result.1   // 7
    println("min: ${lo.to_string()}, max: ${hi.to_string()}")
}
```

## Examples

### Factorial

```tml
func factorial(n: I32) -> I32 {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

func main() {
    println(factorial(5).to_string())   // 120
    println(factorial(10).to_string())  // 3628800
}
```

### Fibonacci

```tml
func fibonacci(n: I32) -> I32 {
    if n <= 1 {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

func main() {
    loop i in 0 to 10 {
        println(fibonacci(i).to_string())
    }
}
```

## Closures

Anonymous functions are written with the `do` keyword:

```tml
func main() {
    let double = do(x: I32) -> I32 x * 2

    println(double(5).to_string())   // 10
    println(double(21).to_string())  // 42
}
```

The body can be a single expression (shown above) or a block:

```tml
let clamp = do(x: I32, lo: I32, hi: I32) -> I32 {
    if x < lo {
        return lo
    }
    if x > hi {
        return hi
    }
    x
}
```

Closures can capture variables from the surrounding scope:

```tml
func main() {
    let offset: I32 = 10
    let shift   = do(x: I32) -> I32 x + offset

    println(shift(5).to_string())   // 15
    println(shift(32).to_string())  // 42
}
```

Closures are covered in full in Chapter 9.

## Generic Functions

A function can operate on any type that satisfies certain constraints. Type parameters are
written in square brackets after the function name:

```tml
func identity[T](value: T) -> T {
    return value
}

func main() {
    let n = identity(42)        // T = I32
    let s = identity("hello")   // T = Str
    println(n.to_string())
    println(s)
}
```

Constraints on type parameters are written in a `where` clause:

```tml
func largest[T](a: T, b: T) -> T
where T: Ord
{
    if a > b { return a }
    return b
}
```

`Ord` is a behavior (TML's equivalent of a trait) that requires the type to support comparison
operators. Behaviors are covered in Chapter 5.

## Naming Conventions

- Function names use `snake_case`: `parse_header`, `count_words`
- Keep functions focused — each should do one thing
- Parameter names describe the role, not the type: `name` not `str_param`
- Document non-obvious functions with doc comments (covered in the next section)
