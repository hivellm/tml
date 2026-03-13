# Codegen Debugger Memory

## Index
- [suite-merging-investigation.md](suite-merging-investigation.md) - Full findings on the %struct.This/%struct.T and toowned_assoc bugs
- [this-parameter-conventions.md](this-parameter-conventions.md) - How `this`/`self` parameter types are determined in codegen
- [builtin-enum-monomorphization.md](builtin-enum-monomorphization.md) - Maybe[T]::default() fails: built-in enums not recognized as library types
- [array-mut-this-dispatch.md](array-mut-this-dispatch.md) - Array `mut this` method dispatch failure: 3 root causes

## Recent Fixes

### Constrained Generic Behavior Methods Returning () (2026-03-09) -- FIXED
- **Bug**: `hash_it[T: Hash](val: ref T) -> I64` returned `()` instead of `I64`
- **Two root causes**:
  1. **Type checker**: `TypeEnv::lookup_behavior()` in `env_lookups.cpp` didn't search `GlobalModuleCache`
     for standalone files (no `use` imports). Behavior definitions like Hash, PartialEq not found.
     Fix: Added GlobalModuleCache fallback search after module_registry_ search (matching `lookup_func` pattern).
  2. **Codegen routing**: `has_local_generics` detection in `query_core.cpp:583` and `build.cpp:323` only
     checked for generic structs/enums, NOT generic functions. Files with `func foo[T: Hash](...)` went
     through MIR codegen which can't monomorphize. Fix: Added `FuncDecl` generics check.
- **Files**: `env_lookups.cpp` (type checker), `query_core.cpp` + `build.cpp` (codegen routing)
- **Impact**: Unblocks ~57 library functions using constrained generics
- **DLL locking workaround**: When zombie tml.exe processes lock `tml_compiler.dll`, manually link
  to `.sandbox/` via `vcvarsall.bat x64 && link.exe @link.rsp` and use `TML_PLUGIN_DIR` or copy
  exe+plugins to sandbox dir (exe looks for plugins at `exe.parent_path()/../plugins/`)

### ListIter Double-Free Fix (2026-03-08) -- FIXED (library-level)
- **Bug**: `ListIter[T]` stored `list: List[T]` by value. Since `List[T]` is `{ handle: *Unit }`,
  copying it copies the handle. Both original and copy's Drop free the same handle → double-free.
- **Fix**: Changed `ListIter` to store raw `handle: *Unit`, `len: I64`, `stride: I64`, `index: I64`
  instead of `list: List[T]`. No ownership of List, no Drop conflict.
- **File**: `lib/std/src/collections/behaviors.tml` lines 326-368
- **Pattern**: Any iterator storing a collection by value will double-free. Use raw handle instead.
- **Note**: This was a library fix, not a compiler fix. TML lacks move semantics, so `into_iter(this)`
  always copies. Iterators must store raw pointers to avoid owning the container.

### Specialized Impl Type Param Substitution: ref ref T Bug (2026-03-08) -- FIXED
- **Bug**: `impl[T] Pin[ref T]` methods returned `ref ref I32` instead of `ref I32`
- **Root cause**: `expr_call_method.cpp:399` positionally mapped impl type params to struct type_args
  - For `Pin[ref I32]`, `named.type_args[0] = RefType(I32)`, mapped directly to T
  - But T should be I32 (inner of ref), not ref I32 (the full type arg)
- **Fix 1**: Added `impl_self_type_args` field to `FuncSig` (env.hpp)
  - Stores impl self-type arg patterns (e.g., `[RefType(NamedType("T"))]` for `impl[T] Pin[ref T]`)
  - Populated at: `core.cpp:check_impl_decl`, `env_module_support.cpp` module registration
  - Serialized in: `module_binary.cpp/module_binary_read.cpp`
- **Fix 2**: `build_receiver_subs` helper in `expr_call_method.cpp` uses `extract_type_params`
  to pattern-match impl self-type args against concrete type args (instead of positional mapping)
- **Fix 3**: `check_struct_expr` in `types_checker.cpp` now infers type_args for generic struct literals
  (was returning empty type_args, causing all generic method calls on local structs to fail)
- **Binary cache**: Format changed; old `.tml.meta` files must be cleared after this fix
- **Still blocked**: AST codegen generates wrong struct type name `Pin__mutref_T` instead of `Pin__ref_I32`

