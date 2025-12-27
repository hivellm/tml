# Data Types

Every value in TML has a *data type*, which tells TML what kind of data
is being specified. TML is statically typed, meaning all types must be
known at compile time.

## Scalar Types

Scalar types represent a single value. TML has four primary scalar types:
integers, floating-point numbers, booleans, and characters.

### Integer Types

TML provides signed and unsigned integers of various sizes:

| Length | Signed | Unsigned |
|--------|--------|----------|
| 8-bit  | I8     | U8       |
| 16-bit | I16    | U16      |
| 32-bit | I32    | U32      |
| 64-bit | I64    | U64      |
| 128-bit| I128   | U128     |

The default integer type is `I32`:

```tml
func main() {
    let x = 42        // I32 by default
    let y: I64 = 42   // Explicit I64
    println(x)
}
```

### Integer Literals

You can write integers in different bases:

```tml
func main() {
    let decimal = 98_222      // Decimal (underscores for readability)
    let hex = 0xFF            // Hexadecimal
    let octal = 0o77          // Octal
    let binary = 0b1111_0000  // Binary

    println(decimal)  // 98222
    println(hex)      // 255
    println(binary)   // 240
}
```

### Floating-Point Types

TML has two floating-point types:

| Type | Size | Precision |
|------|------|-----------|
| F32  | 32-bit | ~6-7 decimal digits |
| F64  | 64-bit | ~15-16 decimal digits |

```tml
func main() {
    let x = 2.0       // F64 by default
    let y: F32 = 3.0  // Explicit F32
    println(x)
}
```

### Boolean Type

The boolean type `Bool` has two values: `true` and `false`:

```tml
func main() {
    let t = true
    let f = false
    println(t)  // true
    println(f)  // false
}
```

### Character Type

The `Char` type represents a Unicode scalar value:

```tml
func main() {
    let c = 'z'
    let emoji = '🦀'
    println(c)
}
```

## String Type

Strings in TML are represented by the `Str` type:

```tml
func main() {
    let greeting = "Hello, World!"
    println(greeting)
}
```

### String Interpolation

TML supports string interpolation with `{expression}` syntax:

```tml
func main() {
    let name = "Alice"
    let age = 30

    // Basic interpolation
    let greeting = "Hello, {name}!"
    println(greeting)  // Hello, Alice!

    // Expressions inside braces
    let message = "{name} is {age} years old"
    println(message)  // Alice is 30 years old

    // Complex expressions
    let calc = "Sum: {2 + 3}, Product: {4 * 5}"
    println(calc)  // Sum: 5, Product: 20
}
```

To include a literal `{` or `}` in a string, escape them with a backslash:

```tml
func main() {
    let json = "\{\"name\": \"{name}\"\}"
    println(json)  // {"name": "Alice"}
}
```

## Type Annotations

While TML can often infer types, you can add explicit annotations:

```tml
func main() {
    let x: I32 = 5
    let y: F64 = 3.14
    let active: Bool = true
    let name: Str = "TML"
}
```

## Numeric Operations

TML supports standard arithmetic operations:

```tml
func main() {
    // Addition
    let sum = 5 + 10
    println(sum)  // 15

    // Subtraction
    let difference = 95 - 4
    println(difference)  // 91

    // Multiplication
    let product = 4 * 30
    println(product)  // 120

    // Division
    let quotient = 56 / 8
    println(quotient)  // 7

    // Remainder (modulo)
    let remainder = 43 % 5
    println(remainder)  // 3
}
```

## Type Inference

TML's type inference is powerful. The compiler can usually figure out
the type from context:

```tml
func main() {
    let x = 5          // I32
    let y = 5.0        // F64
    let z = true       // Bool
    let s = "hello"    // Str
}
```
