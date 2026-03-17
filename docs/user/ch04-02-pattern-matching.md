# Pattern Matching with When

The `when` expression compares a value against a series of *patterns* and executes the code
associated with the first matching pattern. Unlike a `switch` statement in C or Java, `when`
is an expression (it produces a value), requires no `break` statements, and the compiler
checks that all cases are covered.

## Basic Syntax

```tml
when value {
    pattern1 => expression1,
    pattern2 => expression2,
    _        => fallback_expression,
}
```

Each line is called an *arm*. Arms are separated by commas. The `_` wildcard matches anything
that has not already been matched.

A minimal example:

```tml
func classify(n: I32) -> Str {
    when n {
        0 => "zero",
        1 => "one",
        2 => "two",
        _ => "other",
    }
}
```

`when` is an expression, so it can appear anywhere a value is expected:

```tml
let label = when score {
    90 through 100 => "A",
    80 through 89  => "B",
    70 through 79  => "C",
    60 through 69  => "D",
    _              => "F",
}
```

## Literal Patterns

Literal patterns match exact values. They work with integers, floats, booleans, and character
literals:

```tml
func http_status(code: I32) -> Str {
    when code {
        200 => "OK",
        201 => "Created",
        400 => "Bad Request",
        401 => "Unauthorized",
        403 => "Forbidden",
        404 => "Not Found",
        500 => "Internal Server Error",
        _   => "Unknown",
    }
}
```

```tml
func describe_bool(b: Bool) -> Str {
    when b {
        true  => "yes",
        false => "no",
    }
}
```

## The Wildcard Pattern

The `_` pattern matches any value and discards it. It is typically the last arm, acting as a
default case:

```tml
func is_weekend(day: I32) -> Bool {
    when day {
        0 => true,   // Sunday
        6 => true,   // Saturday
        _ => false,
    }
}
```

You can also use a named wildcard when you want to give the fallback a descriptive name
without binding it:

```tml
when result {
    Ok(data) => process(data),
    Err(_)   => println("an error occurred"),
}
```

## Range Patterns

Match a contiguous range of values with `to` (exclusive upper bound) or `through` (inclusive
upper bound):

```tml
func grade(score: I32) -> Str {
    when score {
        90 through 100 => "A",
        80 through 89  => "B",
        70 through 79  => "C",
        60 through 69  => "D",
        0 through 59   => "F",
        _              => "invalid",
    }
}
```

Range patterns also work with character literals:

```tml
func char_kind(c: Char) -> Str {
    when c {
        'a' through 'z' => "lowercase letter",
        'A' through 'Z' => "uppercase letter",
        '0' through '9' => "digit",
        _               => "other",
    }
}
```

## Enum Patterns

Matching an enum is the most common use of `when`. Each arm names a variant:

```tml
type Direction = North | South | East | West

func opposite(d: Direction) -> Direction {
    when d {
        Direction.North => Direction.South,
        Direction.South => Direction.North,
        Direction.East  => Direction.West,
        Direction.West  => Direction.East,
    }
}
```

When the enum type is unambiguous from context, the type prefix can be omitted:

```tml
func opposite(d: Direction) -> Direction {
    when d {
        North => South,
        South => North,
        East  => West,
        West  => East,
    }
}
```

## Destructuring Enum Variants with Data

When a variant carries a payload, the pattern extracts that data into named bindings:

### Struct-variant destructuring

```tml
type Shape =
    | Circle { radius: F64 }
    | Rectangle { width: F64, height: F64 }
    | Triangle { base: F64, height: F64 }

func area(s: Shape) -> F64 {
    when s {
        Circle { radius }           => 3.14159 * radius * radius,
        Rectangle { width, height } => width * height,
        Triangle { base, height }   => 0.5 * base * height,
    }
}
```

The field names from the variant definition become local bindings in the arm body. You can
rename a binding with `field: new_name`:

```tml
when s {
    Circle { radius: r } => r * r * 3.14159,
    // ...
}
```

To ignore a specific field, use `_`:

```tml
when s {
    Rectangle { width, height: _ } => width,
    // ...
}
```

### Tuple-variant destructuring

```tml
type Message =
    | Text(Str)
    | Number(I32)
    | Coordinate(F64, F64)
    | Empty

func describe(m: Message) -> Str {
    when m {
        Text(s)          => "text: " + s,
        Number(n)        => "number: " + n.to_string(),
        Coordinate(x, y) => "(" + x.to_string() + ", " + y.to_string() + ")",
        Empty            => "nothing",
    }
}
```

Positional bindings are named freely. Use `_` to discard a position you do not need:

```tml
when m {
    Coordinate(x, _) => "x=" + x.to_string(),
    // ...
}
```

## Matching Maybe and Outcome

`Maybe[T]` and `Outcome[T, E]` are generic enums from the standard library. Match them
directly with `when`:

```tml
func unwrap_or(value: Maybe[I32], default: I32) -> I32 {
    when value {
        Just(v) => v,
        Nothing => default,
    }
}
```

```tml
func handle(result: Outcome[Str, Str]) {
    when result {
        Ok(data)  => println("Success: " + data),
        Err(msg)  => println("Error: " + msg),
    }
}
```

A common pattern is chaining operations that return `Maybe` or `Outcome`:

```tml
func load_and_parse(path: Str) -> Outcome[Config, Str] {
    let text = when read_file(path) {
        Ok(t)    => t,
        Err(msg) => return Err("read failed: " + msg),
    }

    when parse_config(text) {
        Ok(cfg)  => Ok(cfg),
        Err(msg) => Err("parse failed: " + msg),
    }
}
```

## Block Bodies

When an arm needs more than one statement, use a block. The last expression in the block
becomes the arm's value:

```tml
func process(n: I32) -> Str {
    when n {
        0 => "zero",
        1 => {
            let doubled = n * 2
            "one doubled is " + doubled.to_string()
        },
        _ => {
            let square = n * n
            let label = if square > 100 { "large" } else { "small" }
            label + " square: " + square.to_string()
        },
    }
}
```

## Guard Conditions

An arm can include a boolean guard with the `if` keyword placed after the pattern. The arm
matches only when both the pattern and the guard are true:

```tml
func classify_number(n: I32) -> Str {
    when n {
        x if x < 0   => "negative",
        0             => "zero",
        x if x % 2 == 0 => "positive even",
        _             => "positive odd",
    }
}
```

Guards can reference bindings introduced by the pattern:

```tml
func safe_divide(a: I32, b: I32) -> Maybe[I32] {
    when b {
        0    => Nothing,
        b if b < 0 => Just(-(a / -b)),
        _    => Just(a / b),
    }
}
```

## Struct Destructuring

`when` can destructure structs as well as enums:

```tml
type Point {
    x: F64,
    y: F64,
}

func quadrant(p: Point) -> Str {
    when p {
        Point { x, y } if x > 0.0 and y > 0.0 => "Q1",
        Point { x, y } if x < 0.0 and y > 0.0 => "Q2",
        Point { x, y } if x < 0.0 and y < 0.0 => "Q3",
        Point { x, y } if x > 0.0 and y < 0.0 => "Q4",
        _                                       => "on axis",
    }
}
```

If you only need some fields, list just those. The others are silently ignored:

```tml
func x_position(p: Point) -> F64 {
    when p {
        Point { x } => x,
    }
}
```

## Nested Patterns

Patterns can be nested to match deeply structured values:

```tml
type Inner = A | B
type Outer = Wrap(Inner) | Direct

func describe(o: Outer) -> Str {
    when o {
        Wrap(A)  => "wrapped A",
        Wrap(B)  => "wrapped B",
        Direct   => "direct",
    }
}
```

