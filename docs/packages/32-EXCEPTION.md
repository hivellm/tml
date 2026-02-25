# TML Standard Library: Exception

> `std::exception` — C#-style exception hierarchy for structured error handling.

## Overview

The `std::exception` module provides a class-based exception hierarchy inspired by C#/.NET. All exception types extend `Object` via the `Exception` base class and provide consistent `create()` factory methods, `get_message()`, `to_string()`, and `get_type()` methods.

## Import

```tml
use std::exception
use std::exception::{Exception, ArgumentException, IOException, InvalidOperationException}
```

---

## Base Class

### Exception

```tml
impl Exception {
    /// Creates an exception with a message.
    pub func create(message: Str) -> Exception

    /// Returns the error message.
    pub func get_message(this) -> Str

    /// Returns a string representation.
    pub func to_string(this) -> Str

    /// Returns the exception type name.
    pub func get_type(this) -> Str
}
```

---

## Argument Exceptions

### ArgumentException

Thrown when a method argument is invalid.

```tml
impl ArgumentException {
    pub func create(message: Str) -> ArgumentException
    pub func get_message(this) -> Str
    pub func to_string(this) -> Str
    pub func get_type(this) -> Str
}
```

### ArgumentNullException

Thrown when a null/Nothing argument is passed to a method that does not accept it.

```tml
impl ArgumentNullException {
    pub func create(param_name: Str) -> ArgumentNullException
}
```

### ArgumentOutOfRangeException

Thrown when an argument value is outside the allowable range.

```tml
impl ArgumentOutOfRangeException {
    pub func create(param_name: Str, actual: Str, message: Str) -> ArgumentOutOfRangeException
}
```

---

## Operation Exceptions

### InvalidOperationException

Thrown when a method call is invalid for the object's current state.

```tml
impl InvalidOperationException {
    pub func create(message: Str) -> InvalidOperationException
}
```

### NotSupportedException

Thrown when a method is not supported by the implementation.

```tml
impl NotSupportedException {
    pub func create(message: Str) -> NotSupportedException
}
```

### NotImplementedException

Thrown when a method has not been implemented yet.

```tml
impl NotImplementedException {
    pub func create(message: Str) -> NotImplementedException
}
```

---

## Index and Key Exceptions

### IndexOutOfRangeException

Thrown when an index is outside the bounds of a collection.

```tml
impl IndexOutOfRangeException {
    pub func create(message: Str) -> IndexOutOfRangeException

    /// Creates with context about the invalid index and collection length.
    pub func with_bounds(index: I64, length: I64) -> IndexOutOfRangeException
}
```

### KeyNotFoundException

Thrown when a key is not found in a dictionary or map.

```tml
impl KeyNotFoundException {
    pub func create(message: Str) -> KeyNotFoundException
}
```

---

## I/O Exceptions

### IOException

Base exception for I/O errors.

```tml
impl IOException {
    pub func create(message: Str) -> IOException
}
```

### FileNotFoundException

Thrown when a file cannot be found. Extends `IOException`.

```tml
impl FileNotFoundException {
    pub func create(message: Str) -> FileNotFoundException
}
```

---

## Arithmetic and Type Exceptions

### ArithmeticException

Thrown on arithmetic errors such as overflow or division by zero.

```tml
impl ArithmeticException {
    pub func create(message: Str) -> ArithmeticException

    /// Creates an overflow exception.
    pub func overflow() -> ArithmeticException

    /// Creates a divide-by-zero exception.
    pub func divide_by_zero() -> ArithmeticException
}
```

### FormatException

Thrown when the format of a string or input is invalid.

```tml
impl FormatException {
    pub func create(message: Str) -> FormatException
}
```

### InvalidCastException

Thrown when a type cast is invalid.

```tml
impl InvalidCastException {
    pub func create(message: Str) -> InvalidCastException

    /// Creates with source and target type names.
    pub func create(from_type: Str, to_type: Str) -> InvalidCastException
}
```

### TimeoutException

Thrown when an operation exceeds its time limit.

```tml
impl TimeoutException {
    pub func create(message: Str) -> TimeoutException
}
```

---

## Example

```tml
use std::exception::{ArgumentException, IndexOutOfRangeException, ArithmeticException}

func divide(a: I64, b: I64) -> I64 {
    if b == 0 {
        panic(ArithmeticException::divide_by_zero().to_string())
    }
    return a / b
}

func get_item(items: ref [Str], index: I64) -> Str {
    if index < 0 or index >= items.len() {
        let ex = IndexOutOfRangeException::with_bounds(index, items.len())
        panic(ex.to_string())
    }
    return items[index]
}
```

---

## See Also

- [core::error](./04-ERROR.md) — `Outcome[T, E]` error handling
- [core::fmt](./06-FMT.md) — Formatting and display
