# Maybe and Outcome

TML's error model is built on two generic types: `Maybe[T]` and `Outcome[T, E]`. Both are ordinary enums — no magic, no hidden machinery. Their power comes from a rich set of methods that let you transform, combine, and extract values without reaching for pattern matching at every step.

## Maybe[T]

`Maybe[T]` represents an optional value. It is either `Just(T)`, meaning a value is present, or `Nothing`, meaning no value exists. The definition is:

```tml
type Maybe[T] = Just(T) | Nothing
```

Use `Maybe[T]` when the absence of a value is a normal, expected outcome — not an error. A function that looks up a key in a map may find nothing; a function that reads the first element of a list returns nothing when the list is empty. Neither situation is exceptional.

### Constructing Maybe Values

```tml
let found: Maybe[I32] = Just(42)
let missing: Maybe[I32] = Nothing

// Returning Maybe from a function
func find_user(id: U64) -> Maybe[User] {
    if id < users.len() as U64 {
        Just(users.get(id as I64))
    } else {
        Nothing
    }
}
```

### Testing the Variant

```tml
let value: Maybe[I32] = Just(10)

if value.is_just() {
    println("has a value")
}

if value.is_nothing() {
    println("no value")
}
```

### Extracting Values

**`unwrap()`** — returns the inner value, panics if `Nothing`:

```tml
let n: I32 = Just(42).unwrap()  // 42
let n: I32 = Nothing.unwrap()   // panics: called unwrap on Nothing
```

**`expect(message)`** — like `unwrap`, but the panic message is your own:

```tml
let cfg = load_config().expect("config file is required at startup")
```

**`unwrap_or(default)`** — returns the value if `Just`, otherwise returns the default:

```tml
let port = env_port.unwrap_or(8080)
```

**`unwrap_or_else(closure)`** — like `unwrap_or`, but the default is computed lazily:

```tml
let data = cache.get(key).unwrap_or_else(do() fetch_from_db(key))
```

### Transforming Maybe Values

**`map(f)`** — applies a function to the inner value if `Just`, leaves `Nothing` untouched:

```tml
let maybe_str: Maybe[Str] = Just(42).map(do(n) n.to_string())
// Just("42")

let nothing: Maybe[Str] = Nothing.map(do(n) n.to_string())
// Nothing
```

**`and_then(f)`** — chains operations that themselves return `Maybe`. If the current value is `Just(x)`, calls `f(x)` and returns its result; if `Nothing`, returns `Nothing` without calling `f`. Also called *flat map*:

```tml
func parse_port(s: Str) -> Maybe[U16] {
    // and_then threads Maybe values through a pipeline
    s.parse[I32]().ok()
     .and_then(do(n) if n > 0 and n < 65536 { Just(n as U16) } else { Nothing })
}
```

**`filter(predicate)`** — keeps the value if the predicate returns `true`, converts to `Nothing` otherwise:

```tml
let even = Just(4).filter(do(n) n % 2 == 0)   // Just(4)
let odd  = Just(3).filter(do(n) n % 2 == 0)   // Nothing
```

### Converting Maybe to Outcome

**`ok_or(error)`** — converts `Just(v)` to `Ok(v)` and `Nothing` to `Err(error)`:

```tml
func require_name(maybe_name: Maybe[Str]) -> Outcome[Str, Str] {
    return maybe_name.ok_or("name is required")
}
```

**`ok_or_else(f)`** — like `ok_or`, but the error value is computed lazily:

```tml
let result = find_user(id).ok_or_else(do() UserError.NotFound(id))
```

### Pattern Matching on Maybe

Pattern matching with `when` handles both cases explicitly:

```tml
func greet_user(maybe_user: Maybe[User]) -> Str {
    when maybe_user {
        Just(user) => "Hello, " + user.name + "!",
        Nothing    => "Hello, guest!",
    }
}
```

Destructuring in `when` arms gives you direct access to the inner value without an extra unwrap step. The compiler verifies that both variants are covered.

## Outcome[T, E]

`Outcome[T, E]` represents an operation that either succeeds with a value of type `T` or fails with an error of type `E`. Its definition is:

```tml
type Outcome[T, E] = Ok(T) | Err(E)
```

Use `Outcome[T, E]` when failure carries information the caller needs — a parse error describing what went wrong, an I/O error with an OS error code, a validation error listing which fields are invalid.

### Constructing Outcome Values

```tml
let success: Outcome[I32, Str] = Ok(42)
let failure: Outcome[I32, Str] = Err("something went wrong")

// Returning Outcome from a function
func parse_int(s: Str) -> Outcome[I32, ParseError] {
    // ... parsing logic
    if valid {
        Ok(parsed_value)
    } else {
        Err(ParseError.InvalidFormat(s))
    }
}
```

### Testing the Variant

