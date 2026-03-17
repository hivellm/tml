# Enums and Pattern Matching

An *enum* defines a type whose value is one of a fixed set of named variants. Where a struct
groups several values together simultaneously (a user has a name AND an email AND an age),
an enum expresses that a value is exactly one of several possibilities.

## Two Kinds of Enums

TML supports two forms of enum, which cover complementary use cases.

**Simple enums** — sometimes called C-style enums — have variants with no attached data.
They are useful when you need a named set of states or options:

```tml
type Direction = North | South | East | West
type Status = Pending | Running | Completed | Failed
```

**Algebraic data type (ADT) enums** — sometimes called sum types — have variants that each
carry their own payload. The payload can differ between variants:

```tml
type Shape =
    | Circle { radius: F64 }
    | Rectangle { width: F64, height: F64 }
    | Triangle { base: F64, height: F64 }
```

A `Shape` value is either a `Circle` (with a radius), or a `Rectangle` (with width and height),
or a `Triangle` (with base and height) — never more than one at a time.

## Enums and Pattern Matching Together

The real power of enums comes from combining them with the `when` expression, which lets you
inspect which variant you have and extract its data in a single, exhaustive operation:

```tml
func area(shape: Shape) -> F64 {
    when shape {
        Circle { radius } => 3.14159 * radius * radius,
        Rectangle { width, height } => width * height,
        Triangle { base, height } => 0.5 * base * height,
    }
}
```

The compiler ensures every variant is handled. If you add a new variant to `Shape` in the
future, every `when` expression on `Shape` will produce a compile error until the new case
is addressed. This exhaustiveness guarantee makes refactoring safer.

## What This Chapter Covers

- [Defining Enums](ch04-01-defining-enums.md) — simple enums, variants with data, tuple
  variants, generic enums, and recursive enums
- [Pattern Matching with When](ch04-02-pattern-matching.md) — the `when` expression,
  literal patterns, wildcard, bindings, destructuring, ranges, guards, and `if let`
