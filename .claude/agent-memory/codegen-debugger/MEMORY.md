# Codegen Debugger Memory

## Index
- [suite-merging-investigation.md](suite-merging-investigation.md) - Full findings on the %struct.This/%struct.T and toowned_assoc bugs
- [this-parameter-conventions.md](this-parameter-conventions.md) - How `this`/`self` parameter types are determined in codegen

## Key Findings

### Generic Inference: FuncType vs ClosureType Mismatch (2026-03-03) -- FIXED
- `extract_type_params()` in both `expr_call_method.cpp` and `expr_call.cpp` only matched `FuncType` against `FuncType`
- Closures return `ClosureType` from `check_closure()` (types_checker.cpp:247), not `FuncType`
- When method signature has `func() -> U` and arg is closure `do() -> I32`, type param `U` was never extracted
- Fix: Added FuncType-vs-ClosureType matching case in both `extract_type_params` functions
- Affected: `map_or_else[U]`, any method with generic U inferred from closure params
- Key insight: impl-level generics (T) + method-level generics (U) combine in type_params as ["T","U"]
  - Position 0 maps to receiver type_args[0] correctly
  - Position 1+ must be inferred from args via `extract_type_params`

### Old Suite System %struct.This/%struct.T: DEFUNCT
- The old tester (`compiler/src/cli/tester/`) is deleted
- The v3 system (`compiler/src/testing/`) uses per-file QueryContext
- `%struct.This` fallback at `llvm_types.cpp:755-759` is produced when `current_impl_type_` is empty
- `%struct.T` in generics is from stale `current_type_subs_` across merged files
- Neither pattern is reproducible in the current test infrastructure

### Active Bug: toowned_assoc.test.tml local/library conflation
- **File**: `generate.cpp:1006-1011` -- primitive `this` override ignores `ref This`
- **Mechanism**: Both local and library `impl ToOwned for I32` produce the same Itanium name `tml_N4core3I328to_ownedE` because `find_module_for_type("I32")` returns `"core"` for ALL I32 impls
- **Conflict**: Library has `to_owned(this)` (by-value), local has `to_owned(this: ref This)` (pointer)
- The `this`/`self` override at generate.cpp:1006-1011 unconditionally forces by-value for immutable this on primitives, even when the declared type is `ref This`
- Same override exists at `impl.cpp:234-246` (gen_impl_method path)
- Fix: Check `method.params[i].type->is<parser::RefType>()` before applying primitive override

### Code Generation Ordering
1. Library modules: `emit_module_pure_tml_functions()` at generate.cpp:588
2. Library impls registered in `functions_` with lazy deferred bodies
3. Local structs/enums/consts: first pass at generate.cpp:738+
4. Pre-register local function signatures: generate.cpp:826-853
5. Local impl blocks: generate.cpp:855+ with inline codegen (generate.cpp:943-1097)
6. Local functions: generate.cpp:855+
7. Lazy deferred library bodies: `emit_referenced_library_definitions()` at runtime_modules.cpp:1061
8. Vtables: `emit_vtables()` at generate.cpp:1310+

### Name Mangling for Primitives
- `mangle_impl_method("I32", "to_owned")` always produces `tml_N4core3I328to_ownedE`
- This is because `find_module_for_type("I32")` returns `"core"` from the `builtin_modules` table at `llvm_utils.cpp:372-382`
- Both library and local impls for I32 get the SAME mangled name
- The `functions_` map uses flat key `"I32_to_owned"` which also collides
