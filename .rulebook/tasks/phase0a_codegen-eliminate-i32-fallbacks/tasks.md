# Tasks: Eliminate i32 Fallbacks — Make Missing Types Visible

**Status**: New. 0% (0/18). **Priority**: CRITICAL
**Reference**: `docs/analyses/codegen/02-TYPE-SYSTEM.md`

## 1. MIR Validation Pass

- [ ] 1.1 Create `compiler/src/mir/mir_validate.cpp` + header
- [ ] 1.2 Add `validate_types()` — assert every non-void instruction has non-null `inst.type`
- [ ] 1.3 Add `validate_terminators()` — assert return values match function return type
- [ ] 1.4 Add `validate_blocks()` — assert every block has a terminator
- [ ] 1.5 Wire into pipeline after MIR building, before codegen (debug: warn, release: log)
- [ ] 1.6 Build compiler, verify no regressions

## 2. Annotate Fallbacks with Warnings

- [ ] 2.1 `instructions.cpp`: add `TML_LOG_WARN` before each `make_i32_type()` fallback (4 sites)
- [ ] 2.2 `instructions_call.cpp`: add warnings (1 site)
- [ ] 2.3 `instructions_misc.cpp`: add warnings (13 sites)
- [ ] 2.4 `instructions_method.cpp`: add warnings (2 sites)
- [ ] 2.5 Build + run `core/str` + `core/fmt` + `std/json` — catalog which warnings fire
- [ ] 2.6 Save warning list to `.sandbox/i32_fallback_warnings.log`

## 3. Fix Top MIR Builders

- [ ] 3.1 Fix top 5 warning sites in `hir_mir_builder.cpp` (set types on instructions)
- [ ] 3.2 Fix top 5 warning sites in `thir_mir_builder.cpp`
- [ ] 3.3 Fix top 5 warning sites in `thir_mir_builder_expr.cpp`
- [ ] 3.4 Run full test suite — verify warning count decreased
- [ ] 3.5 Update `docs/analyses/codegen/02-TYPE-SYSTEM.md` with remaining count
