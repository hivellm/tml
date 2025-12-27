# RFC-0004: Error Handling

## Status
Partial (v0.4.0)

> `Maybe[T]` and `Outcome[T, E]` types implemented with pattern matching. The `!` propagation operator pending.

## Summary

This RFC defines TML's error handling: the `Outcome[T, E]` type, the `!` propagation operator, the `Maybe[T]` type, and patterns for error management.

## Motivation

Error handling must be:
1. **Explicit** - No hidden control flow (unlike exceptions)
2. **Ergonomic** - Not verbose (unlike manual checking)
3. **Composable** - Errors transform and chain naturally
4. **LLM-friendly** - Clear patterns for code generation

---

## 1. Core Types

### 1.1 Outcome[T, E]

The primary result type for fallible operations.

```tml
type Outcome[T, E] = Ok(T) | Err(E)
```

**Constructors:**
```tml
let success: Outcome[I32, String] = Ok(42)
let failure: Outcome[I32, String] = Err("something went wrong")
```

### 1.2 Maybe[T]

For optional values (no error information needed).

```tml
type Maybe[T] = Just(T) | Nothing
```

**Constructors:**
```tml
let some: Maybe[I32] = Just(42)
let none: Maybe[I32] = Nothing
```

### 1.3 Type Relationships

```tml
// Maybe is Outcome with Unit error
Maybe[T] ≡ Outcome[T, Unit]

// Conversion
maybe.ok_or(error)      // Maybe[T] -> Outcome[T, E]
outcome.ok()            // Outcome[T, E] -> Maybe[T]
```

---

## 2. The ! Operator

The `!` operator (propagate) unwraps success or returns early with error.

### 2.1 Basic Usage

```tml
func read_config() -> Outcome[Config, IoError] {
    let content = File.read("config.json")!  // Propagate on error
    let config = Json.parse(content)!         // Propagate on error
    Ok(config)
}
```

### 2.2 Semantics

For `expr!` where `expr: Outcome[T, E]`:

```tml
// Desugars to:
when expr {
    Ok(value) -> value,           // Continue with unwrapped value
    Err(e) -> return Err(e.into()), // Early return, converting error
}
```

### 2.3 In Non-Outcome Functions

When the enclosing function doesn't return `Outcome`, `!` panics:

```tml
func must_read_config() -> Config {
    let content = File.read("config.json")!  // Panics on error!
    Json.parse(content)!
}

// Desugars to:
func must_read_config() -> Config {
    let content = when File.read("config.json") {
        Ok(v) -> v,
        Err(e) -> panic!("unwrap failed: {e}"),
    }
    when Json.parse(content) {
        Ok(v) -> v,
        Err(e) -> panic!("unwrap failed: {e}"),
    }
}
```

### 2.4 With Maybe

The `!` operator also works with `Maybe[T]`:

```tml
func get_user_email(id: U64) -> Outcome[String, Error] {
    let user = find_user(id)!  // Nothing becomes Err
    Ok(user.email)
}
```

When `!` is applied to `Maybe[T]` in an `Outcome`-returning function:
- `Just(v)` → unwraps to `v`
- `Nothing` → returns `Err(Error::NothingValue)`

---

## 3. Error Conversion

### 3.1 The Into Behavior

Errors automatically convert via the `Into` behavior:

```tml
behavior Into[T] {
    func into(this: This) -> T
}

// Automatic conversion in ! operator
impl Into[AppError] for IoError {
    func into(this: This) -> AppError {
        AppError::Io(this)
    }
}
```

### 3.2 Error Wrapping

```tml
type AppError =
    | Io(IoError)
    | Parse(ParseError)
    | Validation(String)

func load_data() -> Outcome[Data, AppError] {
    let content = File.read("data.json")!  // IoError -> AppError::Io
    let parsed = Json.parse(content)!       // ParseError -> AppError::Parse
    Ok(parsed)
}
```

### 3.3 Custom Conversion with else

```tml
func get_user(id: U64) -> Outcome[User, Error] {
    let user = find_user(id) else return Err(Error::NotFound(id))
    Ok(user)
}

// Or inline:
let user = find_user(id) else Err(Error::NotFound(id))!
```

---

## 4. Combinators

### 4.1 Outcome Methods

