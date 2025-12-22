# 25. Custom Decorators

TML provides native support for user-defined decorators as first-class language constructs.

## 1. Overview

Decorators are compile-time or runtime transformations applied to code elements. They enable:

- **Metaprogramming** - Code generation and transformation
- **Cross-cutting concerns** - Logging, caching, validation
- **Domain-specific abstractions** - Custom attributes and behaviors
- **Framework integration** - Testing, serialization, dependency injection

## 2. Decorator Definition

### 2.1 Basic Syntax

```tml
decorator <name> {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        // Transform or inspect target
    }
}
```

### 2.2 Parameterized Decorators

```tml
decorator <name>(<params>) {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        // Use params in transformation
    }
}
```

### 2.3 Complete Example

```tml
decorator log_calls(level: LogLevel = LogLevel.Debug) {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Func(f) -> {
                let name: String = f.name
                let original: Expr = f.body
                f.body = quote {
                    log(${level}, "Entering: " + ${name})
                    let __result: T = ${original}
                    log(${level}, "Exiting: " + ${name})
                    __result
                }
                DecoratorResult.Modified(f)
            },
            _ -> DecoratorResult.Error("@log_calls only applies to functions"),
        }
    }
}

// Usage
@log_calls(level: LogLevel.Info)
func process_data(data: Data) -> Outcome[Result, Error] {
    // Function body
}
```

## 3. Decorator Target Types

```tml
type DecoratorTarget =
    | Func(FuncDef)           // Functions and methods
    | Type(TypeDef)           // Struct, enum, type alias
    | Field(FieldDef)         // Struct fields
    | Variant(VariantDef)     // Enum variants
    | Param(ParamDef)         // Function parameters
    | Impl(ImplBlock)         // Implementation blocks
    | Mod(ModDef)             // Modules
```

### 3.1 Target-Specific Decorators

```tml
// Only applies to functions
decorator timing {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Func(f) -> transform_func(f),
            _ -> DecoratorResult.Error("@timing only applies to functions"),
        }
    }
}

// Only applies to struct fields
decorator validate(pattern: String) {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Field(f) -> add_validation(f, pattern),
            _ -> DecoratorResult.Error("@validate only applies to fields"),
        }
    }
}

// Applies to multiple targets
decorator deprecated(message: String = "Deprecated") {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        compiler.warn(target.span, message)
        DecoratorResult.Unchanged
    }
}
```

## 4. Decorator Results

```tml
type DecoratorResult =
    | Unchanged                      // No modification
    | Modified(DecoratorTarget)      // Replace target
    | AddItem(Item)                  // Add new item (impl, func, etc.)
    | AddItems(List[Item])           // Add multiple items
    | Remove                         // Remove target (use carefully)
    | Error(String)                  // Decorator error
```

### 4.1 Examples

```tml
// Modify function body
DecoratorResult.Modified(f)

// Add generated impl block
DecoratorResult.AddItem(generated_impl)

// Emit warning but don't change
DecoratorResult.Unchanged

// Error for invalid usage
DecoratorResult.Error("Invalid decorator application")
```

## 5. Execution Time

### 5.1 Compile-Time Decorators (Default)

Execute during compilation, transform AST:

```tml
@compile_time  // Default, can be omitted
decorator inline_strings {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        // Runs at compile time
        // Has access to AST
        // Can generate code
    }
}
```

### 5.2 Runtime Decorators

Execute at program startup or first use:

```tml
@runtime
decorator memoize {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Func(f) -> {
                f.body = quote {
                    static cache: Map[Args, Return] = Map.new()
                    let key: Tuple = (${f.params.as_tuple()})
                    if let Just(v) = cache.get(ref key) then {
                        return v.duplicate()
                    }
                    let result: T = ${f.original_body}
                    cache.insert(key, result.duplicate())
                    result
                }
                DecoratorResult.Modified(f)
            },
            _ -> DecoratorResult.Error("@memoize only applies to functions"),
        }
    }
}
```

## 6. Quote and Splice

### 6.1 Quote Syntax

`quote { ... }` creates a code template:

```tml
let code: Code = quote {
    let x: I32 = 1 + 2
    println(x.to_string())
}
```

### 6.2 Splice Syntax

`${expr}` inserts expressions into quotes:

```tml
let value: I32 = 42
let code: Code = quote {
    let x: I32 = ${value}  // Becomes: let x: I32 = 42
}
```

### 6.3 Identifier Splice

`$name` splices identifiers:

```tml
let func_name: String = "my_func"
let code: Code = quote {
    func $func_name() { }  // Becomes: func my_func() { }
}
```

### 6.4 Complete Example

