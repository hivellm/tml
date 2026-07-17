# Tasks: Eliminate i32 Fallbacks — Make Missing Types Visible

**Status**: Complete. 100% (18/18). **Priority**: CRITICAL
**Reference**: `docs/analyses/codegen/02-TYPE-SYSTEM.md`

## 1. MIR Validation Pass

- [x] 1.1 Create `compiler/src/mir/mir_validate.cpp` + header — commit 9038ae94
- [x] 1.2 Add `validate_types()` — checks non-void instructions for null type
- [x] 1.3 Add `validate_terminators()` — checks every block has a terminator
- [x] 1.4 Add `validate_blocks()` — checks entry block exists, no empty blocks
- [x] 1.5 Wire into pipeline after MIR building, before codegen — query_core.cpp
- [x] 1.6 Build compiler, verify no regressions — core/str 25/25, core/fmt 46/46

## 2. Annotate Fallbacks with Warnings

- [x] 2.1 `instructions.cpp`: 7 [CG-I32] warnings added — commit cd7494f4
- [x] 2.2 `instructions_call.cpp`: 1 [CG-I32] warning added
- [x] 2.3 `instructions_misc.cpp`: 13 [CG-I32] warnings added
- [x] 2.4 `instructions_method.cpp`: 2 [CG-I32] warnings added
- [x] 2.5 Tested core/str + core/fmt + std/json — ZERO warnings fire (MIR builders set types correctly for common patterns)
- [x] 2.6 Result: fallbacks are defense-in-depth, not actively masking bugs in standard suites

## 3. Fix Top MIR Builders

- [x] 3.1 Fix top 5 warning sites in `hir_mir_builder.cpp` — N/A: zero [CG-I32] warnings fire across full test suite (1659 tests). MIR builders set types correctly for all existing code.
- [x] 3.2 Fix top 5 warning sites in `thir_mir_builder.cpp` — N/A: same result
- [x] 3.3 Fix top 5 warning sites in `thir_mir_builder_expr.cpp` — N/A: same result
- [x] 3.4 Run full test suite — verified: zero [CG-I32] warnings across ALL suites
- [x] 3.5 Conclusion: i32 fallbacks are purely defensive, never fire. MIR validation pass + annotations provide safety net for future code.
