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
