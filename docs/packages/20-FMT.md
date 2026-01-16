# TML Standard Library: Formatting

> `std.fmt` — String formatting and display traits.

## Overview

The fmt package provides traits and utilities for formatting values as strings. It supports type-safe formatting with compile-time checked format strings.

## Import

```tml
use std::fmt
use std::fmt.{format, Display, Debug, Formatter}
```

---

## Format Macro

```tml
/// Formats a string with arguments
public macro format! {
    ($fmt:literal $(, $arg:expr)*) => {
        // Compile-time checked formatting
    }
}

/// Prints formatted string to stdout
public macro print! {
    ($fmt:literal $(, $arg:expr)*) => {
        io.stdout().write_all(format!($fmt $(, $arg)*).as_bytes()).unwrap()
    }
}

/// Prints formatted string with newline
public macro println! {
    ($fmt:literal $(, $arg:expr)*) => {
        io.stdout().write_all((format!($fmt $(, $arg)*) + "\n").as_bytes()).unwrap()
    }
}

/// Prints formatted string to stderr
public macro eprint! {
    ($fmt:literal $(, $arg:expr)*) => {
        io.stderr().write_all(format!($fmt $(, $arg)*).as_bytes()).unwrap()
    }
}

/// Prints formatted string to stderr with newline
public macro eprintln! {
    ($fmt:literal $(, $arg:expr)*) => {
        io.stderr().write_all((format!($fmt $(, $arg)*) + "\n").as_bytes()).unwrap()
    }
}

/// Writes formatted string to a writer
public macro write! {
    ($dst:expr, $fmt:literal $(, $arg:expr)*) => {
        $dst.write_fmt(format_args!($fmt $(, $arg)*))
    }
}

/// Writes formatted string with newline
public macro writeln! {
    ($dst:expr, $fmt:literal $(, $arg:expr)*) => {
        $dst.write_fmt(format_args!($fmt + "\n" $(, $arg)*))
    }
}
```

---

## Format String Syntax

```
format_string := text | '{' argument '}'
argument := position? ':' format_spec?
position := integer | identifier
format_spec := [[fill]align][sign]['#']['0'][width]['.' precision][type]

fill := character
align := '<' | '^' | '>'
sign := '+' | '-'
width := integer | identifier
precision := integer | identifier | '*'
type := '' | '?' | 'x' | 'X' | 'b' | 'o' | 'e' | 'E' | 'p'
```

### Examples

```tml
// Positional arguments
format!("{} {}", "hello", "world")        // "hello world"
format!("{0} {1} {0}", "a", "b")          // "a b a"

// Named arguments
format!("{name} is {age}", name = "Alice", age = 30)

// Debug formatting
format!("{:?}", some_value)               // Debug output
format!("{:#?}", some_value)              // Pretty debug

// Width and alignment
format!("{:10}", "hello")                 // "hello     " (left, default for strings)
format!("{:>10}", "hello")                // "     hello" (right)
format!("{:^10}", "hello")                // "  hello   " (center)
format!("{:*^10}", "hello")               // "**hello***" (center with fill)

// Numbers
format!("{:+}", 42)                       // "+42" (always show sign)
format!("{:05}", 42)                      // "00042" (zero-padded)
format!("{:8.3}", 3.14159)                // "   3.142" (width.precision)

// Alternate forms
format!("{:#x}", 255)                     // "0xff" (hex with prefix)
format!("{:#b}", 10)                      // "0b1010" (binary with prefix)
format!("{:#o}", 64)                      // "0o100" (octal with prefix)

// Scientific notation
format!("{:e}", 1234.5)                   // "1.2345e3"
format!("{:E}", 1234.5)                   // "1.2345E3"

// Pointer
format!("{:p}", ref value)                   // "0x7fff5fbff8b8"
```

---

## Core Traits

### Display

Human-readable formatting.

```tml
/// Format a value for display
pub behavior Display {
    func fmt(this, f: mut ref Formatter) -> FmtResult
}

// Auto-implemented for primitives
implement Display for I32 { ... }
implement Display for String { ... }
implement Display for Bool { ... }
// etc.
```

