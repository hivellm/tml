# RFC-0008: Generics and Monomorphization

## Status
Active (Implemented in v0.4.0)

## Summary

This RFC specifies how TML handles generic types through monomorphization,
where each unique instantiation of a generic type generates specialized code.

## Motivation

Generic types enable code reuse while maintaining type safety:
- `Maybe[T]` works for any type `T`
- `Outcome[T, E]` separates success and error types
- Collections like `List[T]` and `Map[K, V]` are type-safe

LLMs benefit from generics because:
1. Patterns are reusable across types
2. Type errors are caught at compile time
3. Generated code is efficient (no boxing)

## Specification

### Syntax

Generic type parameters use square brackets (not angle brackets):

```ebnf
GenericParams = '[' GenericParam (',' GenericParam)* ']'
GenericParam  = Ident (':' TypeBound)?
TypeBound     = Type ('+' Type)*
```

**Rationale**: `[T]` avoids ambiguity with `<` comparison operator.

### Generic Structs

```tml
type Pair[T] {
    first: T,
    second: T,
}

type Entry[K, V] {
    key: K,
    value: V,
}
```

### Generic Enums

```tml
type Maybe[T] {
    Just(T),
    Nothing,
}

type Outcome[T, E] {
    Ok(T),
    Err(E),
}
```

### Instantiation

Type arguments are provided at usage sites:

```tml
let p: Pair[I32] = Pair { first: 10, second: 20 }
let m: Maybe[I32] = Just(42)
let r: Outcome[I32, I32] = Ok(200)
```

### Monomorphization

Each unique instantiation generates a specialized type:

| TML Type | Mangled Name |
|----------|--------------|
| `Pair[I32]` | `Pair__I32` |
| `Pair[Bool]` | `Pair__Bool` |
| `Entry[I32, Str]` | `Entry__I32__Str` |
| `Maybe[I32]` | `Maybe__I32` |
| `Outcome[I32, I32]` | `Outcome__I32__I32` |

### LLVM IR Representation

```llvm
; Pair[I32] -> Pair__I32
%struct.Pair__I32 = type { i32, i32 }

; Maybe[I32] -> Maybe__I32 (tagged union)
%struct.Maybe__I32 = type { i32, [4 x i8] }
; tag 0 = Just, tag 1 = Nothing

; Outcome[I32, I32] -> Outcome__I32__I32
%struct.Outcome__I32__I32 = type { i32, [4 x i8] }
; tag 0 = Ok, tag 1 = Err
```

### Pattern Matching

Generic enums work with `when` expressions:

```tml
let m: Maybe[I32] = Just(42)
when m {
    Just(v) => print(v),
    Nothing => print(0),
}
```

The pattern matching extracts the payload with correct type.

## Implementation Details

### Type Substitution

The compiler maintains a substitution map during instantiation:

```
T -> I32
K -> Str
V -> Bool
```

All occurrences of type parameters are replaced with concrete types.

### Instantiation Cache

To avoid duplicate code generation:

```cpp
struct GenericInstantiation {
    std::string base_name;
    std::vector<TypePtr> type_args;
    std::string mangled_name;
    bool generated = false;
};
```

### Field/Variant Registry

For runtime field access and pattern matching:

```cpp
// struct_fields_["Pair__I32"] = [{first, 0, i32}, {second, 1, i32}]
// enum_variants_["Maybe__I32::Just"] = 0
// enum_variants_["Maybe__I32::Nothing"] = 1
```

## Examples

### Complete Example

```tml
use test

type Pair[T] {
    first: T,
    second: T,
}

type Maybe[T] {
    Just(T),
    Nothing,
}

@test
func test_generics() -> I32 {
    // Generic struct
    let p: Pair[I32] = Pair { first: 10, second: 20 }
    print(p.first)   // 10
    print(p.second)  // 20

    // Generic enum
    let m: Maybe[I32] = Just(42)
    when m {
        Just(v) => print(v),  // 42
        Nothing => print(0),
    }

    return 0
}
```

## Compatibility

- **RFC-0001**: Generics extend the core type system
- **RFC-0002**: Square bracket syntax is LL(1) compatible
- **RFC-0007**: IR includes type_params for generic definitions

## Limitations

### Current Implementation

1. **Generic Functions**: ✅ Fully implemented with monomorphization
2. **Bounds**: ✅ `where` clause syntax with type checking
3. **Field Type Resolution**: ✅ Type checker correctly resolves concrete types

### Remaining Work

1. **Higher-kinded types**: Not supported (`F[_]`)
2. **Associated types**: Not yet implemented
3. **Default type parameters**: Not yet implemented

## Alternatives Rejected

### Angle Brackets `<T>`

Rejected because `<` is used for comparison:
```tml
// Ambiguous
if a<b>c { }  // Comparison or generic?
```

### Erased Generics (Java-style)

Rejected because:
- Runtime type information lost
- No specialization possible
- Boxing overhead for primitives

### Trait Objects (Rust dyn)

Deferred for future consideration:
- Useful for heterogeneous collections
- Adds complexity
- Monomorphization covers most use cases

## References

- [Rust Monomorphization](https://rustc-dev-guide.rust-lang.org/backend/monomorph.html)
- [C++ Templates](https://en.cppreference.com/w/cpp/language/templates)
- [Swift Generics](https://docs.swift.org/swift-book/LanguageGuide/Generics.html)
