# Maybe Handling — let-else and Optional Chaining

This chapter covers TML's syntactic sugar for working with `Maybe[T]` values without deeply nested pattern matching.

## The Problem

Working with `Maybe[T]` values often creates cascading `when` blocks:

```tml
when parse(line) {
    Just(json) => {
        when json.get_string("event") {
            Just(ev) => {
                when json.get_string("tool") {
                    Just(tool) => {
                        process(tool)  // 4 levels deep!
                    }, Nothing => {}
                }
            }, Nothing => {}
        }
    }, Nothing => {}
}
```

This is unreadable. TML provides two solutions.

## Solution 1: `let-else` Guard Clause

The `let-else` statement unwraps a `Maybe[T]` or executes a diverging block:

```tml
let Just(json) = parse(line) else { continue }
let Just(ev) = json.get_string("event") else { continue }
let Just(tool) = json.get_string("tool") else { continue }
process(tool)  // flat, readable!
```

### Syntax

```tml
let Just(variable) = maybe_expression else { diverging_statement }
```

- The `else` block **must diverge**: `return`, `continue`, `break`, or `panic()`
- Type annotation is optional — inferred from the expression
- Works with any refutable pattern, not just `Just`

### Examples

```tml
// In a loop — skip invalid entries
loop (reader.has_next()) {
    let line = reader.read_line()
    let Just(json) = parse(line) else { continue }
    let Just(name) = json.get_string("name") else { continue }
    println(name)
}

// In a function — return early
func get_user_name(id: I64) -> Maybe[Str] {
    let Just(user) = db.find_user(id) else { return Nothing }
    let Just(name) = user.get_string("name") else { return Nothing }
    return Just(name)
}
```

## Solution 2: `?.` Optional Chaining

The `?.` operator calls a method on the inner value of a `Maybe[T]`:

```tml
let name = parse(json_str)?.get_string("name")
// name is Maybe[Str]
// Nothing if parse() returns Nothing OR get_string() returns Nothing
```

### Semantics

- `expr?.method(args)` — if `expr` is `Nothing`, returns `Nothing`. If `Just(v)`, calls `v.method(args)` and wraps in `Maybe`.
- **Auto-flattening**: if the method returns `Maybe[V]`, the result is `Maybe[V]` (not `Maybe[Maybe[V]]`).
- **Chaining**: `a?.b()?.c()` propagates `Nothing` through the entire chain.

### Examples

```tml
// JSON field access
let age = parse(json_str)?.get_i64("age")

// Chain multiple calls
let city = parse(json_str)?.get_string("address")?.get_string("city")

// Combine with let-else for flat code
let Just(name) = parse(json_str)?.get_string("name") else { return }
println("Hello, " + name)
```

## Solution 3: Combinators

`Maybe[T]` has 34 methods for functional-style transformations:

```tml
// map: transform the inner value
let upper_name = parse(str)?.get_string("name").map(do(n) n.to_upper())

// and_then: chain Maybe-returning operations
let result = parse(str).and_then(do(json) json.get_string("name"))

// unwrap_or: provide a default
let name = parse(str)?.get_string("name").unwrap_or("anonymous")

// filter: keep only matching values
let adult_age = parse(str)?.get_i64("age").filter(do(a) a >= 18)
```

## When to Use Which

| Situation | Use |
|-----------|-----|
| Unwrap + early exit in a loop | `let Just(x) = expr else { continue }` |
| Unwrap + early return in a function | `let Just(x) = expr else { return }` |
| Chain method calls on Maybe | `expr?.method()` |
| Transform the inner value | `.map(do(x) ...)` |
| Chain Maybe-returning operations | `.and_then(do(x) ...)` |
| Provide a default value | `.unwrap_or(default)` |