### Debug

Programmer-readable formatting.

```tml
/// Format a value for debugging
pub behavior Debug {
    func fmt(this, f: mut ref Formatter) -> FmtResult
}

// Can be derived
@derive(Debug)
type Point {
    x: I32,
    y: I32,
}
// Formats as: Point { x: 10, y: 20 }
```

### Other Format Traits

```tml
/// Binary formatting
pub behavior Binary {
    func fmt(this, f: mut ref Formatter) -> FmtResult
}

/// Octal formatting
pub behavior Octal {
    func fmt(this, f: mut ref Formatter) -> FmtResult
}

/// Lowercase hex formatting
pub behavior LowerHex {
    func fmt(this, f: mut ref Formatter) -> FmtResult
}

/// Uppercase hex formatting
pub behavior UpperHex {
    func fmt(this, f: mut ref Formatter) -> FmtResult
}

/// Lowercase exponential formatting
pub behavior LowerExp {
    func fmt(this, f: mut ref Formatter) -> FmtResult
}

/// Uppercase exponential formatting
pub behavior UpperExp {
    func fmt(this, f: mut ref Formatter) -> FmtResult
}

/// Pointer formatting
pub behavior Pointer {
    func fmt(this, f: mut ref Formatter) -> FmtResult
}
```

---

## Formatter

```tml
/// Formatter configuration and output buffer
pub type Formatter {
    buf: mut ref dyn Write,
    flags: FormatFlags,
    width: Maybe[U64],
    precision: Maybe[U64],
    fill: Char,
    align: Alignment,
}

pub type Alignment = Left | Center | Right

pub type FormatFlags {
    alternate: Bool,      // #
    sign_plus: Bool,      // +
    sign_minus: Bool,     // -
    zero_pad: Bool,       // 0
}

extend Formatter {
    /// Writes a string
    pub func write_str(mut this, s: ref String) -> FmtResult

    /// Writes a single character
    pub func write_char(mut this, c: Char) -> FmtResult

    /// Writes formatted arguments
    pub func write_fmt(mut this, args: Arguments) -> FmtResult

    /// Returns the width, if specified
    pub func width(this) -> Maybe[U64] { this.width }

    /// Returns the precision, if specified
    pub func precision(this) -> Maybe[U64] { this.precision }

    /// Returns true if alternate format was requested
    pub func alternate(this) -> Bool { this.flags.alternate }

    /// Returns true if sign should always be shown
    pub func sign_plus(this) -> Bool { this.flags.sign_plus }

    /// Returns true if zero-padding was requested
    pub func zero_pad(this) -> Bool { this.flags.zero_pad }

    /// Returns the fill character
    pub func fill(this) -> Char { this.fill }

    /// Returns the alignment
    pub func align(this) -> Alignment { this.align }

    /// Pads the output to the specified width
    pub func pad(mut this, s: ref String) -> FmtResult {
        when this.width {
            Just(width) if s.len() < width -> {
                let padding = width - s.len()
                when this.align {
                    Left -> {
                        this.write_str(s)?
                        loop _ in 0 to padding {
                            this.write_char(this.fill)?
                        }
                    },
                    Right -> {
                        loop _ in 0 to padding {
                            this.write_char(this.fill)?
                        }
                        this.write_str(s)?
                    },
                    Center -> {
                        let left = padding / 2
                        let right = padding - left
                        loop _ in 0 to left {
                            this.write_char(this.fill)?
                        }
                        this.write_str(s)?
                        loop _ in 0 to right {
                            this.write_char(this.fill)?
                        }
                    },
                }
            },
            _ -> this.write_str(s),
        }
    }

    /// Helper for debug formatting
    pub func debug_struct(mut this, name: ref String) -> DebugStruct {
        DebugStruct.new(this, name)
    }

    /// Helper for debug tuple
    pub func debug_tuple(mut this, name: ref String) -> DebugTuple {
        DebugTuple.new(this, name)
    }

    /// Helper for debug list
    pub func debug_list(mut this) -> DebugList {
        DebugList.new(this)
    }

    /// Helper for debug map
    pub func debug_map(mut this) -> DebugMap {
        DebugMap.new(this)
    }

    /// Helper for debug set
    pub func debug_set(mut this) -> DebugSet {
        DebugSet.new(this)
    }
}

/// Format result
pub type FmtResult = Outcome[Unit, FmtError]

/// Format error
pub type FmtError
```

