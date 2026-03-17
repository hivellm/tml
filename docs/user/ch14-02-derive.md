# Derive Macros

The `@derive` decorator instructs the compiler to automatically implement one or more
standard behaviors for a type. Instead of writing boilerplate method implementations by
hand, you name the behaviors you want and the compiler generates correct, efficient code:

```tml
@derive(PartialEq, Hash, Debug)
type Color {
    r: U8,
    g: U8,
    b: U8,
}
```

The generated code is identical to what you would write manually. It is fully visible to
the optimizer and participates in all normal type checking.

## Syntax

```
@derive(BehaviorName1, BehaviorName2, ...)
type TypeName { ... }
```

All named behaviors are derived in the order listed. When one derived behavior depends on
another — for example, `Ord` requires `PartialOrd`, which requires `PartialEq` — list the
dependencies first:

```tml
@derive(PartialEq, PartialOrd, Ord)
type Version {
    major: I32,
    minor: I32,
    patch: I32,
}
```

## Derivable Behaviors

### `PartialEq`

Generates an `eq(other: ref Self) -> Bool` method that compares all fields in declaration
order. Two values are equal if and only if every corresponding pair of fields is equal.

**Before derive:**
```tml
type Point { x: I32, y: I32 }

extend Point : PartialEq {
    func eq(this, other: ref Point) -> Bool {
        return this.x == other.x and this.y == other.y
    }
}
```

**After derive:**
```tml
@derive(PartialEq)
type Point { x: I32, y: I32 }
```

The generated implementation is equivalent. Fields that do not implement `PartialEq` cause
a compile error on the `@derive` line.

For enum types with data variants, the derived implementation first compares the variant
discriminant and then compares the payload of matching variants.

### `Eq`

A marker behavior indicating that equality is reflexive, symmetric, and transitive with no
exceptional values (unlike `F64`, which has `NaN != NaN`). `Eq` requires `PartialEq`:

```tml
@derive(PartialEq, Eq)
type Status { code: I32 }
```

`Eq` generates no methods. It is a guarantee to the type system and to generic code that
relies on total equality (such as `HashMap` keys).

### `PartialOrd`

Generates a `partial_cmp(other: ref Self) -> Maybe[Ordering]` method that compares fields
lexicographically. Requires `PartialEq`:

```tml
@derive(PartialEq, PartialOrd)
type Score { value: F64 }
```

The comparison proceeds field by field in declaration order. The first field that differs
determines the result. If all fields are equal, `Just(Ordering::Equal)` is returned.
`Nothing` is returned only if any field's `partial_cmp` returns `Nothing` (as happens with
`F64` when either value is `NaN`).

### `Ord`

Generates a `cmp(other: ref Self) -> Ordering` method that provides a total ordering.
Requires `PartialOrd` and `Eq`. Use `Ord` for types that will be stored in sorted
collections (`BTreeMap`, `BTreeSet`) or compared with `min`/`max`:

```tml
@derive(PartialEq, Eq, PartialOrd, Ord)
type Timestamp {
    seconds: I64,
    nanos:   U32,
}
```

The lexicographic comparison follows the same field order as `PartialOrd`. Fields must
themselves implement `Ord`.

### `Hash`

Generates a `hash(hasher: mut ref Hasher)` method that feeds all fields into the provided
hasher. Required for using a type as a key in `HashMap` or `HashSet`. Requires `Eq`:

```tml
@derive(PartialEq, Eq, Hash)
type UserId { value: U64 }
```

The generated `hash` implementation calls `hasher.write_*` for each field in declaration
order. Fields must themselves implement `Hash`.

If you implement `PartialEq` manually, you must also implement `Hash` manually so that the
invariant `a == b implies hash(a) == hash(b)` is preserved.

### `Debug`

Generates a `debug_string() -> Str` method suitable for debugging output. The format is
`TypeName { field: value, field: value }` for structs and `VariantName(value)` for enums:

```tml
@derive(Debug)
type Rectangle {
    width:  F64,
    height: F64,
}

func main() {
    let r = Rectangle { width: 3.0, height: 4.0 }
    println("{}", r.debug_string())
    // Prints: Rectangle { width: 3.0, height: 4.0 }
}
```

Fields must implement `Debug`. The output format is intended for developer inspection, not
user presentation. For user-facing strings, derive `Display` instead.

### `Display`

