---
name: when-pattern-binding-fix
description: When-pattern enum payload bindings alias payload_ptr directly instead of load+copy (fixes Maybe[mut ref T] dangling pointer)
type: project
---

## When-pattern primitive binding — alias not copy (2026-03-19, FIXED)

**Bug**: `Maybe[mut ref T]` methods like `as_mut()` crashed with ACCESS_VIOLATION.

**Root cause**: In `when.cpp`, enum payload bindings for primitive types did:
1. `load i32, ptr payload_ptr` (copy value from struct)
2. `alloca i32` (new local)
3. `store i32 value, ptr alloca` (store copy)
4. Bind `val` to alloca

Then `mut ref val` returned the alloca pointer — dangling after function return.

**Fix**: All 3 binding sites in `when.cpp` (single-field, tuple-field, multi-field) now alias `payload_ptr` directly for ALL types (struct AND primitive), like:
```cpp
locals_[ident.name] = VarInfo{payload_ptr, bound_type, payload_type, std::nullopt};
```

**Why this works**: `gen_ident` detects `%t<digit>` register names and does `load type, ptr reg`, so by-value access still loads from the original location. `ref val`/`mut ref val` returns `payload_ptr` which points into the original struct.

**Key file**: `compiler/src/codegen/llvm/control/when.cpp` lines ~759, ~844, ~948

**Why:** Returning a pointer to a local alloca is UB — the alloca is freed on function return.

**How to apply:** Any when-pattern binding that creates a local copy instead of aliasing the original should be treated as suspicious. The original payload_ptr is valid for the entire when arm scope.