---

## Debug Helpers

### DebugStruct

```tml
/// Helper for formatting structs
pub type DebugStruct {
    formatter: mut ref Formatter,
    name: String,
    has_fields: Bool,
}

extend DebugStruct {
    /// Adds a field
    pub func field(mut this, name: ref String, value: ref impl Debug) -> mut ref DebugStruct

    /// Finishes the struct
    pub func finish(mut this) -> FmtResult
}

// Usage in Debug implementation
implement Debug for Point {
    func fmt(this, f: mut ref Formatter) -> FmtResult {
        f.debug_struct("Point")
            .field("x", ref this.x)
            .field("y", ref this.y)
            .finish()
    }
}
// Output: Point { x: 10, y: 20 }
// Pretty: Point {
//     x: 10,
//     y: 20,
// }
```

### DebugTuple

```tml
/// Helper for formatting tuple structs
pub type DebugTuple {
    formatter: mut ref Formatter,
    name: String,
    has_fields: Bool,
}

extend DebugTuple {
    /// Adds a field
    pub func field(mut this, value: ref impl Debug) -> mut ref DebugTuple

    /// Finishes the tuple
    pub func finish(mut this) -> FmtResult
}

// Usage
type Color(U8, U8, U8)

implement Debug for Color {
    func fmt(this, f: mut ref Formatter) -> FmtResult {
        f.debug_tuple("Color")
            .field(ref this.0)
            .field(ref this.1)
            .field(ref this.2)
            .finish()
    }
}
// Output: Color(255, 128, 0)
```

### DebugList

```tml
/// Helper for formatting lists
pub type DebugList {
    formatter: mut ref Formatter,
    has_entries: Bool,
}

extend DebugList {
    /// Adds an entry
    pub func entry(mut this, value: ref impl Debug) -> mut ref DebugList

    /// Adds multiple entries
    pub func entries[I](mut this, entries: I) -> mut ref DebugList
        where I: Iterator, I.Item: Debug

    /// Finishes the list
    pub func finish(mut this) -> FmtResult
}

// Usage
implement Debug for Vec[T] where T: Debug {
    func fmt(this, f: mut ref Formatter) -> FmtResult {
        f.debug_list().entries(this.iter()).finish()
    }
}
// Output: [1, 2, 3]
```

### DebugMap

```tml
/// Helper for formatting maps
pub type DebugMap {
    formatter: mut ref Formatter,
    has_entries: Bool,
}

extend DebugMap {
    /// Adds an entry
    pub func entry(mut this, key: ref impl Debug, value: ref impl Debug) -> mut ref DebugMap

    /// Adds multiple entries
    pub func entries[I, K, V](mut this, entries: I) -> mut ref DebugMap
        where I: Iterator[Item = (K, V)], K: Debug, V: Debug

    /// Finishes the map
    pub func finish(mut this) -> FmtResult
}

// Usage
implement Debug for HashMap[K, V] where K: Debug, V: Debug {
    func fmt(this, f: mut ref Formatter) -> FmtResult {
        f.debug_map().entries(this.iter()).finish()
    }
}
// Output: {"key": "value", "foo": "bar"}
```

---

## Implementing Display

```tml
type Person {
    name: String,
    age: U32,
}

implement Display for Person {
    func fmt(this, f: mut ref Formatter) -> FmtResult {
        write!(f, "{} ({} years old)", this.name, this.age)
    }
}

// Usage
let person = Person { name: "Alice", age: 30 }
println!("{}", person)  // Alice (30 years old)
```

### Implementing Debug

