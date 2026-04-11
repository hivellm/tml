# F12: Generic Monomorphization

**Priority**: Low (long-term)
**Impact**: Collections performance, type safety
**Complexity**: Very High

## Problem

Generics currently use type erasure (values stored as I64), requiring unsafe casts:

```tml
let owned_key_i64: I64 = key.duplicate() as I64
let owned_val_i64: I64 = value.duplicate() as I64
```

This loses type safety and prevents SIMD optimizations inside generic code.

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `lib/std/src/collections/hashmap.tml` | 128-130, 210-211 | I64 type erasure |
| `lib/std/src/collections/hashmap.tml` | 223-226 | SIMD disabled in generics |

## Proposal

Full monomorphization: generate specialized versions of generic functions
for each concrete type combination, similar to Rust/C++ templates.

Benefits:
- No runtime casts
- Proper type safety
- SIMD works inside generic code
- Better optimization opportunities

## C++ Compiler Changes

This is a major compiler architecture change:
1. **Instantiation engine**: Clone + substitute generic functions per concrete type set
2. **Symbol mangling**: Encode type parameters in symbol names
3. **Dead code elimination**: Remove unused instantiations
4. **Incremental compilation**: Cache instantiations across compilations
