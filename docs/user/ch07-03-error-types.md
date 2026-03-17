# Designing Error Types

How you design your error types determines how usable your library or module is. Good error types communicate what went wrong, carry enough context to act on the failure, and compose cleanly with TML's `!` operator and `From` conversions.

## Simple Enum Errors

The simplest error type is an enum with one variant per failure mode:

```tml
type ParseError =
    | InvalidFormat
    | UnexpectedToken { found: Str }
    | UnexpectedEnd
    | Overflow

func parse_value(s: Str) -> Outcome[I32, ParseError] {
    if s.is_empty() {
        return Err(ParseError.UnexpectedEnd)
    }
    // ...
    if unexpected_char {
        return Err(ParseError.UnexpectedToken { found: ch.to_string() })
    }
    Ok(value)
}
```

Pattern matching on `ParseError` is exhaustive — the compiler ensures every variant is handled:

```tml
when parse_value(input) {
    Ok(v)  => println("Parsed: " + v.to_string()),
    Err(ParseError.InvalidFormat) => println("Format is wrong"),
    Err(ParseError.UnexpectedToken { found }) => {
        println("Unexpected character: " + found)
    },
    Err(ParseError.UnexpectedEnd) => println("Input ended too soon"),
    Err(ParseError.Overflow)      => println("Number too large"),
}
```

This works well for errors with a small, stable set of variants. Callers can react to each variant specifically, and adding a new variant later is a compile error at every call site — a useful forcing function for updating error handlers.

## Rich Errors with Context

For application-level errors, structured context makes failures debuggable:

```tml
type ErrorKind =
    | Io
    | Parse
    | Network
    | NotFound
    | Unauthorized
    | Internal

type AppError {
    kind: ErrorKind,
    message: Str,
    source: Maybe[Heap[dyn Error]],
}

extend AppError {
    func new(kind: ErrorKind, message: Str) -> AppError {
        return AppError {
            kind: kind,
            message: message,
            source: Nothing,
        }
    }

    func with_source[E: Error](mut this, cause: E) -> AppError {
        this.source = Just(Heap.new(cause as dyn Error))
        return this
    }

    func kind(this) -> ErrorKind { return this.kind }
    func message(this) -> ref Str { return ref this.message }
}

extend AppError with Display {
    func to_string(this) -> Str {
        let base = this.message.duplicate()
        when ref this.source {
            Just(cause) => base + ": " + cause.to_string(),
            Nothing     => base,
        }
    }
}

extend AppError with Error {
    func source(this) -> Maybe[ref dyn Error] {
        when ref this.source {
            Just(cause) => Just(ref *cause),
            Nothing     => Nothing,
        }
    }
}
```

The `source` chain preserves the original cause so that diagnostic tools can walk the full error chain. The `Heap[dyn Error]` stores any concrete error type through dynamic dispatch, keeping `AppError` independent of the specific lower-level error types it wraps.

## Error Conversion with From

When a function calls into multiple subsystems — file I/O, a database, a network library — each subsystem has its own error type. The `!` operator converts between error types automatically, provided the outer error type implements `From` for each inner type.

Define `From` implementations for every lower-level error your type wraps:

```tml
extend AppError with From[IoError] {
    func from(e: IoError) -> AppError {
        AppError.new(ErrorKind.Io, e.to_string())
            .with_source(e)
    }
}

extend AppError with From[ParseError] {
    func from(e: ParseError) -> AppError {
        AppError.new(ErrorKind.Parse, e.to_string())
            .with_source(e)
    }
}

extend AppError with From[NetworkError] {
    func from(e: NetworkError) -> AppError {
        AppError.new(ErrorKind.Network, e.to_string())
            .with_source(e)
    }
}
```

With these in place, a function that returns `Outcome[T, AppError]` can use `!` on any call returning `Outcome[_, IoError]`, `Outcome[_, ParseError]`, or `Outcome[_, NetworkError]` without explicit conversion:

```tml
func load_and_process(path: Str) -> Outcome[Report, AppError] {
    let raw     = File.read(path)!         // IoError → AppError
    let records = parse_csv(raw)!          // ParseError → AppError
    let remote  = fetch_metadata(path)!    // NetworkError → AppError
    return Ok(build_report(records, remote))
}
```

