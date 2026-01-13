# Spec: TypeId Compiler Intrinsic

## Problem

`TypeId::of[T]()` requires compiler support to generate unique type identifiers at compile time.

### Current State

```tml
// In lib/core/src/any.tml
pub type TypeId {
    id: U64
}

impl TypeId {
    // Cannot implement without compiler intrinsic:
    // pub func of[T]() -> TypeId { ... }
}
```

### Expected Behavior

```tml
let id1 = TypeId::of[I32]()
let id2 = TypeId::of[I64]()
let id3 = TypeId::of[I32]()

assert(id1 != id2)  // Different types have different IDs
assert(id1 == id3)  // Same type has same ID
```

## Design

### Type ID Generation

Each monomorphized type gets a unique ID based on:
1. Type name (e.g., "I32", "List")
2. Type arguments (e.g., "[I32]", "[Str, I64]")
3. Module path (to avoid collisions)

#### Algorithm

```
TypeId = hash(canonical_type_name)

canonical_type_name examples:
- "I32" -> "core::I32"
- "List[I32]" -> "std::collections::List[core::I32]"
- "HashMap[Str, I64]" -> "std::collections::HashMap[core::Str, core::I64]"
```

Use FNV-1a hash (64-bit) for fast, collision-resistant hashing.

### Intrinsic Recognition

In type checker, recognize `TypeId::of[T]()` pattern:

```cpp
// In types/checker/call.cpp
if (is_static_call && receiver_type == "TypeId" && method == "of") {
    // This is the TypeId::of intrinsic
    auto type_arg = call.type_args[0];
    mark_as_intrinsic(IntrinsicKind::TypeIdOf, type_arg);
    return make_type_id();
}
```

### Codegen

In codegen, emit constant for TypeId::of:

```cpp
// In codegen/llvm_ir_gen_builtins.cpp
if (intrinsic == IntrinsicKind::TypeIdOf) {
    auto type_arg = get_type_arg(call);
    std::string canonical = get_canonical_type_name(type_arg);
    uint64_t hash = fnv1a_hash(canonical);

    // Generate struct literal
    emit_line("  " + result + " = insertvalue %struct.TypeId undef, i64 " +
              std::to_string(hash) + ", 0");
    last_expr_type_ = "%struct.TypeId";
    return result;
}
```

## Implementation

### Step 1: Add Intrinsic Enum

```cpp
// In types/intrinsics.hpp
enum class IntrinsicKind {
    TypeIdOf,      // TypeId::of[T]()
    TypeName,      // type_name[T]()
    SizeOf,        // size_of[T]()
    AlignOf,       // align_of[T]()
};
```

### Step 2: Type Checker Recognition

```cpp
// In types/checker/call.cpp
auto TypeChecker::check_static_call(const CallExpr& call) -> TypePtr {
    // ... existing code ...

    // Check for TypeId::of intrinsic
    if (receiver_name == "TypeId" && method_name == "of") {
        if (call.type_args.size() != 1) {
            error("TypeId::of requires exactly one type argument");
        }
        auto type_arg = resolve_type(call.type_args[0]);
        call.intrinsic = IntrinsicKind::TypeIdOf;
        call.intrinsic_type_arg = type_arg;
        return make_type_id();
    }

    // ... existing code ...
}
```

### Step 3: Codegen Implementation

```cpp
// In codegen/llvm_ir_gen_builtins.cpp
auto LLVMIRGen::gen_intrinsic_call(const CallExpr& call) -> std::string {
    switch (call.intrinsic) {
    case IntrinsicKind::TypeIdOf: {
        auto type_arg = call.intrinsic_type_arg;
        std::string canonical = canonicalize_type_name(type_arg);
        uint64_t id = fnv1a_hash_64(canonical);

        std::string result = fresh_reg();
        emit_line("  " + result + " = insertvalue %struct.TypeId undef, i64 " +
                  std::to_string(id) + ", 0");
        last_expr_type_ = "%struct.TypeId";
        return result;
    }
    // ... other intrinsics ...
    }
}

uint64_t fnv1a_hash_64(const std::string& str) {
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    for (char c : str) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL; // FNV prime
    }
    return hash;
}
```

## Test Cases

```tml
use core::any::*

@test
func test_typeid_equality() -> I32 {
    let id1 = TypeId::of[I32]()
    let id2 = TypeId::of[I32]()
    assert(id1 == id2, "same type should have same TypeId")
    return 0
}

@test
func test_typeid_inequality() -> I32 {
    let id1 = TypeId::of[I32]()
    let id2 = TypeId::of[I64]()
    assert(id1 != id2, "different types should have different TypeIds")
    return 0
}

@test
func test_typeid_generic() -> I32 {
    let id1 = TypeId::of[List[I32]]()
    let id2 = TypeId::of[List[I64]]()
    let id3 = TypeId::of[List[I32]]()

    assert(id1 != id2, "List[I32] != List[I64]")
    assert(id1 == id3, "List[I32] == List[I32]")
    return 0
}
```
