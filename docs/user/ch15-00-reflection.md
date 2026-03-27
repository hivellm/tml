# Chapter 15: Reflection

TML provides opt-in runtime reflection for types that derive the `Reflect` behavior. This enables introspection of type metadata, field access, and class method information at runtime.

## Compile-Time Intrinsics

These intrinsics are evaluated at compile time and have zero runtime cost:

| Intrinsic | Returns | Description |
|-----------|---------|-------------|
| `field_count[T]()` | `I64` | Number of fields in struct/class |
| `field_name[T](index)` | `Str` | Field name by index |
| `field_offset[T](index)` | `I64` | Field byte offset |
| `field_type_id[T](index)` | `U64` | Field type hash |
| `variant_count[T]()` | `I64` | Number of enum variants |
| `type_id[T]()` | `U64` | FNV-1a hash of type name |
| `type_name[T]()` | `Str` | Type name as string |
| `size_of[T]()` | `I64` | Size in bytes |
| `align_of[T]()` | `I64` | Alignment in bytes |

### OOP Class Intrinsics

| Intrinsic | Returns | Description |
|-----------|---------|-------------|
| `is_abstract[T]()` | `Bool` | True if class is abstract |
| `is_sealed[T]()` | `Bool` | True if class is sealed |
| `base_class[T]()` | `Str` | Base class name (empty if none) |
| `method_count[T]()` | `I64` | Number of class methods |
| `method_name[T](index)` | `Str` | Method name by index |
| `is_virtual[T](index)` | `Bool` | True if method is virtual |
| `is_override[T](index)` | `Bool` | True if method overrides base |
| `is_static_method[T](index)` | `Bool` | True if method is static |

## @derive(Reflect)

Add `@derive(Reflect)` to generate runtime type metadata:

```tml
use core::reflect::*

@derive(Reflect)
type Person {
    name: Str,
    age: I32
}

func main() {
    let info: ref TypeInfo = Person::type_info()
    println(info.name)          // "Person"
    println(info.field_count)   // 2
    println(info.size)          // 12
}
```

## TypeInfo

The `TypeInfo` struct provides complete type metadata:

- `id: U64` — unique type identifier
- `name: Str` — type name
- `kind: TypeKind` — Struct, Enum, Primitive, Class, Interface, etc.
- `size: I64` — size in bytes
- `align: I64` — alignment
- `field_count: I64` — number of fields
- `variant_count: I64` — number of enum variants (or method count for classes)

## MethodInfo

For class types, `MethodInfo` describes each method:

- `name: Str` — method name
- `is_virtual: Bool` — uses virtual dispatch
- `is_override: Bool` — overrides base class method
- `is_static: Bool` — no receiver (static method)

## Field Access

Read and write struct fields dynamically:

```tml
use core::reflect::*

@derive(Reflect)
type Point { x: I64, y: I64 }

func main() {
    var p = Point { x: 10, y: 20 }
    let base: I64 = lowlevel { ref p as I64 }
    let offset_x = field_offset[Point](0)
    let val = read_field_i64(base, offset_x)
    println(val) // 10
}
```

## Debug Print

The `debug_print` utility prints any Reflect type:

```tml
use std::debug::debug_print

@derive(Reflect)
type Color { r: I64, g: I64, b: I64 }

func main() {
    let c = Color { r: 255, g: 128, b: 0 }
    debug_print(ref c) // Color { r: 255, g: 128, b: 0 }
}
```

## OOP Class Reflection

Query class metadata at compile time:

```tml
use std::collections::List

class Animal {
    name: Str
    virtual func speak(this) -> Str { return "..." }
    new(name: Str) { this.name = name }
}

func main() {
    let mc = method_count[Animal]()      // 1
    let m0 = method_name[Animal](0)      // "speak"
    let v0 = is_virtual[Animal](0)       // true
    let abs = is_abstract[Animal]()      // false
}
```