The `!` operator calls `AppError.from(e)` on each error before returning it. The caller sees only `AppError` and uses the `kind` field to dispatch to the right handler.

## Layered Error Hierarchies

In larger applications, it is common to define separate error types per module and compose them at the service level:

```tml
// auth module
type AuthError = InvalidToken | TokenExpired | InsufficientPermissions

// users module
type UserError = NotFound(U64) | AlreadyExists | InvalidEmail(Str)

// database module
type DbError = ConnectionFailed | QueryFailed(Str) | Timeout

// service level
type ServiceError =
    | Auth(AuthError)
    | User(UserError)
    | Db(DbError)
    | Internal(Str)

extend ServiceError with From[AuthError] {
    func from(e: AuthError) -> ServiceError { ServiceError.Auth(e) }
}

extend ServiceError with From[UserError] {
    func from(e: UserError) -> ServiceError { ServiceError.User(e) }
}

extend ServiceError with From[DbError] {
    func from(e: DbError) -> ServiceError { ServiceError.Db(e) }
}
```

Each module uses its own narrow error type internally. The service layer wraps them into `ServiceError` via `From`, and the `!` operator handles all the wrapping automatically.

## Assertions

TML provides assertion functions for conditions that must hold for the program to be correct. Assertions are not for expected runtime failures — they are for bugs.

**`assert(condition)`** — panics with a generic message if `condition` is `false`:

```tml
assert(index < len)
assert(not items.is_empty())
```

**`assert_eq(left, right)`** — panics and prints both values if they differ:

```tml
assert_eq(result, expected_value)
assert_eq(list.len(), 3)
```

**`assert_ne(left, right)`** — panics if the values are equal:

```tml
assert_ne(user_id, 0)
```

**`debug_assert(condition)`** — identical to `assert`, but compiled away in release builds. Use for expensive invariant checks that are only needed during development:

```tml
debug_assert(is_sorted(ref items))   // O(n) check, safe to skip in release
```

Assertions are appropriate inside functions to document preconditions, postconditions, and invariants that the function relies on. They communicate intent to both the compiler and future readers.

## Contracts: `requires` and `ensures`

TML supports lightweight design-by-contract annotations that are checked at runtime in debug builds and can be statically analyzed by tools:

```tml
func divide(a: F64, b: F64) -> F64
    requires b != 0.0
    ensures  return.is_finite()
{
    return a / b
}
```

`requires` expresses a precondition — a condition the caller must ensure before calling the function. If the precondition is violated at runtime (in debug mode), the program panics with a message explaining which precondition failed.

`ensures` expresses a postcondition — a condition that the function guarantees will hold for its return value. The keyword `return` inside an `ensures` clause refers to the value that will be returned.

Multiple conditions can be stacked:

```tml
func insert_sorted[T: Ord](mut list: ref List[T], value: T)
    requires list.len() < I64.MAX as U64
    ensures  list.len() == old(list.len()) + 1
{
    // binary search and insert
}
```

The `old(expr)` form inside `ensures` captures the value of an expression as it was at the point of the function call (before any mutation).

Contracts are documentation that can be checked. They make the assumptions of a function explicit and help tools identify call sites that may violate them.

## Panic and Unreachable

For situations that represent bugs rather than expected failures, use `panic` and `unreachable`:

```tml
func get_day_name(day: U8) -> Str {
    when day {
        0 => "Sunday",
        1 => "Monday",
        2 => "Tuesday",
        3 => "Wednesday",
        4 => "Thursday",
        5 => "Friday",
        6 => "Saturday",
        _ => unreachable("day value must be 0-6, got " + day.to_string()),
    }
}
```

`unreachable` is semantically equivalent to `panic` but communicates intent: the programmer asserts this branch can never execute. In release builds with optimizations, the compiler may use this as a hint to eliminate dead code.

`panic` is for defensive guards:

```tml
func process_chunk(data: ref [U8], expected_len: I64) {
    if data.len() != expected_len {
        panic("chunk length mismatch: expected " + expected_len.to_string() +
              ", got " + data.len().to_string())
    }
    // ...
}
```

Neither `panic` nor `unreachable` should be used as a substitute for proper error handling. If a condition is a plausible runtime outcome — wrong user input, a missing file, a network timeout — it belongs in an `Outcome`. If it represents a bug that should never occur in correct code, `panic` is appropriate.
