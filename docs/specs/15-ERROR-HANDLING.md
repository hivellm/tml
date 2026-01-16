# TML v1.0 — Error Handling

## Design Philosophy

TML's error handling is designed for:
1. **LLM readability** - Clear, predictable error flow
2. **Pragmatic defaults** - Sensible behavior without boilerplate
3. **Explicit recovery** - Know exactly where errors are handled
4. **Zero hidden control flow** - No invisible exceptions

## 1. The `!` Operator (Propagate or Panic)

Every fallible function call ends with `!` which means:
- **If Ok**: unwrap and continue
- **If Err**: propagate to caller (if caller returns Outcome) OR panic (if caller returns value)

```tml
// Simple and clear - every ! is a potential exit point
func process_file(path: String) -> Outcome[Data, Error] {
    let file: Outcome[File, Error] = File.open(path)!        // propagate on error
    let content: String = file.read_all()!     // propagate on error
    let data: Outcome[Data, Error] = parse(content)!         // propagate on error
    return Ok(data)
}

// In a non-Outcome function, ! panics on error
func must_load_config() -> Config {
    let content: Outcome[String, Error] = File.read("config.json")!  // panic if fails
    return parse(content)!                    // panic if fails
}
```

### Why `!` instead of `?` or `try`

| Syntax | Problem |
|--------|---------|
| `?` (Rust) | Easy to miss visually, especially for LLMs |
| `try expr` | Adds noise, looks like control flow |
| `expr!` | Highly visible, clearly marks fallible points |

## 2. Inline Error Recovery with `else`

Handle errors inline without nesting:

```tml
// Pattern: expr! else recovery
let config: Config = parse_config(text)! else Config.default()

let port: Outcome[String, Error] = env.get("PORT")!
    .parse[U16]()! else 8080

let user: User = db.find_user(id)! else {
    log.warn("User not found: " + id.to_string())
    return Err(Error.NotFound)
}
```

### `else` with error binding

Access the error in the recovery block:

```tml
let data: Data = fetch_remote(url)! else do(err) {
    log.error("Fetch failed: " + err.to_string())
    load_from_cache(url)! else Data.empty()
}
```

## 3. Block-Level Error Handling with `catch`

For multiple operations that share error handling:

```tml
func sync_data() -> Outcome[Unit, SyncError] {
    catch {
        let local: Data = load_local()!
        let remote: Outcome[Data, Error] = fetch_remote()!
        let merged: Data = merge(local, remote)!
        save(merged)!
        return Ok(())
    } else do(err) {
        log.error("Sync failed: " + err.to_string())
        return Err(SyncError.from(err))
    }
}
```

### Scoped recovery

```tml
func robust_process() -> Data {
    // First attempt with timeout
    catch {
        return fetch_with_timeout(5.seconds())!
    } else {
        // Fallback to cache
        catch {
            return load_from_cache()!
        } else {
            // Final fallback
            return Data.default()
        }
    }
}
```

## 4. Outcome Type

```tml
type Outcome[T, E] = Ok(T) | Err(E)
```

### Methods

```tml
// Check state
outcome.is_ok() -> Bool
outcome.is_err() -> Bool

// Transform
outcome.map(do(t) ...)         // Map Ok value
outcome.map_err(do(e) ...)     // Map Err value
outcome.and_then(do(t) ...)    // Chain fallible operations

// Extract
outcome.unwrap() -> T          // Panic if Err
outcome.unwrap_or(default)     // Default if Err
outcome.unwrap_or_else(do() default)  // Lazy default

// Convert
outcome.to_maybe() -> Maybe[T]  // Discard error
outcome.err() -> Maybe[E]       // Discard value
```

## 5. Maybe Type

```tml
type Maybe[T] = Just(T) | Nothing
```

### Using `!` with Maybe

```tml
func find_user(id: U64) -> Maybe[User] { ... }

// In Outcome-returning function: converts Nothing to Err
func get_user_email(id: U64) -> Outcome[String, Error] {
    let user: User = find_user(id)!  // Nothing becomes Err(Error.NothingValue)
    return Ok(user.email)
}

// With custom error via else
func get_user_email(id: U64) -> Outcome[String, Error] {
    let user: User = find_user(id)! else return Err(Error.UserNotFound(id))
    return Ok(user.email)
}
```

## 6. Error Type Design

### Simple errors (enums)

```tml
type ParseError =
    | InvalidFormat
    | UnexpectedToken { found: String, expected: String }
    | EndOfInput
```

### Rich errors (structs with enum)

```tml
type Error {
    kind: ErrorKind,
    message: String,
    source: Maybe[Heap[Error]],  // Cause chain
    location: Location,           // File/line info
}

type ErrorKind =
    | Io
    | Parse
    | Network
    | Validation
    | NotFound
    | Unauthorized
```