```tml
impl Outcome[T, E] {
    // Check state
    func is_ok(this: ref This) -> Bool
    func is_err(this: ref This) -> Bool

    // Unwrap (panic if wrong variant)
    func unwrap(this: This) -> T
    func unwrap_err(this: This) -> E
    func expect(this: This, msg: String) -> T
    func expect_err(this: This, msg: String) -> E

    // Safe unwrap with default
    func unwrap_or(this: This, default: T) -> T
    func unwrap_or_else(this: This, f: func() -> T) -> T
    func unwrap_or_default(this: This) -> T where T: Default

    // Transform success
    func map[U](this: This, f: func(T) -> U) -> Outcome[U, E]
    func and_then[U](this: This, f: func(T) -> Outcome[U, E]) -> Outcome[U, E]

    // Transform error
    func map_err[F](this: This, f: func(E) -> F) -> Outcome[T, F]
    func or_else[F](this: This, f: func(E) -> Outcome[T, F]) -> Outcome[T, F]

    // Combine
    func and[U](this: This, other: Outcome[U, E]) -> Outcome[U, E]
    func or(this: This, other: Outcome[T, E]) -> Outcome[T, E]

    // Convert
    func ok(this: This) -> Maybe[T]
    func err(this: This) -> Maybe[E]
}
```

### 4.2 Maybe Methods

```tml
impl Maybe[T] {
    // Check state
    func is_just(this: ref This) -> Bool
    func is_nothing(this: ref This) -> Bool

    // Unwrap
    func unwrap(this: This) -> T
    func expect(this: This, msg: String) -> T
    func unwrap_or(this: This, default: T) -> T
    func unwrap_or_else(this: This, f: func() -> T) -> T
    func unwrap_or_default(this: This) -> T where T: Default

    // Transform
    func map[U](this: This, f: func(T) -> U) -> Maybe[U]
    func and_then[U](this: This, f: func(T) -> Maybe[U]) -> Maybe[U]
    func filter(this: This, predicate: func(ref T) -> Bool) -> Maybe[T]

    // Combine
    func and[U](this: This, other: Maybe[U]) -> Maybe[U]
    func or(this: This, other: Maybe[T]) -> Maybe[T]
    func xor(this: This, other: Maybe[T]) -> Maybe[T]

    // Convert
    func ok_or[E](this: This, error: E) -> Outcome[T, E]
    func ok_or_else[E](this: This, f: func() -> E) -> Outcome[T, E]
}
```

---

## 5. Pattern Matching

### 5.1 Basic Patterns

```tml
func handle_result(result: Outcome[I32, String]) {
    when result {
        Ok(value) -> println("Got: {value}"),
        Err(msg) -> println("Error: {msg}"),
    }
}
```

### 5.2 Nested Patterns

```tml
func process(result: Outcome[Maybe[User], Error]) {
    when result {
        Ok(Just(user)) -> greet(user),
        Ok(Nothing) -> println("No user found"),
        Err(e) -> println("Error: {e}"),
    }
}
```

### 5.3 Guard Patterns

```tml
when result {
    Ok(n) if n > 0 -> println("Positive: {n}"),
    Ok(n) if n < 0 -> println("Negative: {n}"),
    Ok(_) -> println("Zero"),
    Err(e) -> println("Error: {e}"),
}
```

---

## 6. Error Types

### 6.1 Standard Error Behavior

```tml
behavior Error {
    func message(this: ref This) -> String
    func source(this: ref This) -> Maybe[ref dyn Error]
}
```

### 6.2 Common Error Types

```tml
// IO errors
type IoError =
    | NotFound(String)
    | PermissionDenied(String)
    | AlreadyExists(String)
    | InvalidInput(String)
    | TimedOut
    | Interrupted
    | Other(String)

// Parse errors
type ParseError = {
    message: String,
    line: U32,
    column: U32,
}

// Validation errors
type ValidationError = {
    field: String,
    message: String,
}
```

### 6.3 Error Chaining

```tml
type AppError = {
    message: String,
    source: Maybe[Heap[dyn Error]>,
}

impl AppError {
    func wrap[E: Error](error: E, message: String) -> This {
        This {
            message,
            source: Just(Heap.new(error)),
        }
    }
}

func load_config() -> Outcome[Config, AppError] {
    let content = File.read("config.json")
        .map_err(do(e) AppError.wrap(e, "failed to read config"))?
    // ...
}
```

---

## 7. Panic

For unrecoverable errors, use `panic!`:

```tml
func divide(a: I32, b: I32) -> I32 {
    if b == 0 then panic!("division by zero")
    a / b
}

// With formatting
panic!("index {index} out of bounds for length {len}")
```

### 7.1 Panic vs Error

| Use Case | Mechanism |
|----------|-----------|
| Invalid input from external source | `Outcome` |
| Bug in program logic | `panic!` |
| Resource exhaustion | Depends on recoverability |
| Contract violation | `panic!` (in debug) |

