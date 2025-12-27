# Control Flow

Control flow lets you run code conditionally and repeat code. TML provides
`if` expressions and several loop constructs.

## `if` Expressions

The `if` expression lets you branch your code:

```tml
func main() {
    let number = 7

    if number < 5 {
        println("less than 5")
    } else {
        println("5 or greater")
    }
}
```

### Condition Must Be Boolean

Unlike some languages, TML requires the condition to be a `Bool`:

```tml
func main() {
    let number = 3

    // Error: number is I32, not Bool
    // if number { ... }

    // Correct: explicit comparison
    if number != 0 {
        println("number is not zero")
    }
}
```

### `else if` Chains

Handle multiple conditions with `else if`:

```tml
func main() {
    let number = 6

    if number % 4 == 0 {
        println("divisible by 4")
    } else if number % 3 == 0 {
        println("divisible by 3")
    } else if number % 2 == 0 {
        println("divisible by 2")
    } else {
        println("not divisible by 4, 3, or 2")
    }
}
```

### `if` as an Expression

In TML, `if` is an expression that returns a value:

```tml
func main() {
    let condition = true
    let number = if condition { 5 } else { 6 }
    println(number)  // 5
}
```

Both branches must return the same type.

### Two `if` Syntaxes: Block Form and Expression Form

TML supports two syntaxes for `if` expressions:

**Block form** (with braces):
```tml
func max(a: I32, b: I32) -> I32 {
    let result = if a > b {
        a
    } else {
        b
    }
    return result
}
```

**Expression form** (with `then` keyword):
```tml
func max(a: I32, b: I32) -> I32 {
    return if a > b then a else b
}

func abs(x: I32) -> I32 {
    return if x < 0 then -x else x
}

func sign(x: I32) -> String {
    return if x < 0 then
        "negative"
    else if x > 0 then
        "positive"
    else
        "zero"
}
```

Both syntaxes work identically—choose based on readability.

### Ternary Conditional Operator

For simple conditional expressions, TML provides a ternary operator `? :`:

```tml
func main() {
    let x = 10
    let y = 20

    // Find maximum using ternary
    let max = x > y ? x : y
    println(max)  // 20

    // Find minimum
    let min = x < y ? x : y
    println(min)  // 10
}
```

The ternary operator is an expression that evaluates to a value. Both branches must return the same type.

**Nested ternary:**
```tml
func max_of_three(a: I32, b: I32, c: I32) -> I32 {
    return a > b ? (a > c ? a : c) : (b > c ? b : c)
}
```

**When to use ternary vs if:**
- Use ternary for simple, inline conditions
- Use if for complex logic or multiple statements

```tml
// Ternary (concise, one-liner)
let status = is_valid ? "PASS" : "FAIL"

// If-then-else (more explicit)
let status = if is_valid then "PASS" else "FAIL"

// If block (for complex logic)
let status = if is_valid {
    log("Validation passed")
    "PASS"
} else {
    log("Validation failed")
    "FAIL"
}
```

### `if let` Pattern Matching

Use `if let` to conditionally unwrap `Maybe` and `Outcome` values:

```tml
func get_first_item(items: List[I32]) -> String {
    if let Just(first) = items.get(0) {
        return "First item: " + first.to_string()
    } else {
        return "List is empty"
    }
}

func load_config() -> Config {
    let result = Config.load()

    if let Ok(config) = result {
        return config
    } else {
        return Config.default()
    }
}
```

You can also handle errors with binding:

```tml
func try_parse(input: String) -> I32 {
    let result = input.parse_i32()

    if let Err(error) = result {
        println("Parse failed: " + error.message)
        return 0
    }

    if let Ok(value) = result {
        return value
    }

    return 0
}
```

`if let` works with any pattern, including nested patterns:

```tml
func process_nested(data: Maybe[Outcome[String, Error]]) -> String {
    if let Just(result) = data {
        if let Ok(value) = result {
            return value
        }
    }
    return "failed"
}
```

## Loops

TML provides a unified `loop` construct with different patterns.

### Infinite Loop

The basic `loop` runs forever until you `break`:

```tml
func main() {
    let mut count = 0
    loop {
        println(count)
        count = count + 1
        if count >= 5 {
            break
        }
    }
}
```

Output:
```
0
1
2
3
4
```

### `while` Loop

TML provides a traditional `while` loop that runs as long as a condition is true:

```tml
func main() {
    let mut count = 0
    while count < 5 {
        println(count)
        count = count + 1
    }
}
```

Output:
```
0
1
2
3
4
```

The `while` loop checks the condition before each iteration. You can also use `break` and `continue` inside while loops:

```tml
func main() {
    let mut i = 0
    while i < 10 {
        i = i + 1
        if i % 2 == 0 {
            continue  // Skip even numbers
        }
        println(i)  // Only odd numbers
    }
}
```

### `for` Loop with Ranges

Use `for` to iterate over ranges:

```tml
func main() {
    // 0 to 4 (exclusive end)
    for i in 0 to 5 {
        println(i)
    }
}
```

Use `through` for inclusive end:

```tml
func main() {
    // 1 through 5 (inclusive)
    for i in 1 through 5 {
        println(i)
    }
}
```

### `for` Loop with Collections

You can iterate directly over collections like `List`, `Vec`, `HashMap`, and `Buffer`:

```tml
func main() {
    let list = list_create(5)
    list_push(list, 10)
    list_push(list, 20)
    list_push(list, 30)

    // Iterate over list elements
    for item in list {
        println(item)
    }

    list_destroy(list)
}
```

Output:
```
10
20
30
```

This works with any collection type:

```tml
func sum_list(numbers: List) -> I32 {
    let mut total = 0
    for num in numbers {
        total = total + num
    }
    return total
}

func main() {
    let list = list_create(3)
    list_push(list, 5)
    list_push(list, 10)
    list_push(list, 15)

    let result = sum_list(list)
    println("Sum: ", result)  // Sum: 30

    list_destroy(list)
}
```

**Note:** Collection iteration is currently experimental. For HashMap, it iterates over values. Future versions will support key-value pair iteration.

### `continue`

Skip to the next iteration with `continue`:

```tml
func main() {
    for i in 0 to 10 {
        if i % 2 == 0 {
            continue  // Skip even numbers
        }
        println(i)  // Only odd numbers
    }
}
```

### Nested Loops

You can nest loops and break out of them:

```tml
func main() {
    let mut i = 0
    loop {
        if i >= 3 {
            break
        }
        let mut j = 0
        loop {
            if j >= 3 {
                break
            }
            println(i, ", ", j)
            j = j + 1
        }
        i = i + 1
    }
}
```

## Example: FizzBuzz

A classic example combining control flow concepts:

```tml
func main() {
    for i in 1 through 20 {
        if i % 15 == 0 {
            println("FizzBuzz")
        } else if i % 3 == 0 {
            println("Fizz")
        } else if i % 5 == 0 {
            println("Buzz")
        } else {
            println(i)
        }
    }
}
```

## Example: Sum of Numbers

```tml
func main() {
    let mut sum = 0
    for i in 1 through 10 {
        sum = sum + i
    }
    println("Sum: ", sum)  // Sum: 55
}
```
