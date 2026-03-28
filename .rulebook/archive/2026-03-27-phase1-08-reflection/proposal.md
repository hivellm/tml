# RFC: Complete Reflection System for TML

## Summary

Implement a comprehensive reflection system for TML that enables runtime type introspection, field access, and dynamic dispatch without requiring traditional OOP (inheritance hierarchies).

## Motivation

Reflection enables:
- **Serialization/Deserialization**: JSON, binary, XML without boilerplate
- **Debug/Logging**: Automatic `to_string()` for any type
- **Testing frameworks**: Property-based testing, fuzzing
- **ORM/Database mapping**: Automatic query generation
- **Dependency injection**: Runtime component wiring
- **Plugin systems**: Dynamic loading and instantiation

## Design Philosophy

TML's reflection follows the **"zero-cost abstraction"** principle:
- Compile-time reflection is preferred (no runtime cost)
- Runtime reflection is opt-in via `@derive(Reflect)`
- No OOP inheritance required - uses behaviors (traits)

## Proposed API

### 1. Compile-Time Intrinsics (Already Partially Implemented)

```tml
// Existing
let id: U64 = type_id[Point]()
let name: Str = type_name[Point]()
let size: USize = size_of[Point]()
let align: USize = align_of[Point]()

// New intrinsics to add
let count: USize = field_count[Point]()
let variant_count: USize = variant_count[Maybe[I32]]()
```

### 2. TypeInfo Struct (Runtime Metadata)

```tml
type TypeKind {
    Primitive,      // I32, Bool, F64, etc.
    Struct,         // type Point { x: I32, y: I32 }
    Enum,           // type Color { Red, Green, Blue }
    Adt,            // type Maybe[T] { Just(T), Nothing }
    Tuple,          // (I32, Str, Bool)
    Array,          // [I32; 10]
    Slice,          // [I32]
    Function,       // func(I32) -> Bool
    Pointer,        // ref T, mut ref T
    Behavior,       // dyn Display
}

type FieldInfo {
    name: Str,
    type_id: U64,
    type_name: Str,
    offset: USize,
    is_public: Bool,
}

type VariantInfo {
    name: Str,
    tag: I64,
    payload_types: List[U64],  // TypeIds of payload fields
}

type TypeInfo {
    id: U64,
    name: Str,
    kind: TypeKind,
    size: USize,
    align: USize,
    fields: List[FieldInfo],      // For structs
    variants: List[VariantInfo],  // For enums/ADTs
    type_params: List[U64],       // Generic parameters
}
```

### 3. Reflect Behavior

```tml
behavior Reflect {
    /// Get static type information
    func type_info() -> ref TypeInfo

    /// Get runtime type info (for dyn types)
    func runtime_type_info(this) -> ref TypeInfo

    /// Get field value by name (returns Maybe[ref Any])
    func get_field(ref this, name: Str) -> Maybe[ref Any]

    /// Set field value by name
    func set_field(mut ref this, name: Str, value: ref Any) -> Outcome[(), ReflectError]

    /// Get field value by index
    func get_field_by_index(ref this, index: USize) -> Maybe[ref Any]

    /// For enums: get current variant name
    func variant_name(ref this) -> Str

    /// For enums: get variant tag
    func variant_tag(ref this) -> I64
}
```

### 4. Any Type (Type-Erased Container)

```tml
/// Type-erased value with runtime type information
type Any {
    data: ptr,           // Pointer to data
    type_info: ref TypeInfo,
    drop_fn: ptr,        // Destructor function pointer
}

impl Any {
    /// Create Any from a value
    func from[T: Reflect](value: T) -> Any

    /// Try to downcast to concrete type
    func downcast[T: Reflect](ref this) -> Maybe[ref T]

    /// Try to downcast mutably
    func downcast_mut[T: Reflect](mut ref this) -> Maybe[mut ref T]

    /// Check if contains type T
    func is[T: Reflect](ref this) -> Bool

    /// Get type info
    func type_info(ref this) -> ref TypeInfo
}
```

### 5. Automatic Derivation