### 7.2 Catching Panics

Panics can be caught at task boundaries:

```tml
func run_safely() -> Outcome[Output, PanicInfo] {
    catch_panic(do() {
        risky_operation()
    })
}
```

---

## 8. Examples

### 8.1 File Processing Pipeline

```tml
func process_log_file(path: String) -> Outcome[Stats, Error] {
    let content = File.read(path)!
    let lines = content.lines()

    let mut stats = Stats.new()
    for line in lines {
        let entry = parse_log_entry(line)?
        stats.add(entry)
    }

    Ok(stats)
}

func parse_log_entry(line: String) -> Outcome[LogEntry, ParseError] {
    let parts = line.split(" ")
    if parts.len() < 3 then {
        return Err(ParseError { message: "invalid format", line: 0, column: 0 })
    }

    Ok(LogEntry {
        timestamp: parse_timestamp(parts[0])!,
        level: parse_level(parts[1])!,
        message: parts[2..].join(" "),
    })
}
```

### 8.2 HTTP Request with Retries

```tml
func fetch_with_retry(url: String, max_retries: U32) -> Outcome[Response, Error] {
    let mut last_error = Nothing

    for attempt in 0 to max_retries {
        when http.get(url) {
            Ok(response) -> return Ok(response),
            Err(e) if e.is_transient() -> {
                last_error = Just(e)
                sleep(Duration::seconds(1 << attempt))
            },
            Err(e) -> return Err(e),
        }
    }

    Err(last_error.unwrap_or(Error::MaxRetries))
}
```

### 8.3 Validation Pipeline

```tml
func validate_user(input: UserInput) -> Outcome[ValidUser, List[ValidationError]> {
    let mut errors: List[ValidationError] = []

    let name = when validate_name(input.name) {
        Ok(n) -> Just(n),
        Err(e) -> { errors.push(e); Nothing },
    }

    let email = when validate_email(input.email) {
        Ok(e) -> Just(e),
        Err(e) -> { errors.push(e); Nothing },
    }

    let age = when validate_age(input.age) {
        Ok(a) -> Just(a),
        Err(e) -> { errors.push(e); Nothing },
    }

    if errors.is_empty() then {
        Ok(ValidUser {
            name: name.unwrap(),
            email: email.unwrap(),
            age: age.unwrap(),
        })
    } else {
        Err(errors)
    }
}
```

---

## 9. IR Representation

### 9.1 Propagate Expression

```json
{
  "kind": "propagate",
  "expr": {
    "kind": "call",
    "func": "File.read",
    "args": [{ "kind": "ident", "name": "path" }]
  }
}
```

### 9.2 Else Expression

```json
{
  "kind": "else",
  "expr": { "kind": "call", "func": "find_user", "args": [...] },
  "fallback": { "kind": "call", "func": "Err", "args": [...] }
}
```

---

## 10. Compatibility

- **RFC-0001**: `Outcome` and `Maybe` are core algebraic types
- **RFC-0002**: `!` is postfix operator, `when` for pattern matching
- **RFC-0003**: Contracts can use error types in conditions

---

## 11. Alternatives Rejected

### 11.1 Exceptions

```java
// Rejected
try {
    file.read()
} catch (IOException e) {
    handle(e)
}
```

Problems:
- Hidden control flow
- Non-local effects
- Hard for LLMs to reason about
- Difficult to ensure handling

### 11.2 Error Codes (C-style)

```c
// Rejected
int result = read_file(path, &content);
if (result != 0) { ... }
```

Problems:
- Easy to ignore
- Out-of-band error info
- Verbose checking

### 11.3 ? Operator (Rust-style)

TML uses `!` instead of `?`:

```rust
// Rust
file.read()?

// TML
file.read()!
```

Rationale:
- `!` is more visually distinct
- Suggests "must succeed" semantics
- Consistent with `panic!` for failures

### 11.4 Checked Exceptions (Java-style)

```java
// Rejected
void process() throws IOException, ParseException { ... }
```

Problems:
- Rigid and inflexible
- Forces handling or declaration everywhere
- Leads to catch-and-ignore patterns

---

## 12. References

- [Rust Error Handling](https://doc.rust-lang.org/book/ch09-00-error-handling.html)
- [Swift Result Type](https://developer.apple.com/documentation/swift/result)
- [Haskell Maybe/Either](https://wiki.haskell.org/Maybe)
- [Railway Oriented Programming](https://fsharpforfunandprofit.com/rop/)
