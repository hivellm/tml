# Use `..base` struct update syntax to avoid redundant field copies

## Rule

When constructing a struct where most fields come from an existing value and only a few change, use `..base` instead of copying every field manually.

## Anti-pattern (FORBIDDEN)

```tml
let config2 = Config {
    debug: true,
    verbose: config1.verbose,
    level: config1.level,
    timeout: config1.timeout,
    retries: config1.retries,
}
```

## Correct pattern

```tml
let config2 = Config { debug: true, ..config1 }
```

## When to use

- Builder-style methods that change one field and copy the rest
- Creating variants of a struct (e.g., struct registration where `is_union: 1` differs from default)
- Test fixtures with small variations from a base object
