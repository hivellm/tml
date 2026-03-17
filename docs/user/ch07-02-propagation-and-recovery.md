# The ! Operator and Recovery

The `!` postfix operator is the primary tool for working with fallible values in TML. It is both a propagation operator — forwarding errors up the call stack — and an exit point that can be redirected with an inline `else` clause or a `catch` block. Every `!` in a function is a visible, searchable marker of a potential early exit.

## Propagation in Outcome-Returning Functions

When a function returns `Outcome[T, E]`, applying `!` to an expression of type `Outcome[U, F]` does two things:

1. If the value is `Ok(v)`, the expression evaluates to `v` and execution continues normally.
2. If the value is `Err(e)`, the function returns `Err(E.from(e))` immediately. The current function's remaining code does not execute.

The automatic `E.from(e)` conversion means that as long as the outer error type implements `From[F]`, error types can differ between calls:

```tml
type AppError = Io(IoError) | Parse(ParseError) | Validation(ValidationError)

extend AppError with From[IoError] {
    func from(e: IoError) -> AppError { AppError.Io(e) }
}

extend AppError with From[ParseError] {
    func from(e: ParseError) -> AppError { AppError.Parse(e) }
}

func load_and_parse(path: Str) -> Outcome[Config, AppError] {
    let raw: Str = File.read(path)!        // IoError → AppError via From
    let config: Config = parse(raw)!       // ParseError → AppError via From
    return Ok(config)
}
```

Each `!` is a concise annotation that says "this call may fail; if it does, convert the error and return it." The compiler enforces that the conversion is valid — if `AppError` does not implement `From[SomeOtherError]`, the `!` on that call is a type error.

### Propagating Maybe Values

`!` also works on `Maybe[T]`:

```tml
func get_username(id: U64) -> Maybe[Str] {
    let user: User = find_user(id)!    // returns Nothing if not found
    return Just(user.name)
}
```

When used in a `Maybe`-returning function, `!` on a `Nothing` returns `Nothing` from the enclosing function.

## Panic in Non-Outcome Functions

When `!` is used inside a function that does not return `Outcome` or `Maybe`, there is nowhere to propagate an error. In this case, `!` panics with a descriptive message if the value is `Err` or `Nothing`:

```tml
func main() {
    let config: Config = File.read("config.toml")!   // panics on error
    let port: U16 = config.port.to_string().parse[U16]()!  // panics on error
    start_server(config, port)
}
```

This is appropriate for startup code, scripts, and situations where there is genuinely no sensible recovery path. The panic message includes the error value and the source location, making failures easy to diagnose.

## Inline Recovery with `! else`

When you want to handle a failure locally rather than propagate it, attach an `else` clause to the `!` expression. The `else` clause runs only when the value is `Err` or `Nothing`; on success, the `!` expression produces the inner value as usual.

### Default Value Recovery

The simplest form supplies a fallback value directly:

```tml
let port: U16 = env.get("PORT")!.parse[U16]()! else 8080
let timeout: U64 = config.timeout! else 30_000
let name: Str = find_name(id)! else "anonymous"
```

The type of the `else` expression must match the type of the `Ok` variant (or `Just` variant). The compiler verifies this.

### Block Recovery

For more complex recovery logic, the `else` clause can be a block:

```tml
let user: User = db.find_user(id)! else {
    log.warn("User " + id.to_string() + " not found, using guest")
    metrics.increment("guest_fallback")
    User.guest()
}
```

The block must produce a value of the correct type, either by evaluating to it as the final expression or by returning or breaking from the enclosing function.

You can also use the block to exit the current function entirely:

```tml
func get_settings(id: U64) -> Outcome[Settings, AppError] {
    let user: User = db.find_user(id)! else {
        return Err(AppError.NotFound(id))
    }
    return Ok(user.settings)
}
```

### Error-Binding Recovery

The third form binds the error or absence to a name so the recovery block can inspect it:

```tml
let data: Data = fetch(url)! else do(err) {
    log.debug("Fetch failed (" + err.to_string() + "), using cache")
    cache.get(url).unwrap_or(Data.empty())
}
```

The `do(err)` pattern binds the error value. For `Maybe`, there is no error value to bind, so the `do(_)` form is conventional:

```tml
let config: Config = load_config()! else do(_) Config.default()
```

## Block-Level Error Handling with `catch`

When a series of operations all share the same error handling logic, wrapping them in a `catch` block eliminates the need to write `! else { ... }` on every line. Any `!` inside the `catch` block that would propagate is instead redirected to the `else` handler:

