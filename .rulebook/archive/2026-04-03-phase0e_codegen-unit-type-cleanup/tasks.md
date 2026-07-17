# Tasks: Unit Type Cleanup — Consistent {} Representation

**Status**: COMPLETE. 100% (12/12). **Priority**: MEDIUM

## 1. Core Change

- [x] 1.1 In `mir_types.cpp`: change `PrimitiveType::Unit` mapping from `"void"` to `"{}"` — done
- [x] 1.2 Add `is_unit_type()` helper on MirCodegen in `codegen_helpers.cpp` — checks MirPrimitiveType::Unit
- [x] 1.3 In `emit_function()`: convert "{}" to "void" for return type and current_func_ret_type_ — done
- [x] 1.4 In `emit_function_declaration()`: convert "{}" to "void" for return type — done
- [x] 1.5 Build + run `core/str` 25/25, `core/fmt` 46/46, `core/ops` 47/47 — all pass

## 2. Remove Void Patches

- [x] 2.1 Removed `if (type_str == "void") { type_str = "{}"; }` from `LoadInst` handler
- [x] 2.2 Updated `StoreInst` handler to check `"{}"` instead of `"void"` for skip
- [x] 2.3 Removed void-to-{} patch from `AllocaInst` handler
- [x] 2.4 Removed void-to-{} patch from `PhiInst` handler in `instructions_misc.cpp`
- [x] 2.5 Removed void-to-{} param patches from both `emit_function()` and `emit_function_declaration()` + removed void-to-{} arg patches from `instructions_call.cpp` and `instructions_method.cpp` (2 sites each) + updated receiver void check to "{}" in `instructions_method.cpp` + added "{}" to "void" conversion for call ret_types in `instructions_call.cpp` (indirect calls) and `instructions_method.cpp` (3 sites)
- [x] 2.6 Build + run `core/str` 25/25, `core/fmt` 46/46, `core/num` 53/53 — all pass. std/json has pre-existing module resolution failures (unrelated)

## 3. Return Path Fix

- [x] 3.1 In `terminators.cpp` ReturnTerm: simplified — "{}" and current_func_ret_type_=="void" both emit `ret void`; removed redundant separate void and {} branches
- [x] 3.2 Tested core/str 25/25, core/fmt 46/46, core/ops 47/47, core/num 53/53, core/error 35/35 — zero regressions
