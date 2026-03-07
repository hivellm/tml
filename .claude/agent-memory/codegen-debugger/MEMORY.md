# Codegen Debugger Memory

## Index
- [suite-merging-investigation.md](suite-merging-investigation.md) - Full findings on the %struct.This/%struct.T and toowned_assoc bugs
- [this-parameter-conventions.md](this-parameter-conventions.md) - How `this`/`self` parameter types are determined in codegen
- [builtin-enum-monomorphization.md](builtin-enum-monomorphization.md) - Maybe[T]::default() fails: built-in enums not recognized as library types
- [array-mut-this-dispatch.md](array-mut-this-dispatch.md) - Array `mut this` method dispatch failure: 3 root causes

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

### Default Behavior Method FuncType Params (2026-03-03) -- FIXED
- **Bug**: `generate_default_method()` in `dyn.cpp:420-428` skipped methods with `FuncType` params
- **Effect**: `for_each`, `map`, `filter`, etc. emitted as panic stubs instead of real bodies
- **Call-site bug**: `method_impl.cpp:600` defaults `expected_type="i32"` when `func_sig` is null
  - `env_.lookup_func("Counter::for_each")` returns nullopt for default behavior methods
  - Closure `{ ptr, ptr }` was passed as `i32` type in the call instruction
- **Fix 1**: Removed `has_func_ptr_param` check in `dyn.cpp` -- param resolution already handles FuncType correctly
  - `resolve_parser_type_with_subs` maps `func(This::Item)` -> `types::FuncType(params=[I32])`
  - `llvm_type_from_semantic` maps FuncType -> `"{ ptr, ptr }"`
  - `gen_block` handles fat pointer closure calls via `call.cpp:1464` dispatch
- **Fix 2**: Added fallback in `method_impl.cpp:612` to use `method_it->second.param_types`
  when `func_sig` is unavailable (default behavior methods registered in `functions_` but not in type env)
- **Still broken (separate bug)**: `%struct.Fn` in library generic instantiation for `repeat_with[F]`, `once_with[F]`, `map[I, F]`
  - When generic type param `F` resolves to a closure, it gets mangled as `"Fn"` -> `%struct.Fn`
  - Should be `{ ptr, ptr }` but `llvm_type_from_semantic` on a `NamedType("Fn")` returns `%struct.Fn`

### ManuallyDrop Generic Codegen: Two Bugs Fixed (2026-03-03)
- **Bug 1**: `call_generic_struct.cpp` type inference only handled 3 cases for matching arg types to param types
  - Case 3 handles `NamedType[X]` vs `NamedType[T]` but NOT when wrapped in `RefType`
  - For `take(slot: mut ref ManuallyDrop[T])`, param is `RefType{mut, NamedType{...}}` -- none matched
  - **Fix**: Added Case 4 in `call_generic_struct.cpp:711+` to unwrap RefType from both arg and param
  - Also added same fix in `method_static_dispatch.cpp:681+` for dot-notation calls
- **Bug 2**: `unary.cpp:174` sets `struct_ptr = local_it->second.reg` for `ref slot.value`
  - For ref params, the alloca holds a POINTER to the struct, not the struct itself
  - GEP was applied to the alloca directly, reading pointer bytes as struct data
  - **Fix**: Added load of pointer from alloca when local is RefType at `unary.cpp:174+`
  - Key check: `local_it->second.type == "ptr" && base_type->is<types::RefType>()`
- **Note**: `drop_in_place` intrinsic is NOT implemented in codegen (silently no-ops)

### Maybe[T] Generic Method Codegen: 6 Bugs Fixed (2026-03-03)
- See [maybe-method-codegen.md](maybe-method-codegen.md) for full details
- **Bug 1**: `core::option` missing from `essential_library_modules` in `runtime_modules.cpp:317`
  - Fix: Added `"core::option"` and `"core::result"` to the list
- **Bug 2**: Bare type parameter inference missing in `method_impl.cpp:160-171`
  - For `ok_or(this, err: E)`, E was never inferred from args
  - Fix: Added "Case 1: Bare type parameter" check for `param_named.name == type_param`
- **Bug 3**: `contains(value: ref U)` inline codegen didn't deref pointer arg
  - Fix: Added load of inner_llvm_type from ptr when `cmp_arg_type == "ptr"`
- **Bug 4**: `flatten(this) -> Maybe[T]` return type was wrong in `option.tml`
  - Fix: Changed return type to `-> T` (generic T, not Maybe[T])
- **Bug 5**: `llvm_ir_gen_stmt.cpp:616` stored `nullptr` as semantic_type for generic enum unit variants
  - When `let x: Maybe[Maybe[I32]] = Nothing`, the `Nothing` path stores `nullptr` for semantic_type
  - This causes `infer_expr_type` to produce wrong flat type `Maybe[Maybe, I32]` instead of `Maybe[Maybe[I32]]`
  - Fix: Store `semantic_var_type` (from annotation) instead of `nullptr`
- **Bug 6**: `filter` inline codegen for primitives -- `is_ptr_to_value` vs explicit deref conflict
  - For `ref T` closure params, primitive path binds value directly (type="i32")
  - This works for auto-deref `x >= 5` but NOT for explicit `*v > 0`
  - Kept primitive path for backwards compat; explicit deref pattern requires separate fix

### Built-in Enum Monomorphization: Maybe[T]::default() (2026-03-06) -- OPEN
- See [builtin-enum-monomorphization.md](builtin-enum-monomorphization.md) for full details
- **Bug 1**: `method_static_dispatch.cpp:747-764` -- `is_imported` checks `mod.enums` but Maybe is a
  built-in enum (created at `generate.cpp:263-282`), NOT in any module's enums map -> is_library_type=false
- **Bug 2**: `generic.cpp:569-574` -- module search filter only checks `mod.structs`/`mod.internal_structs`,
  not `mod.enums`. Combined with is_library_type=false, ALL modules skipped, `core::default` never parsed
- **Secondary**: `pending_generic_impls_` is single-valued map (runtime_modules.cpp:1001), 10+ impl[T]
  blocks for Maybe overwrite each other; system relies on fallback module search which fails due to bugs 1+2
- **Affects**: ALL built-in enums (Maybe, Outcome, Ordering, Poll, ControlFlow)
- **Fix 1**: Check `pending_generic_enums_` in is_imported computation at method_static_dispatch.cpp:751
- **Fix 2**: Check `mod.enums` in generic.cpp:569-572 has_struct computation
- **Debug**: `TML_LOG=debug tml.exe build file.tml --emit-ir 2>&1 | grep IMPL_INST`

### Name Mangling for Primitives
- `mangle_impl_method("I32", "to_owned")` always produces `tml_N4core3I328to_ownedE`
- This is because `find_module_for_type("I32")` returns `"core"` from the `builtin_modules` table at `llvm_utils.cpp:372-382`
- Both library and local impls for I32 get the SAME mangled name
- The `functions_` map uses flat key `"I32_to_owned"` which also collides