```tml
func sync_data() -> Outcome[Unit, SyncError] {
    catch {
        let local  = load_local()!
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

Without `catch`, each `!` would need its own `else` clause. With `catch`, all four operations share a single handler. The handler receives the first error that occurred, regardless of which operation produced it.

The `else` clause on a `catch` block uses the same three forms as inline recovery:

```tml
// Default value — the catch block evaluates to this on any error
let result: I32 = catch {
    let a = step_a()!
    let b = step_b(a)!
    step_c(a, b)!
} else 0

// Block — run arbitrary recovery logic
let conn: Connection = catch {
    Connection.open(primary_url)!
} else {
    log.warn("Primary failed, trying backup")
    Connection.open(backup_url)!
}

// Error-binding — inspect the error
let response: Response = catch {
    api.call(request)!
} else do(err) {
    log.error("API call failed: " + err.to_string())
    Response.service_unavailable()
}
```

### Nested Catch for Fallback Strategies

`catch` blocks can be nested to implement layered fallback strategies:

```tml
func load_config(paths: List[Str]) -> Outcome[Config, ConfigError] {
    // Try each path in order; the first success wins
    let raw: Str = catch {
        File.read(paths.get(0))!
    } else catch {
        log.info("Primary config not found, trying secondary")
        File.read(paths.get(1))!
    } else catch {
        log.info("Secondary config not found, trying bundled default")
        File.read(paths.get(2))!
    } else {
        return Err(ConfigError.NoConfigFound)
    }

    let config = parse_config(raw)! else do(e) {
        return Err(ConfigError.ParseFailed(e))
    }

    return Ok(config)
}
```

The outermost `catch` tries the first path. If that fails, the first `else catch` tries the second path. If that also fails, the second `else catch` tries the bundled default. If all three fail, the final `else` returns an error.

## Real-World Examples

### Configuration Loading

```tml
type ConfigError =
    | NotFound
    | ParseFailed(ParseError)
    | ValidationFailed(Str)

func load_app_config() -> Outcome[AppConfig, ConfigError] {
    // Try user config, fall back to system config, then embedded defaults
    let path: Str = catch {
        env.get("APP_CONFIG_PATH")!
    } else {
        catch {
            // Check standard XDG path on Linux
            let home = env.get("HOME")!
            home + "/.config/app/config.toml"
        } else {
            "/etc/app/config.toml"
        }
    }

    let raw: Str = File.read(path)! else do(e) {
        return Err(ConfigError.NotFound)
    }

    let config: AppConfig = parse_toml[AppConfig](raw)! else do(e) {
        return Err(ConfigError.ParseFailed(e))
    }

    validate_config(ref config)! else do(msg) {
        return Err(ConfigError.ValidationFailed(msg))
    }

    return Ok(config)
}
```

### HTTP API Handler

```tml
type HandlerError = BadRequest(Str) | Unauthorized | DbError(DbError) | Internal(Str)

func handle_update_user(req: Request) -> Outcome[Response, HandlerError] {
    // Parse and validate the request
    let token: AuthToken = catch {
        req.headers.get("Authorization")!
            .strip_prefix("Bearer ")!
            .parse[AuthToken]()!
    } else {
        return Err(HandlerError.Unauthorized)
    }

    let body: UpdateUserBody = catch {
        req.body.parse_json[UpdateUserBody]()!
    } else do(e) {
        return Err(HandlerError.BadRequest("Invalid JSON: " + e.to_string()))
    }

    body.validate()! else do(msg) {
        return Err(HandlerError.BadRequest(msg))
    }

    // Authorize the request
    let user_id: U64 = auth.verify(token)! else {
        return Err(HandlerError.Unauthorized)
    }

    // Perform the update
    let updated: User = catch {
        db.begin_transaction()!
        let user = db.find_user(user_id)! else do(e) {
            return Err(HandlerError.DbError(e))
        }
        user.apply_update(ref body)
        db.save_user(user)!
        db.commit()!
        user
    } else do(e) {
        db.rollback()
        return Err(HandlerError.DbError(e))
    }

    return Ok(Response.ok_json(updated))
}
```

The handler uses `catch` to group related error conditions, inline `! else` for single recoveries, and error-binding forms when the error value needs to be forwarded. Every exit point is visible in the source.

## Summary of Forms

| Form | When to use |
|------|-------------|
| `expr!` | Propagate or panic — the common case |
| `expr! else value` | Use a compile-time-constant or literal default |
| `expr! else { block }` | Recovery requires multiple statements |
| `expr! else do(err) { }` | Recovery needs to inspect the error value |
| `catch { ... } else { }` | Multiple operations share one handler |
| `catch { ... } else do(err) { }` | Shared handler needs the error value |
