# Functions

Functions are the building blocks of readable, reusable code. You've
already seen one function: `main`. Let's learn how to define your own.

## Defining Functions

Use the `func` keyword to define a function:

```tml
func main() {
    println("Hello from main!")
    greet()
}

func greet() {
    println("Hello!")
}
```

Functions can be defined in any order. TML will find them.

## Parameters

Functions can take parameters:

```tml
func main() {
    greet("Alice")
    greet("Bob")
}

func greet(name: Str) {
    println("Hello, ", name, "!")
}
```

Multiple parameters are separated by commas:

```tml
func main() {
    let result = add(5, 3)
    println(result)  // 8
}

func add(a: I32, b: I32) -> I32 {
    return a + b
}
```

## Return Values

Use `->` to specify the return type:

```tml
func add(a: I32, b: I32) -> I32 {
    return a + b
}
```

### Implicit Returns

The last expression in a function is implicitly returned:

```tml
func add(a: I32, b: I32) -> I32 {
    a + b  // No semicolon = return value
}
```

### Early Returns

Use `return` to exit early:

```tml
func check(x: I32) -> Str {
    if x < 0 {
        return "negative"
    }
    if x == 0 {
        return "zero"
    }
    return "positive"
}
```

## Functions Returning Nothing

Functions that don't return a value have return type `Unit` (implied):

```tml
func say_hello() {
    println("Hello!")
    // Implicitly returns Unit
}

func say_hello_explicit() -> Unit {
    println("Hello!")
}
```

## Function Examples

### Factorial

```tml
func factorial(n: I32) -> I32 {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

func main() {
    println(factorial(5))  // 120
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
    let i = 0
    loop {
        if i >= 10 {
            break
        }
        println(fibonacci(i))
        i = i + 1
    }
}
```

## Best Practices

1. **Use descriptive names**: `calculate_area` is better than `ca`
2. **Keep functions small**: Each function should do one thing
3. **Use snake_case**: `my_function` not `myFunction`
4. **Document complex functions**: Explain what the function does
