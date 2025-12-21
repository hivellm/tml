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
- **If Err**: propagate to caller (if caller returns Result) OR panic (if caller returns value)

```tml
// Simple and clear - every ! is a potential exit point
func process_file(path: String) -> Result[Data, Error] {
    let file = File.open(path)!        // propagate on error
    let content = file.read_all()!     // propagate on error
    let data = parse(content)!         // propagate on error
    return Ok(data)
}

// In a non-Result function, ! panics on error
func must_load_config() -> Config {
    let content = File.read("config.json")!  // panic if fails
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
let config = parse_config(text)! else Config.default()

let port = env.get("PORT")!
    .parse[U16]()! else 8080

let user = db.find_user(id)! else {
    log.warn("User not found: " + id.to_string())
    return Err(Error.NotFound)
}
```

### `else` with error binding

Access the error in the recovery block:

```tml
let data = fetch_remote(url)! else |err| {
    log.error("Fetch failed: " + err.to_string())
    load_from_cache(url)! else Data.empty()
}
```

## 3. Block-Level Error Handling with `catch`

For multiple operations that share error handling:

```tml
func sync_data() -> Result[Unit, SyncError] {
    catch {
        let local = load_local()!
        let remote = fetch_remote()!
        let merged = merge(local, remote)!
        save(merged)!
        return Ok(())
    } else |err| {
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

## 4. Result Type

```tml
type Result[T, E] = Ok(T) | Err(E)
```

### Methods

```tml
// Check state
result.is_ok() -> Bool
result.is_err() -> Bool

// Transform
result.map(do(t) ...)         // Map Ok value
result.map_err(do(e) ...)     // Map Err value
result.and_then(do(t) ...)    // Chain fallible operations

// Extract
result.unwrap() -> T          // Panic if Err
result.unwrap_or(default)     // Default if Err
result.unwrap_or_else(do() default)  // Lazy default

// Convert
result.ok() -> Option[T]      // Discard error
result.err() -> Option[E]     // Discard value
```

## 5. Option Type

```tml
type Option[T] = Some(T) | None
```

### Using `!` with Option

```tml
func find_user(id: U64) -> Option[User] { ... }

// In Result-returning function: converts None to Err
func get_user_email(id: U64) -> Result[String, Error] {
    let user = find_user(id)!  // None becomes Err(Error.NoneValue)
    return Ok(user.email)
}

// With custom error via else
func get_user_email(id: U64) -> Result[String, Error] {
    let user = find_user(id)! else return Err(Error.UserNotFound(id))
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
    source: Option[Box[Error]],  // Cause chain
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

### The `Error` trait

```tml
trait Error {
    func message(this) -> String
    func source(this) -> Option[&dyn Error]  // Optional cause
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

    func source(this) -> Option[&dyn Error] { None }
}
```

## 7. Error Conversion

### Automatic conversion with `From` trait

```tml
type AppError = Io(IoError) | Parse(ParseError) | Db(DbError)

extend AppError with From[IoError] {
    func from(e: IoError) -> AppError { AppError.Io(e) }
}

extend AppError with From[ParseError] {
    func from(e: ParseError) -> AppError { AppError.Parse(e) }
}

// Now ! automatically converts:
func load_and_parse() -> Result[Data, AppError] {
    let content = File.read("data.txt")!  // IoError -> AppError
    let data = parse(content)!             // ParseError -> AppError
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

func binary_search(items: List[I32], target: I32) -> Option[U64]
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
    } else |panic_info| {
        log.error("Handler panicked: " + panic_info.message())
        return Response.internal_error()
    }
}
```

## 10. Comparison: Before and After

### Before (verbose try)

```tml
func process() -> Result[Output, Error] {
    let a = try step_a()
    let b = try step_b(a)
    let c = try step_c(b)
    let d = try step_d(c)
    return Ok(d)
}
```

### After (clear ! markers)

```tml
func process() -> Result[Output, Error] {
    let a = step_a()!
    let b = step_b(a)!
    let c = step_c(b)!
    let d = step_d(c)!
    return Ok(d)
}
```

### Go-style (too verbose)

```tml
// We explicitly DON'T do this
func process() -> Result[Output, Error] {
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
func fetch_data(url: String) -> Result[Data, FetchError] {
    @id("http-get")
    let response = http.get(url)! else |e| {
        return Err(FetchError.Network(e))
    }

    @id("parse-json")
    let data = response.json[Data]()! else |e| {
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
| `expr! else \|e\| { }` | Recover with error access | Need to log/transform error |
| `catch { } else { }` | Block-level error handling | Multiple related operations |
| `panic(msg)` | Unrecoverable error | Bug, invariant violation |
| `assert(cond)` | Debug check | Verify assumptions |
| `requires cond` | Contract precondition | Function requirements |

---

*Previous: [14-EXAMPLES.md](./14-EXAMPLES.md)*
*Next: [INDEX.md](./INDEX.md) — Back to Index*
