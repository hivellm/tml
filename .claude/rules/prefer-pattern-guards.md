# Use pattern guards instead of nested when for conditional matching

## Rule

When a `when` arm needs an additional condition after pattern matching, use `if` guards instead of nesting another `when` or `if` inside the arm body.

## Anti-pattern (FORBIDDEN)

```tml
when value {
    Just(x) => {
        if x > 0 {
            handle_positive(x)
        } else {
            handle_nonpositive(x)
        }
    },
    Nothing => handle_missing(),
}
```

## Correct pattern

```tml
when value {
    Just(x) if x > 0 => handle_positive(x),
    Just(x) => handle_nonpositive(x),
    Nothing => handle_missing(),
}
```

## When to use

- Filtering enum variants by a condition on the inner value
- Range-checking extracted values
- Combining pattern matching with boolean predicates