Generates a `to_string() -> Str` method. Unlike `Debug`, there is no default format: the
generated output is the same field-by-field representation, but the intent is a
human-readable summary. For custom formatting, implement `Display` manually rather than
deriving it:

```tml
@derive(Display)
type Color { r: U8, g: U8, b: U8 }

func main() {
    let c = Color { r: 255, g: 128, b: 0 }
    println(c.to_string())
    // Prints: Color { r: 255, g: 128, b: 0 }
}
```

If you want output like `"rgb(255, 128, 0)"`, implement `to_string()` manually in an
`extend` block instead of using `@derive(Display)`.

### `Default`

Generates a `default() -> Self` static method that constructs a value using each field's
own `Default` implementation. All fields must implement `Default`:

```tml
@derive(Default)
type Config {
    timeout_ms: I32,     // defaults to 0
    max_retries: I32,    // defaults to 0
    verbose: Bool,       // defaults to false
    host: Str,           // defaults to ""
}

func main() {
    let cfg = Config::default()
    // cfg.timeout_ms == 0, cfg.verbose == false, etc.
}
```

Primitive types have the following defaults: integers `0`, floats `0.0`, `Bool` `false`,
`Str` `""`. For fields of struct types, those types must also implement `Default`.

To provide non-zero defaults for specific fields, implement `default()` manually:

```tml
extend Config {
    func default() -> Config {
        return Config {
            timeout_ms: 5000,    // 5 second default
            max_retries: 3,
            verbose: false,
            host: "localhost",
        }
    }
}
```

### `Duplicate`

Generates a `duplicate() -> Self` method that performs a deep copy of the value. All fields
must implement `Duplicate`. Use this when you need an independent copy of a heap-allocated
type:

```tml
@derive(Duplicate)
type Buffer {
    data: List[U8],
    name: Str,
}

func main() {
    let original = Buffer { data: List::from([1u8, 2, 3]), name: "buf" }
    let copy = original.duplicate()
    // copy.data is an independent list; modifying it does not affect original
}
```

`Duplicate` is the TML equivalent of Rust's `Clone`. For types where bitwise copy is
sufficient (all fields are plain values with no heap allocation), use `Copy` instead.

### `Copy`

A marker behavior indicating that the type can be duplicated by simple bitwise copy, with no
special logic required. The compiler handles `Copy` types automatically without calling any
method. All fields must themselves be `Copy`:

```tml
@derive(Copy, Duplicate)
type Vec2 { x: F32, y: F32 }
```

`Copy` implies `Duplicate`: a `Copy` type is always duplicable. The reverse is not true.
Types containing heap-allocated fields (such as `Str` or `List[T]`) cannot be `Copy`.

---

## Combining Derives

Most real types need several behaviors together. The most common combination for a plain
data type is:

```tml
@derive(PartialEq, Eq, Hash, Debug, Duplicate)
type Point {
    x: I32,
    y: I32,
}
```

This makes `Point` comparable, hashable (usable as a `HashMap` key), debuggable, and
explicitly clonable. Add `Ord` and `PartialOrd` if the type will be sorted.

For value types with no heap allocation:

```tml
@derive(PartialEq, Eq, Hash, Debug, Copy, Duplicate)
type Color { r: U8, g: U8, b: U8 }
```

For enum types:

```tml
@derive(PartialEq, Eq, Hash, Debug, Duplicate)
enum Direction { North, South, East, West }
```

---

## What Derive Does Not Generate

`@derive` generates implementations of the named behavior using a standard algorithm based
on fields. It does not generate:

- Methods beyond those required by the behavior's interface
- Constructors (use `extend` to write `new()` or `default()` if `Default` is insufficient)
- Conversions to or from other types (use `From`/`Into` implementations in `extend`)
- Custom display formats (derive `Display` only if the default format is acceptable)

---

## Custom Derive

It is possible to define your own derivable behaviors using the `decorator` keyword and code
generation facilities. This is covered in [Custom Decorators](ch14-03-custom-decorators.md).

---

## See Also

- [Common Behaviors](ch05-02-common-behaviors.md) — the full behavior definitions that
  `@derive` implements
- [Custom Decorators](ch14-03-custom-decorators.md) — writing your own `@derive`-compatible
  code generators
- [Built-in Decorators](ch14-01-builtin-decorators.md) — all other built-in decorators
