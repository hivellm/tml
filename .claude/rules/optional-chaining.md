# Use `?.` optional chaining for Maybe method calls

## Rule

When calling a method on a `Maybe[T]` value, use `?.` instead of unwrapping first.

## Pattern

```tml
// GOOD: optional chaining
let name = parse(json_str)?.get_string("name")
let age = parse(json_str)?.get_i64("age")

// BAD: nested when
when parse(json_str) {
    Just(json) => {
        when json.get_string("name") {
            Just(name) => { ... },
            Nothing => {}
        }
    },
    Nothing => {}
}
```

## Semantics

- `expr?.method(args)` — if `expr` is `Nothing`, returns `Nothing`. If `Just(v)`, calls `v.method(args)` and wraps in `Maybe`.
- Auto-flattening: if the method returns `Maybe[V]`, result is `Maybe[V]` (not `Maybe[Maybe[V]]`).
- Chain multiple: `a?.b()?.c()` propagates `Nothing` through the chain.
- Works with field access too: `expr?.field`

## When to use

- JSON parsing: `parse(str)?.get_string("key")`
- Database queries: `conn.query(sql)?.get_row(0)?.get_string("name")`
- HTTP responses: `response?.body()?.as_json()`
- Any method call on a Maybe value