# Data Types

Every value in TML has a type. The compiler knows all types at compile time — TML is statically
typed. This catches entire classes of bugs before the program runs and enables the compiler to
generate efficient code.

Types in TML fall into two categories: scalar types (a single value) and compound types (values
composed of other values).

## Scalar Types

### Integers

TML provides signed and unsigned integers in six sizes:

| Signed | Unsigned | Bit width | Value range (signed) |
|--------|----------|-----------|----------------------|
| `I8`   | `U8`     | 8         | −128 to 127 |
| `I16`  | `U16`    | 16        | −32,768 to 32,767 |
| `I32`  | `U32`    | 32        | −2,147,483,648 to 2,147,483,647 |
| `I64`  | `U64`    | 64        | −9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |
| `I128` | `U128`   | 128       | very large |

Integer literals default to `I32` when the type cannot be inferred from context:

```tml
let a = 42        // I32 (default)
let b: I64 = 42   // explicit I64
let c: U8  = 255  // explicit U8
```

You can use underscores in numeric literals to improve readability:

```tml
let million: I32 = 1_000_000
let mask: U32    = 0xFF_FF_FF_FF
```

Integer literals can be written in decimal, hexadecimal, octal, or binary:

```tml
let dec = 255           // decimal
let hex = 0xFF          // hexadecimal — same value
let oct = 0o377         // octal — same value
let bin = 0b1111_1111   // binary — same value
```

**Choosing a type:** Use `I32` for general-purpose integers. Use `I64` when values may exceed
±2 billion. Use `U8` for raw bytes. Prefer signed types unless the value is inherently
non-negative and overflow behavior matters.

### Floating-Point Numbers

TML has two floating-point types:

| Type | Bit width | Precision |
|------|-----------|-----------|
| `F32` | 32 | ~7 significant decimal digits |
| `F64` | 64 | ~15 significant decimal digits |

Floating-point literals default to `F64`:

```tml
let x = 3.14       // F64 (default)
let y: F32 = 1.5   // explicit F32
```

Use `F64` for most calculations. Use `F32` when working with large arrays where memory usage
matters, or when interoperating with APIs that require single precision.

### Booleans

The `Bool` type has exactly two values: `true` and `false`.

```tml
let active: Bool = true
let done:   Bool = false
```

Boolean values appear in conditions:

```tml
let x: I32 = 10
if x > 5 {
    println("greater")
}
```

TML does not perform implicit numeric-to-boolean conversion. Writing `if x { ... }` where `x`
is an integer is a compile error. You must write an explicit comparison: `if x != 0 { ... }`.

Logical operators are keywords, not symbols:

```tml
let both   = true and false   // false
let either = true or false    // true
let flipped = not true        // false
```

### Characters

The `Char` type represents a single Unicode scalar value, written with single quotes:

```tml
let letter:  Char = 'A'
let digit:   Char = '7'
let newline: Char = '\n'
let tab:     Char = '\t'
```

`Char` is a 32-bit value capable of holding any Unicode scalar value.

## String Types

### `Str` — Immutable Strings

`Str` is the primary string type. A `Str` value is an immutable, UTF-8 encoded string:

```tml
let greeting: Str = "Hello, World!"
```

String literals use double quotes. Common escape sequences:

| Escape | Character |
|--------|-----------|
| `\\`   | Backslash |
| `\"`   | Double quote |
| `\n`   | Newline |
| `\t`   | Tab |
| `\r`   | Carriage return |

### String Interpolation

Embed expressions inside a string using `${}`:

```tml
func main() {
    let name: Str = "Alice"
    let age: I32  = 30

    let greeting = "Hello, ${name}!"
    let info     = "${name} is ${age.to_string()} years old."

    println(greeting)  // Hello, Alice!
    println(info)      // Alice is 30 years old.
}
```

The expression inside `${}` must produce a value that can be displayed. Simple types like `Str`,
`I32`, `F64`, and `Bool` work directly. For other types, call `.to_string()` explicitly:

```tml
let pi: F64 = 3.14159
let msg = "Pi is approximately ${pi.to_string()}"
```

String concatenation uses `+`:

```tml
let first: Str = "Hello"
let last:  Str = "World"
let full       = first + ", " + last + "!"
println(full)  // Hello, World!
```

## The Unit Type

`Unit` is the type of expressions that produce no meaningful value. It is written as `()`:

```tml
let nothing: Unit = ()
```

Functions that do not return a value implicitly return `Unit`. You will rarely write `Unit`
explicitly, but it appears in function signatures and type signatures for completeness.

## Compound Types

### Tuples

A tuple groups a fixed number of values of potentially different types:

```tml
let pair:   (I32, Str)        = (42, "hello")
let triple: (Bool, F64, I32)  = (true, 3.14, 7)
```

Access tuple elements by index with `.0`, `.1`, and so on:

```tml
let pair = (42, "hello")
let n    = pair.0   // 42
let s    = pair.1   // "hello"
```

Tuples are useful for returning multiple values from a function without defining a named struct:

```tml
func divide(a: I32, b: I32) -> (I32, I32) {
    return (a / b, a % b)
}

func main() {
    let result    = divide(17, 5)
    let quotient  = result.0   // 3
    let remainder = result.1   // 2
}
```

Tuples have a fixed length that cannot change after the binding is created.

### Fixed-Size Arrays

An array holds a fixed number of values of the same type, stored contiguously on the stack:

```tml
let scores: [I32; 3] = [10, 20, 30]
```

The type `[I32; 3]` means "an array of three `I32` values." Both the element type and the
length are part of the type — `[I32; 3]` and `[I32; 4]` are different types.

Access elements with an index:

```tml
let scores = [10, 20, 30]
let first  = scores[0]   // 10
let last   = scores[2]   // 30
```

TML checks array bounds at runtime. Accessing an out-of-bounds index causes a panic rather than
undefined behavior.

Arrays are best for a small, fixed number of items known at compile time. For a collection that
grows at runtime, use `List[T]` from the standard library.

## Type Casting

TML does not implicitly convert between numeric types. You must cast explicitly using `as`:

```tml
let x: I32 = 42
let y: I64 = x as I64
let f: F64 = x as F64
```

Casting truncates when the target type is smaller:

```tml
let big: I32 = 300
let small: U8 = big as U8   // 44 — truncated (300 % 256)
```

Cast only when you understand the range of the source value. For conversions that can fail, the
standard library provides checked conversion methods.

## Type Inference

TML infers types from context, so you rarely need to write every annotation. The compiler uses
the value on the right side of a binding, the return type of a function, or the expected type at
a call site:

```tml
let x    = 42        // I32 — default integer type
let y    = 3.14      // F64 — default float type
let name = "Alice"   // Str
let ok   = true      // Bool
```

When inference is ambiguous or produces the wrong type, add an explicit annotation:

```tml
let index: U64 = 0   // without annotation, inferred as I32
```

Good code uses annotations where they add clarity and omits them where the type is obvious from
the value.

## Summary

| Type category | Types |
|---------------|-------|
| Signed integers | `I8`, `I16`, `I32`, `I64`, `I128` |
| Unsigned integers | `U8`, `U16`, `U32`, `U64`, `U128` |
| Floating-point | `F32`, `F64` |
| Boolean | `Bool` |
| Character | `Char` |
| String | `Str` |
| Unit (no value) | `Unit` |
| Tuple | `(T1, T2, ...)` |
| Fixed-size array | `[T; N]` |