```tml
decorator trace {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Func(f) -> {
                let name: String = f.name
                let params_str: String = f.params.map(do(p) p.name).join(", ")

                f.body = quote {
                    println("[TRACE] ${name}(" + ${params_str} + ")")
                    let __start: Instant = Instant.now()
                    let __result: T = ${f.original_body}
                    let __elapsed: Duration = __start.elapsed()
                    println("[TRACE] ${name} returned in " + __elapsed.as_micros().to_string() + "µs")
                    __result
                }

                DecoratorResult.Modified(f)
            },
            _ -> DecoratorResult.Error("@trace requires a function"),
        }
    }
}
```

## 7. Decorator Composition

### 7.1 Order of Application

Decorators apply top-to-bottom, wrap inside-out:

```tml
@outer       // Applied last, wraps everything
@middle      // Applied second
@inner       // Applied first, closest to original
func example() { }

// Execution: outer(middle(inner(original)))
```

### 7.2 Example

```tml
@log_calls    // Outermost: logs entry/exit
@retry(3)     // Middle: retries on failure
@timing       // Innermost: measures time
func fetch(url: String) -> Outcome[Data, Error] {
    http.get(url)
}

// When called:
// 1. log_calls logs "Entering fetch"
// 2. retry attempts up to 3 times
// 3. timing measures each attempt
// 4. Original function executes
// 5. timing records duration
// 6. retry handles failure
// 7. log_calls logs "Exiting fetch"
```

## 8. Built-in Decorators

| Decorator | Target | Description |
|-----------|--------|-------------|
| `@test` | Func | Mark as test function |
| `@test(name)` | Func | Named test |
| `@bench` | Func | Mark as benchmark |
| `@bench(iterations)` | Func | Benchmark with iteration count |
| `@inline` | Func | Hint to inline function |
| `@inline(always)` | Func | Force inline |
| `@inline(never)` | Func | Never inline |
| `@cold` | Func | Hint: rarely called |
| `@hot` | Func | Hint: frequently called |
| `@stable` | Any | Mark API as stable (stable guarantee) |
| `@stable(since)` | Any | Stable with version info |
| `@unstable` | Any | Mark API as unstable (may change) |
| `@deprecated` | Any | Emit deprecation warning |
| `@deprecated(msg)` | Any | Custom deprecation message |
| `@deprecated(since, removal)` | Any | With version info |
| `@must_use` | Func/Type | Warn if result unused |
| `@must_use(msg)` | Func/Type | Custom unused warning |
| `@derive(...)` | Type | Auto-implement behaviors |
| `@when(...)` | Any | Conditional compilation |
| `@doc(...)` | Any | Documentation |
| `@allow(...)` | Any | Suppress specific warnings |
| `@deny(...)` | Any | Error on specific warnings |
| `@pre(...)` | Func | Precondition contract |
| `@post(...)` | Func | Postcondition contract |
| `@invariant(...)` | Type | Type invariant |

### 8.1 Stability System

TML provides a comprehensive stability system inspired by Rust's API stability guarantees:

**Stability Levels:**
- **`@stable`** - API is guaranteed stable (breaking changes require major version bump)
- **`@unstable`** - API may change without warning (experimental features)
- **`@deprecated`** - API will be removed in future version (use alternative)

**Usage Example:**

```tml
// Mark functions with stability annotations
@stable(since: "v1.0")
func print(msg: Str) -> () {
    // Stable API - guaranteed backwards compatible
}

@deprecated(since: "v1.2", use: "Instant::now()")
func time_ms() -> I32 {
    // Deprecated - compiler emits warning
}

@unstable
func experimental_feature() -> () {
    // Unstable - may change in any release
}
```

**Compiler Flags:**
- `--forbid-deprecated` - Treat deprecation warnings as errors
- `--allow-unstable` - Suppress unstable API warnings
- `--stability-report` - Generate usage report of deprecated/unstable APIs

**Stability Guarantees:**
- `@stable` APIs follow semantic versioning
- Breaking changes to `@stable` APIs require major version increment
- `@deprecated` APIs have migration period (minimum 1 minor version)
- Compiler warns about unstable/deprecated API usage by default

## 9. Derive Decorator

### 9.1 Standard Derives

```tml
@derive(Eq, Ord, Hash, Debug, Duplicate, Default)
type Point = {
    x: F64,
    y: F64,
}
```

### 9.2 Available Standard Derives

| Derive | Generated |
|--------|-----------|
| `Eq` | `==`, `!=` |
| `Ord` | `<`, `>`, `<=`, `>=`, `cmp` |
| `Hash` | `hash()` |
| `Debug` | `debug_fmt()` |
| `Display` | `fmt()` |
| `Duplicate` | `duplicate()` |
| `Default` | `default()` |
| `Serialize` | Serialization support |
| `Deserialize` | Deserialization support |

### 9.3 Custom Derive

