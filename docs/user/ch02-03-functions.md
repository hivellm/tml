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

## Function Types

Functions can be used as values in TML. Use `func(Args) -> Return` syntax for function types:

```tml
// Function type alias
type BinaryOp = func(I32, I32) -> I32

func apply_op(a: I32, b: I32, op: BinaryOp) -> I32 {
    return op(a, b)
}

func add(x: I32, y: I32) -> I32 {
    return x + y
}

func main() {
    let result = apply_op(5, 3, add)
    println(result)  // 8
}
```

### Common Function Type Patterns

```tml
// Predicate (returns Bool)
type Predicate[T] = func(T) -> Bool

// Mapper (transforms one type to another)
type Mapper[T, U] = func(T) -> U

// Comparator (compares two values)
type Comparator[T] = func(T, T) -> I32

// Callback (no return value)
type Callback = func() -> ()
```

## Closures (Anonymous Functions)

Use the `do` keyword to create anonymous functions (closures):

```tml
func main() {
    // Simple closure
    let double = do(x: I32) -> I32 x * 2

    println(double(5))  // 10
    println(double(7))  // 14
}
```

### Closure Syntax

Closures can have single-expression or block bodies:

```tml
// Single expression
let increment = do(x: I32) -> I32 x + 1

// Block body
let complex = do(x: I32) -> I32 {
    let doubled = x * 2
    let incremented = doubled + 1
    return incremented
}
```

### Using Closures with Higher-Order Functions

```tml
func filter[T](items: List[T], pred: func(T) -> Bool) -> List[T] {
    var result = List.new()
    loop item in items {
        if pred(item) {
            result.push(item)
        }
    }
    return result
}

func map[T, U](items: List[T], mapper: func(T) -> U) -> List[U] {
    var result = List.new()
    loop item in items {
        result.push(mapper(item))
    }
    return result
}

func main() {
    let numbers = [1, 2, 3, 4, 5, 6]

    // Filter even numbers
    let evens = filter(numbers, do(n: I32) -> Bool n % 2 == 0)
    println(evens)  // [2, 4, 6]

    // Map to doubles
    let doubled = map(numbers, do(n: I32) -> I32 n * 2)
    println(doubled)  // [2, 4, 6, 8, 10, 12]

    // Chain operations
    let result = map(
        filter(numbers, do(n: I32) -> Bool n > 2),
        do(n: I32) -> I32 n * n
    )
    println(result)  // [9, 16, 25, 36]
}
```

### Practical Closure Examples

```tml
// Sorting with custom comparator
func sort_by[T](items: mut ref List[T], cmp: func(T, T) -> I32) {
    // Sort using provided comparator
    items.sort_with(cmp)
}

func main() {
    var numbers = [5, 2, 8, 1, 9]

    // Sort ascending
    sort_by(mut ref numbers, do(a: I32, b: I32) -> I32 a - b)

    // Sort descending
    sort_by(mut ref numbers, do(a: I32, b: I32) -> I32 b - a)
}
```

```tml
// Event handlers
type EventHandler = func(I32, I32) -> ()

type Button {
    label: String,
    on_click: EventHandler,
}

func create_button(label: String, handler: EventHandler) -> Button {
    return Button {
        label: label,
        on_click: handler,
    }
}

func main() {
    let button = create_button(
        "Click me",
        do(x: I32, y: I32) -> () {
            println("Clicked at: ", x, ", ", y)
        }
    )

    // Simulate click
    button.on_click(100, 200)
}
```

## Generic Functions

Functions can be generic, accepting any type that satisfies certain constraints:

```tml
// Simple generic function
func identity[T](value: T) -> T {
    return value
}

func main() {
    let x = identity(42)       // T = I32
    let y = identity("hello")  // T = Str
    println(x)
    println(y)
}
```

### Where Clauses

Use `where` to constrain generic types:

```tml
// T must implement Add behavior
func double[T](value: T) -> T
where T: Add
{
    return value + value
}

// Multiple constraints
func compare_and_print[T](a: T, b: T) -> Bool
where T: Ord + Debug
{
    println("Comparing: {} and {}", a, b)
    return a > b
}

func main() {
    println(double(21))     // 42
    println(double(3.14))   // 6.28
}
```

### Generic Type Bounds

Common behavior bounds:

| Bound | Description |
|-------|-------------|
| `Add` | Supports `+` operator |
| `Sub` | Supports `-` operator |
| `Ord` | Supports comparison (`<`, `>`, etc.) |
| `Eq` | Supports equality (`==`, `!=`) |
| `Debug` | Can be printed for debugging |
| `Duplicate` | Can be cloned |
| `Default` | Has a default value |

## Best Practices

1. **Use descriptive names**: `calculate_area` is better than `ca`
2. **Keep functions small**: Each function should do one thing
3. **Use snake_case**: `my_function` not `myFunction`
4. **Document complex functions**: Explain what the function does
5. **Use closures for short operations**: For longer logic, define named functions
6. **Prefer function types for callbacks**: Makes APIs clearer
7. **Use `where` clauses for complex constraints**: Keeps function signatures readable