### Const Generic Monomorphization in Struct Layout (2026-03-08/12) -- FIXED
- **Bug**: `[T; N]` fields in generic structs emitted `[0 x T]` instead of `[N x T]`
- **Root causes** (multi-layered fix across sessions):
  1. `resolve_simple_type` in `env_module_support.cpp:788` only handled `LiteralExpr` array sizes,
     ignoring `IdentExpr("N")` for const generic params → `ArrayType{size=0, const_generic_param=""}`
  2. `types::ArrayType` lacked `const_generic_param` field to track which const param controls size
  3. `apply_type_substitutions` and `substitute_type` didn't resolve const generic sizes
  4. `require_struct_instantiation` module registry path only mapped `type_params` to `subs`,
     not `const_params` → const generic N never in substitution map
  5. `module_binary_read.cpp` deserialization didn't parse `@ParamName` suffix from array size
  6. Multiple codegen paths (type checker, HIR builder, gen_ident, method_static_dispatch,
     call_generic_struct, runtime_modules deferred bodies, impl.cpp) needed const generic handling
- **Files changed**: `type.hpp`, `type.cpp` (substitute_type + substitute_type_with_consts),
  `env_module_support.cpp` (resolve_simple_type), `llvm_struct_decl.cpp` (const_params mapping),
  `module_binary_read.cpp` (@ParamName parsing), `types_resolve.cpp`, `resolve.cpp`,
  `llvm_types.cpp`, `llvm_struct_expr.cpp`, `core.cpp` (gen_ident), `method_static_dispatch.cpp`,
  `call_generic_struct.cpp`, `runtime_modules.cpp`, `impl.cpp`
- **Key discovery**: Library struct field types go through `resolve_simple_type` (NOT `resolve_type`)
  in `env_module_support.cpp`. This is a lightweight resolver used during module registration.
- **Cache impact**: Old `.tml.meta` files must be regenerated after this fix (field type format changed)

### Struct Field ptr-to-struct Load Missing (2026-03-08) -- FIXED
- **File**: `compiler/src/codegen/llvm/expr/llvm_struct_expr.cpp`
- **Bug**: `gen_struct_expr_ptr` had 3 code paths for struct construction (Self, current_ret_type_, generic)
  - Only the generic path (line ~701) had ptr-to-struct load fixup
  - The Self path and current_ret_type_ path were missing it
- **Symptom**: `into_iter(this)` on `List[T]` emitted `store %struct.List__I32 %this, ptr %field` where `%this` is `ptr` type
- **Root cause**: Immutable `this` on structs is passed as `ptr`, but struct init field store used the ptr value directly as struct type
- **Fix**: Added `if (last_expr_type_ == "ptr" && field_type.starts_with("%struct."))` load fixup to Self and current_ret_type_ paths
- **Unblocked**: ListIter[T], into_iter, iter -- codegen correct, but runtime double-free remains (separate issue)

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

### Nested Enum Layout: Maybe[Maybe[I32]] (2026-03-08) -- FIXED
- **Bug**: `calc_type_size` in `enum.cpp:417-450` computed inner enum size using old `{ i32, [N x i64] }` formula
  even though compact layout optimization (lines 480-498) produces `{ i32, i32 }` for small payloads
- **Effect**: `Maybe[Maybe[I32]]` was `{ i32, [2 x i64] }` (20 bytes) instead of `{ i32, i64 }` (16 bytes)
- **Root cause**: Inner enum payload size 4 -> `inner_num_i64=1` -> `return 8 + 1*8 = 16` bytes,
  but this was the raw inner enum size, not accounting for compact layout
- **Fix**: Replaced lines 444-450 with compact-layout-aware sizing (tag-only=4, <=4=8, <=8=16, else formula)
- **Also fixed**: Non-generic `calc_type_size` (line 185-200) had no enum type lookup at all; added
  `enum_instantiations_` + `enum_payload_type_` lookup for nested enum-in-enum cases

### Name Mangling for Primitives
- `mangle_impl_method("I32", "to_owned")` always produces `tml_N4core3I328to_ownedE`
- This is because `find_module_for_type("I32")` returns `"core"` from the `builtin_modules` table at `llvm_utils.cpp:372-382`
- Both library and local impls for I32 get the SAME mangled name
- The `functions_` map uses flat key `"I32_to_owned"` which also collides
