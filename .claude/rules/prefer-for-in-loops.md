# ALWAYS use `for i in 0 to N` instead of manual index loops

## Rule

When iterating over a range of integers, use `for-in` with `to` (exclusive) or `through` (inclusive) instead of manual `var i = 0; loop (i < N) { ...; i = i + 1 }`.

## Anti-pattern (FORBIDDEN)

```tml
var i: I64 = 0
loop (i < items.len()) {
    let item = items.get(i)
    process(item)
    i = i + 1
}
```

## Correct pattern

```tml
for i in 0 to items.len() {
    let item = items.get(i)
    process(item)
}
```

## Variants

- **Exclusive range**: `for i in 0 to 10` → 0, 1, ..., 9
- **Inclusive range**: `for i in 1 through 5` → 1, 2, 3, 4, 5
- **Non-zero start**: `for i in start to end { ... }`

## Why

Manual index loops are error-prone (forgotten increment → infinite loop) and obscure intent. `for-in` is:
- Shorter (1 line vs 3)
- Safer (no forgotten `i = i + 1`)
- Clearer about iteration bounds
