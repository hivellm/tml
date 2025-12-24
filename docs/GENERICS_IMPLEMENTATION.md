# Generics Implementation in TML

This document describes the implementation of generic types in the TML compiler using **monomorphization** (Rust-style).

## Overview

TML uses monomorphization for generics, meaning each unique instantiation of a generic type generates separate code. For example, `Pair[I32]` and `Pair[Bool]` become two distinct LLVM types.

## Name Mangling

Generic types are mangled using double underscores as separators:

| TML Type | Mangled Name | LLVM Type |
|----------|--------------|-----------|
| `Pair[I32]` | `Pair__I32` | `%struct.Pair__I32` |
| `Entry[I32, Str]` | `Entry__I32__Str` | `%struct.Entry__I32__Str` |
| `Maybe[Bool]` | `Maybe__Bool` | `%struct.Maybe__Bool` |
| `Outcome[I32, I32]` | `Outcome__I32__I32` | `%struct.Outcome__I32__I32` |

## Supported Features

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

// Usage
let p: Pair[I32] = Pair { first: 10, second: 20 }
print(p.first)   // 10
print(p.second)  // 20

let e: Entry[I32, I32] = Entry { key: 1, value: 100 }
print(e.key)     // 1
print(e.value)   // 100
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

// Usage with pattern matching
let m: Maybe[I32] = Just(42)
when m {
    Just(v) => print(v),     // 42
    Nothing => print(0),
}

let r: Outcome[I32, I32] = Ok(200)
when r {
    Ok(v) => print(v),       // 200
    Err(e) => print(e),
}
```

### Multi-Parameter Generics

```tml
type Triple[A, B, C] {
    a: A,
    b: B,
    c: C,
}

let t: Triple[I32, I32, I32] = Triple { a: 1, b: 2, c: 3 }
print(t.a)  // 1
print(t.b)  // 2
print(t.c)  // 3
```

## Implementation Details

### 1. Type Substitution (`substitute_type`)

Located in `src/types/type.cpp`, this function recursively substitutes generic type parameters with concrete types.

```cpp
auto substitute_type(const TypePtr& type,
                     const std::unordered_map<std::string, TypePtr>& subs) -> TypePtr;
```

### 2. Instantiation Cache

The codegen maintains caches to avoid duplicate code generation:

```cpp
// In llvm_ir_gen.hpp
struct GenericInstantiation {
    std::string base_name;
    std::vector<types::TypePtr> type_args;
    std::string mangled_name;
    bool generated = false;
};

std::unordered_map<std::string, GenericInstantiation> struct_instantiations_;
std::unordered_map<std::string, GenericInstantiation> enum_instantiations_;
```

### 3. Deferred Generation

Generic type declarations are stored for later instantiation:

```cpp
// Pending generic declarations
std::unordered_map<std::string, const parser::StructDecl*> pending_generic_structs_;
std::unordered_map<std::string, const parser::EnumDecl*> pending_generic_enums_;
```

### 4. Field Registry

For dynamic field access resolution:

```cpp
struct FieldInfo {
    std::string name;
    int index;
    std::string llvm_type;
};
std::unordered_map<std::string, std::vector<FieldInfo>> struct_fields_;
```

### 5. Enum Variant Registry

For pattern matching in generic enums:

```cpp
std::unordered_map<std::string, int> enum_variants_;
// Key: "Maybe__I32::Just" -> 0
// Key: "Maybe__I32::Nothing" -> 1
```

## Code Generation Flow

1. **Declaration Phase**: Generic declarations are stored in `pending_generic_*` maps
2. **Usage Detection**: When `Pair[I32]` is encountered, `require_struct_instantiation()` is called
3. **Immediate Registration**: Field/variant info is registered immediately for use in function codegen
4. **Deferred IR Generation**: Actual LLVM IR types are emitted in `generate_pending_instantiations()`

## Implemented Features

### Generic Functions

```tml
func identity[T](x: T) -> T {
    return x
}

// Usage - types are inferred from arguments
let x: I32 = identity(42)  // Infers T = I32
```

### Where Clause Constraints

Constraints are enforced at call sites:

```tml
// Function with constraint
func equals[T](a: T, b: T) -> Bool where T: Eq {
    return true
}

// Works - I32 implements Eq
let result: Bool = equals(10, 20)

// Error - MyType doesn't implement Eq
type MyType { value: I32 }
let x: MyType = MyType { value: 1 }
equals(x, x)  // Error: Type 'MyType' does not implement behavior 'Eq'
```

Supported constraint syntax:
- Single behavior: `where T: Eq`
- Multiple behaviors: `where T: Eq + Ord`
- Multiple type params: `where T: Eq, U: Ord`

### Builtin Behavior Implementations

| Type | Implements |
|------|-----------|
| I8-I128, U8-U128 | Eq, Ord, Numeric, Hash, Display, Debug, Default, Duplicate |
| F32, F64 | Eq, Ord, Numeric, Display, Debug, Default, Duplicate |
| Bool | Eq, Ord, Hash, Display, Debug, Default, Duplicate |
| Char, Str | Eq, Ord, Hash, Display, Debug, Duplicate |

### Generic Field Access

Field access on generic structs correctly resolves types:

```tml
let p: Pair[I32] = Pair { first: 10, second: 20 }
let x: I32 = p.first  // Works - correctly infers I32
if p.first == 10 { ... }  // Works - comparison uses I32
```

## Test Coverage

The comprehensive test suite covers:

- Single-parameter generic structs (`Pair[T]`)
- Two-parameter generic structs (`Entry[K, V]`)
- Three-parameter generic structs (`Triple[A, B, C]`)
- Generic enum variants (`Just(T)`, `Nothing`)
- Multi-param generic enums (`Outcome[T, E]`)
- Pattern matching with generics
- Multiple instantiations in same scope
- Edge cases (zero values, negative numbers)

See: `tests/tml/compiler/generics_comprehensive.test.tml`

## Files Modified

| File | Purpose |
|------|---------|
| `include/tml/types/type.hpp` | `substitute_type` declaration |
| `src/types/type.cpp` | Type substitution implementation |
| `include/tml/codegen/llvm_ir_gen.hpp` | Caches and registries |
| `src/codegen/llvm_ir_gen.cpp` | Mangling, instantiation requests |
| `src/codegen/llvm_ir_gen_decl.cpp` | Struct/enum generation |
| `src/codegen/llvm_ir_gen_types.cpp` | Field access resolution |
| `src/codegen/llvm_ir_gen_builtins.cpp` | Enum constructor codegen |
| `src/codegen/llvm_ir_gen_stmt.cpp` | Let statement with generics |
| `src/codegen/llvm_ir_gen_control.cpp` | When expression with generics |
