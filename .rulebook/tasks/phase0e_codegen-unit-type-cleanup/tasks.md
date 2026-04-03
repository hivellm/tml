# Tasks: Unit Type Cleanup — Consistent {} Representation

**Status**: New. 0% (0/12). **Priority**: MEDIUM — depends on phase0a
**Reference**: `docs/analyses/codegen/07-RECOMMENDED-CHANGES.md` (Priority 6)

## 1. Core Change

- [ ] 1.1 In `mir_types.cpp`: change `PrimitiveType::Unit` mapping from `"void"` to `"{}"`
- [ ] 1.2 Add `is_unit_return()` helper on MirCodegen — returns true for Unit return type
- [ ] 1.3 In `emit_function()`: use `"void"` for return type only when `is_unit_return()`
- [ ] 1.4 In `emit_function_declaration()`: same — `"void"` only for return type
- [ ] 1.5 Build + run `core/str` — verify basic tests pass

## 2. Remove Void Patches

- [ ] 2.1 Remove `if (type_str == "void") { type_str = "{}"; }` from `LoadInst` handler
- [ ] 2.2 Remove same patch from `StoreInst` handler
- [ ] 2.3 Remove same patch from `AllocaInst` handler
- [ ] 2.4 Remove same patch from `PhiInst` handler
- [ ] 2.5 Remove void→{} conversions in `emit_function_declaration()` parameter handling
- [ ] 2.6 Build + run `core/str` + `core/fmt` + `std/json` — verify no regressions

## 3. Return Path Fix

- [ ] 3.1 In `emit_terminator()` ReturnTerm: simplify void/unit handling (only check `is_unit_return()`)
- [ ] 3.2 Run full test suite — verify zero regressions