A common case is matching nested `Maybe` or `Outcome`:

```tml
func process(data: Maybe[Outcome[I32, Str]]) -> Str {
    when data {
        Just(Ok(n))  => "got " + n.to_string(),
        Just(Err(e)) => "error: " + e,
        Nothing      => "absent",
    }
}
```

## `if let` for Single-Variant Checks

When you only care about one variant and want to take action only in that case, `if let`
is more concise than a full `when`:

```tml
func print_if_present(value: Maybe[Str]) {
    if let Just(s) = value {
        println("Found: " + s)
    }
}
```

`if let` works with `else` to handle the non-matching case:

```tml
func get_or_default(value: Maybe[I32]) -> I32 {
    if let Just(n) = value {
        return n
    } else {
        return 0
    }
}
```

It also works with `Outcome`:

```tml
func try_connect(addr: Str) {
    if let Ok(conn) = open_connection(addr) {
        conn.send("hello")
    } else {
        println("could not connect to " + addr)
    }
}
```

`if let` accepts any pattern — including struct patterns and guards:

```tml
if let Point { x, y } = current_position() {
    if x > 0.0 {
        println("on the right side")
    }
}
```

## Exhaustiveness

The compiler verifies that every possible value is handled by at least one arm. If you omit
a variant, you get a compile error:

```tml
type Color = Red | Green | Blue

// ERROR: non-exhaustive — Green is not covered
func name(c: Color) -> Str {
    when c {
        Red  => "red",
        Blue => "blue",
    }
}
```

To handle all remaining cases without listing them individually, use the wildcard `_`:

```tml
func name(c: Color) -> Str {
    when c {
        Red => "red",
        _   => "not red",
    }
}
```

Exhaustiveness checking is one of the primary benefits of `when` over chains of `if`/`else`:
it is impossible to silently forget a case.

## Comparison with Traditional Switch

| Feature | C/Java `switch` | TML `when` |
|---------|-----------------|------------|
| Return a value | No (statement) | Yes (expression) |
| Fall-through | Yes (default) | No |
| `break` required | Yes | No |
| Default case | `default:` | `_ =>` |
| Destructuring | No | Yes |
| Range patterns | No | Yes |
| Guard conditions | No | Yes |
| Exhaustiveness check | No | Yes |

TML's `when` is strictly more expressive and safer than a traditional `switch`. The
exhaustiveness guarantee, in particular, eliminates an entire class of bugs that appear when
new variants or states are added to a type.

## Practical Example: Command Dispatch

The following example combines several pattern matching features to implement a simple
command dispatcher:

```tml
type Command =
    | Quit
    | Move { x: I32, y: I32 }
    | Write(Str)
    | ChangeColor(U8, U8, U8)

type AppState {
    x: I32,
    y: I32,
    running: Bool,
}

func apply(state: AppState, cmd: Command) -> AppState {
    when cmd {
        Quit => {
            AppState { running: false, ..state }
        },
        Move { x, y } => {
            AppState { x: state.x + x, y: state.y + y, ..state }
        },
        Write(text) => {
            println(text)
            state
        },
        ChangeColor(r, g, b) => {
            println("color: " + r.to_string() + "," + g.to_string() + "," + b.to_string())
            state
        },
    }
}

func main() {
    var state = AppState { x: 0, y: 0, running: true }

    let commands = [
        Command.Move { x: 10, y: 5 },
        Command.Write("hello"),
        Command.ChangeColor(255, 128, 0),
        Command.Move { x: -3, y: 2 },
    ]

    for cmd in commands {
        state = apply(state, cmd)
    }

    println("Final position: " + state.x.to_string() + ", " + state.y.to_string())
    // Final position: 7, 7
}
```

Each `Command` variant carries different data, and the `when` expression handles all four
cases. Adding a new command variant would require updating `apply` — and the compiler would
immediately flag every `when` that does not cover the new case.
