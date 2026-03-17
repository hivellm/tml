# Control Flow

Control flow lets your program make decisions and repeat work. TML provides:

- `if` expressions for conditional branching
- The `loop` keyword as the unified construct for all loop styles
- The `when` expression for pattern matching

## `if` Expressions

The `if` expression evaluates a condition and runs one of two branches:

```tml
func main() {
    let temperature: I32 = 75

    if temperature > 100 {
        println("Boiling")
    } else if temperature > 0 {
        println("Liquid")
    } else {
        println("Frozen")
    }
}
```

### Conditions Must Be Boolean

TML requires the condition to be a `Bool`. Passing an integer or other type is a compile error:

```tml
let count: I32 = 5

// Error: expected Bool, found I32
// if count { ... }

// Correct: explicit comparison
if count > 0 {
    println("has items")
}
```

### `if` Is an Expression

In TML, `if` is an expression — it produces a value. Both branches must produce the same type:

```tml
func main() {
    let x: I32 = 10
    let label: Str = if x >= 0 { "non-negative" } else { "negative" }
    println(label)  // non-negative
}
```

For short, single-expression conditionals, use the `then` form:

```tml
let abs_x: I32 = if x >= 0 then x else -x

let sign: Str = if x < 0 then "negative"
                else if x > 0 then "positive"
                else "zero"
```

The `then` form and the block form are equivalent. Use whichever reads more clearly: `then` for
concise one-liners, blocks when the branches contain multiple statements.

### Ternary Conditional

TML also supports the `? :` ternary operator for simple inline conditions:

```tml
let max: I32 = a > b ? a : b
let label: Str = is_valid ? "PASS" : "FAIL"
```

The ternary operator and `if ... then ... else ...` are interchangeable for single-expression
conditions. Prefer `if ... then ... else ...` when the expression approaches the line length
limit, since it is easier to break across lines.

## Loops

TML uses a single `loop` keyword for every loop style. The variant is selected by what follows
`loop`.

### Counted Range Loop

Iterate over a range of integers with `loop i in start to end`:

```tml
func main() {
    loop i in 0 to 5 {
        println(i.to_string())
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

`to` produces an exclusive range: `0 to 5` iterates over 0, 1, 2, 3, 4. The upper bound is not
included.

For an inclusive upper bound, use `through`:

```tml
func main() {
    loop i in 1 through 5 {
        println(i.to_string())
    }
}
```

Output:

```
1
2
3
4
5
```

`through` produces an inclusive range: `1 through 5` iterates over 1, 2, 3, 4, 5.

### Iterating Over a Collection

Iterate over any collection — arrays, lists, slices — with `loop item in collection`:

```tml
func main() {
    let scores: [I32; 4] = [10, 20, 30, 40]
    loop score in scores {
        println(score.to_string())
    }
}
```

This works with any type that implements the `Iter` behavior. Standard library collections such
as `List[T]` and `HashMap[K, V]` all support this form.

### Conditional Loop

Run a block while a condition holds with `loop while condition`:

```tml
func main() {
    var count: I32 = 0
    loop while count < 5 {
        println(count.to_string())
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

The condition is evaluated before each iteration. When it becomes false, the loop stops.

### Infinite Loop

A bare `loop` with no qualifier runs forever until `break` is reached:

```tml
func main() {
    var attempts: I32 = 0
    loop {
        attempts = attempts + 1
        if attempts >= 3 {
            break
        }
        println("Trying...")
    }
    println("Done after ${attempts.to_string()} attempts.")
}
```

Infinite loops are common when polling for an event or implementing a server that processes
requests until shutdown.

### `break` and `continue`

`break` exits the nearest enclosing loop. `continue` skips to the next iteration:

```tml
func main() {
    loop i in 0 to 20 {
        if i % 2 == 0 {
            continue   // skip even numbers
        }
        if i > 10 {
            break      // stop after 10
        }
        println(i.to_string())
    }
}
```

Output:

```
1
3
5
7
9
```

### Nested Loops

`break` and `continue` apply to the nearest enclosing loop:

```tml
func main() {
    loop i in 0 to 3 {
        loop j in 0 to 3 {
            if j == 1 {
                continue   // continues the inner loop
            }
            println("${i.to_string()}, ${j.to_string()}")
        }
    }
}
```

## `when` — Pattern Matching

The `when` expression matches a value against a set of patterns and evaluates the corresponding
arm. It is TML's equivalent of `match` (Rust) or `switch` (C, Java).

### Basic Usage

```tml
func describe(n: I32) -> Str {
    return when n {
        0 => "zero",
        1 => "one",
        2 => "two",
        _ => "something else",
    }
}

func main() {
    println(describe(1))   // one
    println(describe(99))  // something else
}
```

The `_` wildcard matches any value that no earlier arm matched. Every `when` expression must be
exhaustive — the compiler rejects a `when` that can fall through without a match.

Arms are separated by commas. The last arm may include a trailing comma or omit it.

### `when` Is an Expression

Like `if`, `when` produces a value. All arms must have the same type:

```tml
let day_name: Str = when day_number {
    1 => "Monday",
    2 => "Tuesday",
    3 => "Wednesday",
    4 => "Thursday",
    5 => "Friday",
    6 => "Saturday",
    7 => "Sunday",
    _ => "invalid",
}
```

### Block Arms

When an arm needs multiple statements, use a block. The last expression in the block is the
value of the arm:

```tml
func process(n: I32) -> I32 {
    return when n {
        0 => {
            println("Got zero")
            0
        },
        _ => {
            let doubled = n * 2
            doubled + 1
        },
    }
}
```

### Range Patterns

Match a range of values using `to` (exclusive) or `through` (inclusive):

```tml
func grade(score: I32) -> Str {
    return when score {
        90 through 100 => "A",
        80 through 89  => "B",
        70 through 79  => "C",
        60 through 69  => "D",
        _              => "F",
    }
}
```

### Enum Patterns

`when` is the primary way to work with enum values. The standard library types `Maybe[T]` and
`Outcome[T, E]` are enums, and `when` destructures them naturally:

```tml
func describe_maybe(m: Maybe[I32]) -> Str {
    return when m {
        Just(value) => "has value: ${value.to_string()}",
        Nothing     => "empty",
    }
}

func handle_result(r: Outcome[I32, Str]) -> I32 {
    return when r {
        Ok(value) => value,
        Err(msg)  => {
            println("Error: ${msg}")
            -1
        },
    }
}
```

`Maybe` and `Outcome` are introduced in Chapter 7. The key point for now: `when` is how you
examine which variant an enum holds and extract the data inside it.

### Compared to Traditional `switch`

| `switch` (C/Java) | `when` (TML) |
|-------------------|--------------|
| `case 1: { ... break; }` | `1 => { ... },` |
| Falls through by default | No fall-through |
| `default:` | `_ =>` |
| Statement only | Expression — returns a value |
| Matches literals only | Matches literals, ranges, enums, structs |

`when` is safer than `switch` (no accidental fall-through), more expressive (pattern matching),
and cleaner (no `break` statements).

## Combining Control Flow

Here is a complete example that uses all of the constructs introduced in this section:

```tml
func fizzbuzz(limit: I32) {
    loop i in 1 through limit {
        let label: Str = if i % 15 == 0 then "FizzBuzz"
                         else if i % 3 == 0 then "Fizz"
                         else if i % 5 == 0 then "Buzz"
                         else i.to_string()
        println(label)
    }
}

func main() {
    fizzbuzz(20)
}
```

Output:

```
1
2
Fizz
4
Buzz
Fizz
7
8
Fizz
Buzz
11
Fizz
13
14
FizzBuzz
16
17
Fizz
19
Buzz
```
