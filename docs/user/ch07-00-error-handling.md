# Error Handling

Every program encounters conditions it cannot always satisfy: a file that does not exist, a network request that times out, user input that fails validation. How a language handles these conditions shapes the reliability and readability of every program written in it.

TML takes an explicit, value-based approach to error handling. Errors are ordinary values — they are returned from functions, stored in variables, transformed with combinators, and propagated with a single operator. There are no hidden exception stacks, no invisible early exits, and no untyped runtime errors that must be caught at arbitrary call sites.

## The Two Core Types

TML provides two types that cover all common error-handling situations:

- `Maybe[T]` — an optional value that is either `Just(T)` (present) or `Nothing` (absent). Use this when absence is not an error, or when the caller does not need to know why a value is missing.

- `Outcome[T, E]` — a result that is either `Ok(T)` (success with a value) or `Err(E)` (failure with an error). Use this when failures carry information the caller needs.

These two types sit at the center of TML's error model. Every fallible operation in the standard library returns one of them.

## The `!` Operator

The `!` postfix operator is how you work with fallible values in practice. Applied to an `Outcome` or `Maybe`, it means: "if this succeeded, give me the value; if it failed, handle it."

In a function that returns an `Outcome`, `!` propagates the error to the caller:

```tml
func read_config(path: Str) -> Outcome[Config, IoError] {
    let content = File.read(path)!        // propagates IoError on failure
    let config = parse_config(content)!   // propagates ParseError on failure
    return Ok(config)
}
```

In a function that does not return an `Outcome`, `!` panics if the value is an error:

```tml
func load_config_or_die() -> Config {
    let content = File.read("config.toml")!  // panics if file is missing
    return parse_config(content)!
}
```

Every `!` is a visible potential exit point. You can read any function and know exactly where it might return early or abort by scanning for `!` operators.

## Inline Recovery with `else`

When you want to recover from a failure rather than propagate it, attach an `else` clause directly to the `!` expression:

```tml
// Use a default value on failure
let port = env.get("PORT")!.parse[U16]()! else 8080

// Log the error and return a fallback
let user = db.find_user(id)! else {
    log.warn("User not found, using guest")
    return Ok(UserData.guest())
}

// Access the error value in the recovery block
let prefs = fetch_preferences(id)! else do(err) {
    log.debug("No preferences: " + err.to_string())
    Preferences.default()
}
```

The `else` clause turns what would be a propagation or panic into an inline recovery. The three forms above cover the common cases: a default value, a full recovery block, and a recovery block that inspects the error.

## Block-Level Error Handling with `catch`

When several operations share the same error handling logic, wrapping them in a `catch` block eliminates repetition:

```tml
func sync_data() -> Outcome[Unit, SyncError] {
    catch {
        let local = load_local()!
        let remote = fetch_remote()!
        let merged = merge(local, remote)!
        save(merged)!
        return Ok(())
    } else do(err) {
        log.error("Sync failed: " + err.to_string())
        return Err(SyncError.from(err))
    }
}
```

Any `!` inside the `catch` block that would propagate is instead redirected to the `else` handler. This gives you Rust's `?`-operator style pipelines with an explicit, co-located recovery point.

## Panic for Unrecoverable Situations

Not every error is recoverable. When a program encounters a state that violates its own invariants — an index out of bounds, a null that should never be null, a branch that should never be reached — it should stop immediately rather than continue in a corrupt state.

TML provides `panic` and `unreachable` for these situations:

```tml
panic("index out of bounds")
unreachable("this branch should never execute")
```

Panics unwind the current thread and cannot be caught in normal code. They are not for handling expected failure conditions; they are for bugs.

## What This Chapter Covers

The following sections explain the error-handling system in detail:

- **Maybe and Outcome** (ch07-01) — the two types, their constructors, their method APIs, and when to use each.
- **The ! Operator and Recovery** (ch07-02) — how `!` propagates errors, how `else` recovers from them inline, and how `catch` groups related operations.
- **Designing Error Types** (ch07-03) — how to define error enums and structs, how to implement the `Error` behavior, and how automatic error conversion with `From` makes the `!` operator compose across error type boundaries.

## A Note on Explicit Control Flow

TML's design principle is that every exit point from a function should be visible in the source code. The `!` operator makes this concrete: it is the only way for a fallible call to cause an early return or panic. You cannot accidentally ignore an error — the type system will reject code that does not handle an `Outcome`. You cannot have invisible exceptions propagate through multiple stack frames — all propagation goes through `!` expressions that you can see and count.

This predictability is particularly valuable for LLM-generated code, where hidden control flow is a common source of subtle defects.