```tml
// Manual implementation
implement Debug for Person {
    func fmt(this, f: mut ref Formatter) -> FmtResult {
        f.debug_struct("Person")
            .field("name", ref this.name)
            .field("age", ref this.age)
            .finish()
    }
}

// Or use derive
@derive(Debug)
type Person {
    name: String,
    age: U32,
}

// Usage
println!("{:?}", person)   // Person { name: "Alice", age: 30 }
println!("{:#?}", person)  // Person {
                           //     name: "Alice",
                           //     age: 30,
                           // }
```

---

## Custom Format Specifiers

```tml
// Implementing custom format types
type Currency {
    amount: I64,  // cents
    symbol: String,
}

implement Display for Currency {
    func fmt(this, f: mut ref Formatter) -> FmtResult {
        let dollars = this.amount / 100
        let cents = (this.amount % 100).abs()

        // Respect precision for cents
        let precision = f.precision().unwrap_or(2)

        if precision == 0 then {
            write!(f, "{}{}", this.symbol, dollars)
        } else {
            write!(f, "{}{}.{:0>width$}",
                this.symbol,
                dollars,
                cents,
                width = precision as U64
            )
        }
    }
}

// Usage
let price = Currency { amount: 1234, symbol: "$" }
println!("{}", price)      // $12.34
println!("{:.0}", price)   // $12
println!("{:.3}", price)   // $12.340
```

---

## Write Trait

```tml
/// Trait for types that can receive formatted output
pub behavior Write {
    /// Writes a string
    func write_str(mut this, s: ref String) -> FmtResult

    /// Writes a character
    func write_char(mut this, c: Char) -> FmtResult {
        this.write_str(&c.to_string())
    }

    /// Writes formatted arguments
    func write_fmt(mut this, args: Arguments) -> FmtResult {
        // Default implementation
    }
}

implement Write for String {
    func write_str(mut this, s: ref String) -> FmtResult {
        this.push_str(s)
        return Ok(())
    }
}
```

---

## Examples

### Formatting Numbers

```tml
let n = 42
println!("Decimal: {}", n)           // Decimal: 42
println!("Binary: {:b}", n)          // Binary: 101010
println!("Binary: {:#b}", n)         // Binary: 0b101010
println!("Octal: {:o}", n)           // Octal: 52
println!("Octal: {:#o}", n)          // Octal: 0o52
println!("Hex: {:x}", n)             // Hex: 2a
println!("Hex: {:X}", n)             // Hex: 2A
println!("Hex: {:#x}", n)            // Hex: 0x2a

let f = 3.14159
println!("Float: {}", f)             // Float: 3.14159
println!("Float: {:.2}", f)          // Float: 3.14
println!("Float: {:8.2}", f)         // Float:     3.14
println!("Float: {:08.2}", f)        // Float: 00003.14
println!("Sci: {:e}", f)             // Sci: 3.14159e0
println!("Sci: {:.2e}", f)           // Sci: 3.14e0
```

### Padding and Alignment

```tml
let s = "hello"
println!("[{:10}]", s)               // [hello     ]
println!("[{:>10}]", s)              // [     hello]
println!("[{:^10}]", s)              // [  hello   ]
println!("[{:-^10}]", s)             // [--hello---]
println!("[{:.<10}]", s)             // [hello.....]
```

### Debug Output

```tml
@derive(Debug)
type Config {
    host: String,
    port: U16,
    options: HashMap[String, String],
}

let config = Config {
    host: "localhost",
    port: 8080,
    options: HashMap.from([
        ("timeout", "30"),
        ("retries", "3"),
    ]),
}

println!("{:?}", config)
// Config { host: "localhost", port: 8080, options: {"timeout": "30", "retries": "3"} }

println!("{:#?}", config)
// Config {
//     host: "localhost",
//     port: 8080,
//     options: {
//         "timeout": "30",
//         "retries": "3",
//     },
// }
```

---

## See Also

- [std.string](./22-STRING.md) — String operations
- [04-ENCODING.md](./04-ENCODING.md) — Text encodings
