# Compiler Optimizer Agent Memory

## Name Mangling Architecture

### Script-Level Function Disambiguation (2026-03-01)
Script functions (empty `current_module_name_`) are NOT mangled by `mangle_tml_symbol` -- it returns the bare function name (line 192 of `llvm_utils.cpp`). Instead, disambiguation relies on:
- **Suite prefix**: `get_suite_prefix()` in `llvm_utils.cpp:324` returns `"s<N>_"` when `suite_test_index >= 0 && force_internal_linkage && current_module_prefix_.empty()`
- **Internal linkage**: `force_internal_linkage` makes all functions `define internal` in suite/DLL mode
- **Single compilation unit**: Standalone builds have no collision risk

Phase 4 (Script-Level Function Mangling) was DEFERRED -- suite prefix provides sufficient uniqueness. See `F:\Node\hivellm\tml\.rulebook\tasks\complete-name-mangling\tasks.md`.

### Key Mangling Functions
- `mangle_tml_symbol(module, func)` in `llvm_utils.cpp:179` -- Itanium-style `N<len><seg>...E` encoding
- `mangle_tml_symbol(module, func, param_types)` in `llvm_utils.cpp:213` -- adds `_<type codes>` suffix
- `mangle_impl_method(type, method)` in `llvm_utils.cpp:~425` -- for impl methods
- `get_suite_prefix()` in `llvm_utils.cpp:324` -- `s<N>_` for script-level functions in suite mode
- `mangle_type_code(type)` in `llvm_utils.cpp:231` -- single-letter Itanium type encoding

### get_suite_prefix() Usage (15+ sites)
Used in: `call_user.cpp`, `method_impl.cpp`, `method_generic.cpp`, `method.cpp`, `method_outcome.cpp` (7 sites), `dyn.cpp`, `optimization_passes.cpp`, `func.cpp`

## CGValue Integration — COMPLETE (2026-04-03)

### Phase 4 Complete: value_types_ and value_spill_allocas_ removed
- `cg_values_` is now the SOLE type-tracking map in MirCodegen
- `value_types_` declaration removed from mir_codegen.hpp
- `value_spill_allocas_` declaration removed from mir_codegen.hpp
- All 78+ read sites across 5 .cpp files migrated to cg_values_.find() / .llvm_type
- All write sites removed (were redundant with cg_values_ writes)
- Spill tracking uses CGValue::address() + CGValueKind::Address check
- Parameter type tracking in mir_codegen.cpp emit_function() uses cg_values_ writes
- Struct param ptr override uses CGValue::address() with orig struct type as pointee
- Zero regressions: core/str 25/25, core/fmt 46/46, core/ops 47/47, core/num 53/53

### Key Insight: CGValue kind assignments
- AllocaInst → Address (pointee = alloc_type)
- GEP → Address (pointee = base_type)  
- GEP spill → overwrites base CGValue to Address (pointee = spill_type)
- Struct param ptr override → Address (pointee = original struct type)
- LoadInst → Immediate
- ConstUnit → ZeroSized
- All others → Immediate (even aggregates — they're SSA values, not pointers)

## Key File Locations
- Func pre-registration: `compiler/src/codegen/llvm/decl/func.cpp:37` (pre_register_func)
- Mangling utils: `compiler/src/codegen/llvm/core/llvm_utils.cpp:179`
- Suite config: `compiler/src/cli/commands/cmd_test.cpp:233` (max_per_suite = coverage ? 1 : 10)
- Suite grouping: `compiler/src/testing/testing_discovery.cpp:207` (group_into_suites)

## Cross-Module Enum Resolution (2026-04-08)

Three parallel resolver sites must all consult both `ModuleInfo::enums`
AND `ModuleInfo::internal_enums` for non-`pub` imported enums. The fix
pattern is the same 3-tier fallback (lookup_enum → module_registry FQN →
resolve_imported_symbol + get_module with both maps):

- `checker/stmt.cpp` bind_pattern EnumPattern branch — fixed in Phase 5 (C3)
- `checker/types_checker.cpp` check_path segments.size()==2 branch — fixed in Phase 6 (C6)
- `checker/resolve.cpp` resolve_type_path — already had full fallback

Why this matters: non-`pub` enums stored in `ModuleInfo::internal_enums`
(via `env_module_load_decls.cpp:340-349`), `lookup_enum` walks
`all_modules` and checks both maps, but direct `get_module(path)->enums`
lookups bypass that and must check `internal_enums` explicitly.

## Known Silent Codegen Failures

- **Heap[T] auto-import**: When a struct field has type `Heap[T]` but the
  containing module does not explicitly `use core::alloc::heap::Heap`,
  `require_struct_instantiation` in `llvm_struct_decl.cpp:224` fails
  silently through all 3 lookup paths (pending_generic_structs_,
  module_registry, GlobalASTCache) and returns the mangled name
  unconditionally at line 695. LLVM then emits a reference to
  `%struct.Heap__T` that is never defined. Separate bug, not Phase 0p.
