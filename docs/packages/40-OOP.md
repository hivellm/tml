# TML Standard Library: OOP

> `std::oop` -- Object-oriented programming support (C#-style class hierarchies).

## Overview

Provides C#-style OOP primitives that complement TML's native behavior system. This module defines common interfaces for class-based programming and a base `Object` class for building class hierarchies with inheritance and virtual dispatch.

**When to use behaviors vs OOP:**
- Use **behaviors** (from `core`) for generic, composable abstractions following TML idioms (`PartialEq`, `Ord`, `Hash`).
- Use **interfaces** and **Object** from this module when modeling class hierarchies with inheritance and virtual dispatch.

## Import

```tml
use std::oop::{Object, Objects}
use std::oop::interfaces::*
```

---

## Submodules

| Module | Description |
|--------|-------------|
| `interfaces` | C#-style interfaces (IEquatable, IComparable, IDisposable, etc.) |
| `object` | Base `Object` class for class hierarchies |

---

## Core Interfaces

| Interface | Methods |
|-----------|---------|
| `IEquatable[T]` | `equals(this, other: T) -> Bool` |
| `IComparable[T]` | `compare_to(this, other: T) -> I32` |
| `IHashable` | `get_hash_code(this) -> I64` |
| `IFormattable` | `to_string(this) -> Str`, `to_string_format(this, format: Str) -> Str` |
| `ICloneable` | `duplicate(this) -> Self` |
| `IDisposable` | `dispose(mut this)` |

## Collection Interfaces

| Interface | Key Methods |
|-----------|-------------|
| `IEnumerable[T]` | `get_enumerator(this) -> dyn IEnumerator[T]` |
| `IEnumerator[T]` | `move_next(mut this) -> Bool`, `current(this) -> T`, `reset(mut this)` |
| `ICollection[T]` | `count`, `add`, `remove`, `clear`, `contains` |
| `IList[T]` | `get`, `set`, `index_of`, `insert`, `remove_at` |
| `IDictionary[K, V]` | `get`, `set`, `contains_key`, `remove`, `count`, `clear` |
| `IReadOnlyCollection[T]` | `count`, `is_empty`, `contains` |

## Observer Pattern

| Interface | Methods |
|-----------|---------|
| `IObserver[T]` | `on_next`, `on_error`, `on_completed` |
| `IObservable[T]` | `subscribe(mut this, observer: dyn IObserver[T])` |

---

## Object

The base class for all TML classes. Implements `IEquatable[Object]`, `IHashable`, and `IFormattable`.

```tml
pub class Object {
    virtual func equals(this, other: Object) -> Bool
    virtual func get_hash_code(this) -> I64
    virtual func to_string(this) -> Str
    virtual func to_string_format(this, format: Str) -> Str
    virtual func get_type(this) -> Str
    func reference_equals(this, other: Object) -> Bool
}
```

## Objects

Static utility methods for working with any `Object` instance.

```tml
pub class Objects {
    static func equals(a: Object, b: Object) -> Bool
    static func hash(obj: Object) -> I64
    static func to_string(obj: Object) -> Str
}
```

---

## Example

```tml
use std::oop::{Object, Objects}
use std::oop::interfaces::{IEquatable, IComparable}

class Person extends Object implements IComparable[Person] {
    name: Str
    age: I32

    override func to_string(this) -> Str {
        return "Person: " + this.name
    }

    func compare_to(this, other: Person) -> I32 {
        return this.age - other.age
    }
}

func main() {
    let alice = Person { name: "Alice", age: 30 }
    let bob = Person { name: "Bob", age: 25 }
    println(alice.to_string())            // "Person: Alice"
    println(alice.compare_to(bob) > 0)    // true (Alice is older)
}
```
