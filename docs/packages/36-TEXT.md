# TML Standard Library: Text

> `std::text` -- Dynamic string type with heap allocation and rich manipulation API.

## Overview

Provides `Text`, a growable, heap-allocated string type for dynamic string building and manipulation. Text uses a 24-byte header (data pointer, length, capacity) with a separate heap-allocated byte buffer that grows automatically.

Use `Text` when you need mutable strings that grow and shrink, dynamic string building, or rich string operations. For static, immutable strings use `Str` instead.

## Import

```tml
use std::text::Text
```

---

## Text

A growable, heap-allocated string with automatic capacity management.

### Construction

```tml
func Text::new() -> Text                                // Empty text
func Text::from(s: Str) -> Text                         // From string literal
func Text::with_capacity(cap: I64) -> Text              // Pre-allocated capacity
func Text::from_i64(value: I64) -> Text                 // From integer
func Text::from_f64(value: F64) -> Text                 // From float
func Text::from_f64_precision(value: F64, p: I32) -> Text  // From float with precision
func Text::from_bool(value: Bool) -> Text               // From boolean
```

### Properties

```tml
func len(this) -> I64              // Length in bytes
func capacity(this) -> I64         // Current buffer capacity
func is_empty(this) -> Bool        // True if length is 0
func byte_at(this, index: I64) -> I32  // Byte at index (-1 if out of bounds)
```

### Conversion and Lifecycle

```tml
func as_str(this) -> Str           // Convert to Str (heap copy)
func clone(this) -> Text           // Deep copy
func drop(mut this)                // Free memory
```

### Mutation

```tml
func push(this, byte: I32)                  // Append single byte
func push_str(this, s: Str)                 // Append string
func push_i64(this, value: I64)             // Append integer
func push_formatted(this, prefix: Str, value: I64, suffix: Str)
func clear(this)                             // Clear content
func reserve(this, additional: I64)          // Reserve extra capacity
func fill_char(this, byte: I32, count: I64)  // Append N copies of byte
```

### Search

```tml
func index_of(this, search: Str) -> I64       // First occurrence (-1 if not found)
func last_index_of(this, search: Str) -> I64   // Last occurrence (-1 if not found)
func starts_with(this, prefix: Str) -> Bool
func ends_with(this, suffix: Str) -> Bool
func contains(this, search: Str) -> Bool
func includes(this, search: Str) -> Bool       // Alias for contains
```

### Transformation (return new Text)

```tml
func to_upper_case(this) -> Text
func to_lower_case(this) -> Text
func trim(this) -> Text
func trim_start(this) -> Text
func trim_end(this) -> Text
func substring(this, start: I64, end: I64) -> Text
func slice(this, start: I64, end: I64) -> Text   // Alias for substring
func repeat(this, count: I64) -> Text
func replace(this, search: Str, replacement: Str) -> Text
func replace_all(this, search: Str, replacement: Str) -> Text
func reverse(this) -> Text
func pad_start(this, target_len: I64, pad_char: I32) -> Text
func pad_end(this, target_len: I64, pad_char: I32) -> Text
```

### Concatenation and Comparison

```tml
func concat(this, other: ref Text) -> Text
func concat_str(this, s: Str) -> Text
func compare(this, other: ref Text) -> I32   // -1, 0, or 1
func equals(this, other: ref Text) -> Bool
func print(this)                              // Print to stdout
func println(this)                            // Print with newline
```

---

## Example

```tml
use std::text::Text

func main() {
    let greeting = Text::from("Hello, ")
    greeting.push_str("World!")
    println(greeting.as_str())  // "Hello, World!"

    let upper = greeting.to_upper_case()
    println(upper.as_str())  // "HELLO, WORLD!"

    let trimmed = Text::from("  spaces  ").trim()
    println(trimmed.as_str())  // "spaces"

    let replaced = greeting.replace_all("l", "L")
    println(replaced.as_str())  // "HeLLo, WorLd!"

    greeting.drop()
    upper.drop()
}
```