```tml
let r: Outcome[I32, Str] = Ok(42)

if r.is_ok() {
    println("succeeded")
}

if r.is_err() {
    println("failed")
}
```

### Extracting Values

**`unwrap()`** — returns the `Ok` value, panics on `Err`:

```tml
let n: I32 = Ok(42).unwrap()
```

**`expect(message)`** — panics with a custom message on `Err`:

```tml
let conn = db.connect().expect("database connection is required")
```

**`unwrap_or(default)`** — returns the `Ok` value or a default on `Err`:

```tml
let count = db.count_users().unwrap_or(0)
```

**`unwrap_err()`** — returns the `Err` value, panics on `Ok`. Rarely needed outside tests:

```tml
let e = parse_int("abc").unwrap_err()
```

### Transforming Outcome Values

**`map(f)`** — transforms the `Ok` value; leaves `Err` untouched:

```tml
let doubled: Outcome[I32, ParseError] = parse_int("21").map(do(n) n * 2)
// Ok(42)
```

**`map_err(f)`** — transforms the `Err` value; leaves `Ok` untouched. Used to convert between error types:

```tml
let result: Outcome[I32, AppError] =
    parse_int("42").map_err(do(e) AppError.Parse(e))
```

**`and_then(f)`** — chains fallible operations. If `Ok(x)`, calls `f(x)` and returns its `Outcome`; if `Err`, short-circuits without calling `f`:

```tml
func load_and_validate(path: Str) -> Outcome[Config, AppError] {
    File.read(path)
        .map_err(do(e) AppError.Io(e))
        .and_then(do(content) parse_config(content))
        .and_then(do(cfg) validate_config(cfg))
}
```

Each step in the chain runs only if the previous succeeded. The first error terminates the chain and is carried through to the final result.

**`or_else(f)`** — the error-side counterpart to `and_then`. If `Err(e)`, calls `f(e)` to attempt recovery; if `Ok`, passes through:

```tml
let data = fetch_primary(url).or_else(do(e) {
    log.warn("Primary failed: " + e.to_string() + ", trying backup")
    fetch_backup(url)
})
```

### Pattern Matching on Outcome

```tml
func handle_request(request: Request) -> Response {
    when process(request) {
        Ok(data)  => Response.ok(data),
        Err(e)    => {
            log.error("Request failed: " + e.to_string())
            Response.error(500, e.to_string())
        },
    }
}
```

For more detailed error inspection, match on the error variant directly:

```tml
func describe_error(e: AppError) -> Str {
    when e {
        AppError.NotFound(id)     => "Resource " + id.to_string() + " not found",
        AppError.Unauthorized     => "Access denied",
        AppError.Parse(inner)     => "Parse error: " + inner.to_string(),
        AppError.Io(inner)        => "I/O error: " + inner.to_string(),
    }
}
```

## Converting Between Maybe and Outcome

The standard library provides symmetrical conversions in both directions.

**Maybe to Outcome:**

```tml
// ok_or — supply the error value up front
let outcome: Outcome[I32, Str] = Just(42).ok_or("missing")
let outcome: Outcome[I32, Str] = Nothing.ok_or("missing")
// Ok(42), Err("missing")

// ok_or_else — compute the error lazily
let outcome = maybe_user.ok_or_else(do() UserError.NotFound(id))
```

**Outcome to Maybe:**

```tml
// ok() — discard the error, keep the success value
let maybe: Maybe[I32] = Ok(42).ok()   // Just(42)
let maybe: Maybe[I32] = Err("x").ok() // Nothing

// err() — discard the success, keep the error
let maybe_err: Maybe[Str] = Ok(42).err()   // Nothing
let maybe_err: Maybe[Str] = Err("x").err() // Just("x")
```

A common pattern is to convert `Outcome` to `Maybe` when you want to use `Maybe`-combinators and the specific error is not relevant:

```tml
func first_valid_port(candidates: List[Str]) -> Maybe[U16] {
    loop s in candidates {
        let result = s.parse[U16]().ok()
        if result.is_just() {
            return result
        }
    }
    return Nothing
}
```

## Choosing Between Maybe and Outcome

| Situation | Type to use |
|-----------|-------------|
| Value may or may not exist; absence is normal | `Maybe[T]` |
| Operation can fail; caller needs to know why | `Outcome[T, E]` |
| Looking up a key in a map | `Maybe[V]` |
| Parsing user input | `Outcome[T, ParseError]` |
| Finding the first match in a list | `Maybe[T]` |
| Reading a file | `Outcome[Str, IoError]` |
| Getting an optional config value | `Maybe[T]` |
| Calling an external API | `Outcome[Response, NetworkError]` |

The distinction is not about whether failure is possible — it is about whether failure carries meaning that the caller should act on. When the answer to "why did this fail?" matters, use `Outcome`. When absence is simply the absence of a match, use `Maybe`.
