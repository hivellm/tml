---
name: generic-instantiation-dispatch-fix
description: Multiple generic instantiations (e.g., List[Str] + List[Violation]) — stale last_semantic_type_ caused wrong method dispatch
type: project
---

## Bug: Multiple Generic Instantiations Wrong Method Dispatch (2026-03-25, FIXED)

**Symptom**: When a function uses two different instantiations of the same generic type (e.g., `List[Str]` and `List[Violation]`), method calls on the second instantiation would dispatch to the FIRST instantiation's methods. For example, `lines.len()` on a `List[Str]` would call `List__Violation::len`.

**Root cause**: In `llvm_ir_gen_stmt.cpp` lines 1124-1161, the let-binding path for struct variables validated `last_semantic_type_` by comparing ONLY the base type name (e.g., "List" == "List") without checking type arguments. When `last_semantic_type_` was stale from a previous expression (e.g., `List[Violation]::new()` from a struct init), it would be incorrectly applied to a variable of a different instantiation (e.g., `List[Str]` from `str::split`).

**Key detail**: `last_semantic_type_` is set by method calls and static dispatches (method_impl.cpp, method_static_dispatch.cpp) but NOT by free function calls (call.cpp). So after `List[Violation]::new()` sets it to `List[Violation]`, calling `str::split()` (a free function) does NOT clear or update it.

**Fix**: At `compiler/src/codegen/llvm/llvm_ir_gen_stmt.cpp` line ~1136, added full mangled name comparison: when `var_type` has type args (e.g., `List__Str`), compute `mangle_struct_name(named->name, named->type_args)` from the semantic type and compare against the full mangled name from `var_type`, not just the base name.

**Why:** `last_semantic_type_` should be trusted only when it fully matches the LLVM type, including type arguments.

**How to apply:** Any time `last_semantic_type_` is used as a fallback, ensure the full type (including generic args) is validated, not just the base name. The same pattern could potentially affect the ptr variable path (line ~743) but is lower risk there.

**Reproduction**: Create a struct containing `List[Violation]`, then call `str::split()` to get a `List[Str]`, then call methods on the `List[Str]` — they incorrectly dispatch to `List[Violation]` methods.