```tml
decorator derive_builder {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Type(t) -> {
                let builder_type: Type = generate_builder_type(t)
                let builder_impl: Impl = generate_builder_impl(t)
                DecoratorResult.AddItems([builder_type, builder_impl])
            },
            _ -> DecoratorResult.Error("derive_builder only applies to types"),
        }
    }
}

@derive_builder
type Config = {
    host: String,
    port: U16,
    timeout: Duration,
}

// Generates ConfigBuilder with .host(), .port(), .timeout(), .build()
```

## 10. Validation Decorators

```tml
decorator validate_email {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Field(f) -> {
                if f.type != Type.String then {
                    return DecoratorResult.Error("@validate_email requires String field")
                }
                add_validation(f, EMAIL_REGEX)
            },
            _ -> DecoratorResult.Error("@validate_email only applies to fields"),
        }
    }
}

decorator validate_range(min: I64, max: I64) {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Field(f) -> add_range_check(f, min, max),
            Param(p) -> add_range_check(p, min, max),
            _ -> DecoratorResult.Error("@validate_range applies to fields and params"),
        }
    }
}

type User = {
    @validate_email
    email: String,

    @validate_range(0, 150)
    age: U8,
}
```

## 11. Testing Decorators

```tml
@test
func test_addition() {
    assert_eq(2 + 2, 4)
}

@test(name: "User creation with valid email")
func test_user_creation() {
    let user: User = User.new("test@example.com")
    assert(user.is_ok())
}

@test
@should_panic(expected: "division by zero")
func test_division_by_zero() {
    let _: I32 = 1 / 0
}

@test
@timeout(Duration.seconds(5))
async func test_slow_operation() {
    let result: T = slow_fetch().await
    assert(result.is_ok())
}

@bench(iterations: 1000)
func bench_sorting() {
    let data: List[I32] = generate_random_data(10000)
    data.sort()
}
```

## 12. Framework Integration

### 12.1 Web Framework Example

```tml
decorator route(method: HttpMethod, path: String) {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Func(f) -> {
                // Register route in framework
                router.register(method, path, f)
                DecoratorResult.Unchanged
            },
            _ -> DecoratorResult.Error("@route only applies to functions"),
        }
    }
}

decorator middleware(handler: func(Request, Next) -> Response) {
    // Add middleware to function
}

@route(HttpMethod.Get, "/users/{id}")
@middleware(auth_required)
func get_user(req: Request) -> Response {
    let id: String = req.params.get("id")!
    let user: User = db.find_user(id)!
    Response.json(user)
}
```

### 12.2 Dependency Injection

```tml
decorator inject {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Param(p) -> {
                // Mark parameter for injection
                p.metadata.insert("inject", true)
                DecoratorResult.Modified(Param(p))
            },
            _ -> DecoratorResult.Error("@inject only applies to parameters"),
        }
    }
}

func process(@inject db: Database, @inject logger: Logger, data: Data) {
    // db and logger are injected by the DI container
}
```

## 13. Compiler API

Decorators have access to compiler utilities:

```tml
decorator warn_unused_fields {
    func apply(target: DecoratorTarget) -> DecoratorResult {
        when target {
            Type(t) -> {
                for field in t.fields {
                    if not is_field_used(t, field) then {
                        compiler.warn(field.span, "unused field: " + field.name)
                    }
                }
                DecoratorResult.Unchanged
            },
            _ -> DecoratorResult.Error("applies to types only"),
        }
    }
}
```

### 13.1 Compiler Functions

| Function | Description |
|----------|-------------|
| `compiler.warn(span, msg)` | Emit warning |
| `compiler.error(span, msg)` | Emit error |
| `compiler.note(span, msg)` | Emit note |
| `compiler.type_of(expr)` | Get expression type |
| `compiler.span_of(node)` | Get source location |
| `compiler.resolve(path)` | Resolve path to definition |

## 14. IR Representation

### 14.1 Decorator Definition

```json
{
  "kind": "decorator_def",
  "id": "dec_abc123",
  "name": "retry",
  "params": [
    { "name": "max_attempts", "type": "U32", "default": 3 },
    { "name": "delay_ms", "type": "U32", "default": 1000 }
  ],
  "execution": "compile_time",
  "targets": ["func"],
  "apply_func": { ... }
}
```

### 14.2 Decorated Item

```json
{
  "kind": "func_def",
  "name": "fetch_data",
  "decorators": [
    {
      "name": "retry",
      "resolved": "my_module::retry",
      "args": {
        "max_attempts": 5,
        "delay_ms": 500
      },
      "span": { "file": "main.tml", "start": 10, "end": 35 }
    }
  ],
  "body": { ... }
}
```

## 15. Best Practices

### 15.1 Do

- Keep decorators focused on single responsibility
- Provide clear error messages for invalid usage
- Document expected targets and parameters
- Use compile-time decorators when possible
- Test decorator transformations

### 15.2 Don't

- Modify program semantics unexpectedly
- Create decorators with side effects at definition time
- Rely on decorator application order for correctness
- Generate overly complex code
- Use runtime decorators for compile-time concerns