```tml
// Compiler generates Reflect implementation
@derive(Reflect)
type Point {
    x: I32,
    y: I32,
}

@derive(Reflect)
type Shape {
    Circle(F64),           // radius
    Rectangle(F64, F64),   // width, height
    Triangle(F64, F64, F64), // sides
}

// Generated code (conceptually):
impl Reflect for Point {
    func type_info() -> ref TypeInfo {
        static info: TypeInfo = TypeInfo {
            id: 0x1234...,
            name: "Point",
            kind: TypeKind::Struct,
            size: 8,
            align: 4,
            fields: [
                FieldInfo { name: "x", type_id: type_id[I32](), ... },
                FieldInfo { name: "y", type_id: type_id[I32](), ... },
            ],
            ...
        }
        return ref info
    }

    func get_field(ref this, name: Str) -> Maybe[ref Any] {
        when name.as_str() {
            "x" => Just(Any::from(ref this.x)),
            "y" => Just(Any::from(ref this.y)),
            _ => Nothing
        }
    }
    ...
}
```

### 6. Usage Examples

```tml
// Automatic debug printing
func debug_print[T: Reflect](value: ref T) {
    let info = T::type_info()
    print("{} {{ ", info.name)
    for field in info.fields {
        let val = value.get_field(field.name)
        print("{}: {:?}, ", field.name, val)
    }
    print("}}")
}

// Generic serialization
func to_json[T: Reflect](value: ref T) -> Str {
    let info = T::type_info()
    when info.kind {
        TypeKind::Struct => {
            let mut parts: List[Str] = []
            for field in info.fields {
                if let Just(val) = value.get_field(field.name) {
                    parts.push(format!("\"{}\": {}", field.name, to_json_value(val)))
                }
            }
            return "{ " + parts.join(", ") + " }"
        },
        TypeKind::Primitive => format_primitive(value),
        _ => "null"
    }
}

// Runtime type checking
func process(value: Any) {
    if value.is[Point]() {
        let point = value.downcast[Point]().unwrap()
        println("Point at ({}, {})", point.x, point.y)
    } else if value.is[Shape]() {
        let shape = value.downcast[Shape]().unwrap()
        println("Shape variant: {}", shape.variant_name())
    }
}
```

## Implementation Strategy

### Phase 1: Core Intrinsics (2 days)
- Add `field_count[T]()` intrinsic
- Add `variant_count[T]()` intrinsic
- Add `field_name[T](index)` intrinsic
- Add `field_type_id[T](index)` intrinsic

### Phase 2: TypeInfo Generation (3 days)
- Define TypeInfo, FieldInfo, VariantInfo types in core library
- Compiler generates static TypeInfo for each type
- Store in `.rdata` section of binary

### Phase 3: Reflect Behavior (3 days)
- Define Reflect behavior in core library
- Implement `@derive(Reflect)` in compiler
- Generate field accessors

### Phase 4: Any Type (2 days)
- Implement type-erased Any container
- Safe downcasting with type checks
- Proper cleanup via drop_fn

### Phase 5: Integration & Testing (2 days)
- Debug derive macro
- JSON serialization example
- Comprehensive tests

## Alternatives Considered

### 1. Full OOP with Inheritance
**Rejected**: Adds complexity (vtables, slicing, diamond problem) without clear benefit for reflection.

### 2. Macro-based Reflection
**Rejected**: Less integrated, harder to use, requires external tools.

### 3. No Reflection
**Rejected**: Too limiting for real-world applications (serialization, debugging).

## Compatibility

- **Backwards compatible**: Existing code unchanged
- **Opt-in**: Only types with `@derive(Reflect)` incur metadata cost
- **Zero-cost for non-reflected types**: No vtables or metadata unless requested

## Open Questions

1. Should `Any` use heap allocation or be sized?
2. Should reflection support private fields?
3. Should we add `@reflect` attribute for fine-grained control?

## References

- Rust: `std::any::Any`, `TypeId`
- Go: `reflect` package
- Java: `java.lang.reflect`
- C#: `System.Reflection`
- Zig: `@typeInfo` comptime reflection
