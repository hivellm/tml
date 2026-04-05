# ALWAYS use `let-else` instead of nested `when` for Maybe unwrapping

## Rule

When you need to unwrap a `Maybe[T]` and handle the `Nothing` case with an early exit (`return`, `continue`, `break`), use `let-else` instead of nested `when`.

## Anti-pattern (FORBIDDEN)

```tml
when parse(line) {
    Just(json) => {
        when json.get_string("event") {
            Just(ev) => {
                when json.get_string("tool") {
                    Just(tool) => {
                        // 4 levels deep just to read 3 fields
                        process(tool)
                    }, Nothing => {}
                }
            }, Nothing => {}
        }
    }, Nothing => {}
}
```

## Correct pattern

```tml
let Just(json) = parse(line) else { continue }
let Just(ev) = json.get_string("event") else { continue }
let Just(tool) = json.get_string("tool") else { continue }
process(tool)  // flat, readable
```

## When to use

- Unwrapping `Maybe[T]` in loops → `let Just(x) = expr else { continue }`
- Unwrapping `Maybe[T]` in functions → `let Just(x) = expr else { return }`
- Unwrapping `Outcome[T,E]` → `let Ok(x) = expr else { return Err(e) }`

## Also use `?.` for chaining

```tml
let name = parse(json_str)?.get_string("name")
// name is Maybe[Str] — Nothing if parse fails OR get_string fails
```