### The `Error` behavior

```tml
behavior Error {
    func message(this) -> String
    func source(this) -> Maybe[ref dyn Error]  // Optional cause
}

// Auto-implement for simple enums
extend ParseError with Error {
    func message(this) -> String {
        when this {
            InvalidFormat -> "Invalid format",
            UnexpectedToken { found, expected } ->
                "Expected " + expected + ", found " + found,
            EndOfInput -> "Unexpected end of input",
        }
    }

    func source(this) -> Maybe[ref dyn Error] { Nothing }
}
```

## 7. Error Conversion

### Automatic conversion with `From` behavior

```tml
type AppError = Io(IoError) | Parse(ParseError) | Db(DbError)

extend AppError with From[IoError] {
    func from(e: IoError) -> AppError { AppError.Io(e) }
}

extend AppError with From[ParseError] {
    func from(e: ParseError) -> AppError { AppError.Parse(e) }
}

// Now ! automatically converts:
func load_and_parse() -> Outcome[Data, AppError] {
    let content: Outcome[String, Error] = File.read("data.txt")!  // IoError -> AppError
    let data: Outcome[Data, Error] = parse(content)!             // ParseError -> AppError
    return Ok(data)
}
```

## 8. Assertions and Contracts

### Runtime assertions

```tml
assert(condition, "message")           // Panic if false
assert_eq(a, b, "a should equal b")    // Panic if a != b
debug_assert(condition)                 // Only in debug builds
```

### Contracts (compile-time when possible)

```tml
func divide(a: I32, b: I32) -> I32
    requires b != 0
{
    return a / b
}

func binary_search(items: List[I32], target: I32) -> Maybe[U64]
    requires items.is_sorted()
{
    // ...
}
```

## 9. Panic and Recovery

### Panic

For truly unrecoverable errors:

```tml
panic("Something went terribly wrong")

// With formatted message
panic("Invalid state: expected {expected}, got {actual}")

// Unreachable code
func process(x: I32) -> String {
    when x {
        0 -> "zero",
        1 -> "one",
        _ -> unreachable("x should only be 0 or 1"),
    }
}
```

### Catch panic (for boundaries only)

```tml
// At process/thread boundaries
func safe_handler(request: Request) -> Response {
    catch_panic {
        return handle(request)!
    } else do(panic_info) {
        log.error("Handler panicked: " + panic_info.message())
        return Response.internal_error()
    }
}
```

## 10. Comparison: Before and After

### Before (verbose try)

```tml
func process() -> Outcome[Output, Error] {
    let a: A = try step_a()
    let b: B = try step_b(a)
    let c: C = try step_c(b)
    let d: D = try step_d(c)
    return Ok(d)
}
```

### After (clear ! markers)

```tml
func process() -> Outcome[Output, Error] {
    let a: A = step_a()!
    let b: B = step_b(a)!
    let c: C = step_c(b)!
    let d: D = step_d(c)!
    return Ok(d)
}
```

### Go-style (too verbose)

```tml
// We explicitly DON'T do this
func process() -> Outcome[Output, Error] {
    let a, err1 = step_a()
    if err1 then return Err(err1)

    let b, err2 = step_b(a)
    if err2 then return Err(err2)

    // ... repetitive and noisy
}
```

## 11. LLM Integration

### Clear error points for analysis

```tml
@id("fetch-data")
func fetch_data(url: String) -> Outcome[Data, FetchError] {
    @id("http-get")
    let response: Response = http.get(url)! else do(e) {
        return Err(FetchError.Network(e))
    }

    @id("parse-json")
    let data: Data = response.json[Data]()! else do(e) {
        return Err(FetchError.Parse(e))
    }

    return Ok(data)
}
```

### Predictable control flow

An LLM can analyze:
1. Every `!` is a potential early return (or panic)
2. Every `else` block is explicit error recovery
3. Every `catch` block groups related fallible operations
4. No hidden exceptions - what you see is what happens

## 12. Summary Table

| Syntax | Meaning | Use When |
|--------|---------|----------|
| `expr!` | Propagate error or panic | Most common, clear exit point |
| `expr! else default` | Recover with default | Fallback value needed |
| `expr! else do(e) { }` | Recover with error access | Need to log/transform error |
| `catch { } else { }` | Block-level error handling | Multiple related operations |
| `panic(msg)` | Unrecoverable error | Bug, invariant violation |
| `assert(cond)` | Debug check | Verify assumptions |
| `requires cond` | Contract precondition | Function requirements |

---

*Previous: [14-EXAMPLES.md](./14-EXAMPLES.md)*
*Next: [INDEX.md](./INDEX.md) — Back to Index*
