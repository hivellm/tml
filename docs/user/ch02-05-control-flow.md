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

### `while`-style Loop

Use `loop` with an `if`/`break` at the start:

```tml
func main() {
    let mut count = 0
    loop {
        if count >= 5 {
            break
        }
        println(count)
        count = count + 1
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
