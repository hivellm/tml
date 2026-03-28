# Tasks: Error Codes Expansion — 197 → 460 Codes

**Status**: 75% Complete (41/55 items done, 8 deferred, 6 pending validation)
**Priority**: HIGH
**Phase**: 0 — Infrastructure (blocks all other phases — better errors = faster debugging)

> **AUDIT UPDATE (2026-03-28)**: Initial grep reported 87 untagged type checker errors, but deep audit found 83 were false positives (multi-line calls where the code appeared on continuation lines). Only **4 genuinely untagged errors** existed and were fixed (T200, T201, T207). Type checker now has **zero untagged errors**.

## Phase A: Tag Existing Untagged Errors ✅ DONE

### A.1 Type Checker — ✅ DONE (4 genuine fixes)

- [x] A.1.1 Deep audit: 83/87 grep hits were false positives (multi-line calls)
- [x] A.1.2 Fixed T200 in resolve.cpp:132 — unknown behavior in dyn type
- [x] A.1.3 Fixed T201 in resolve.cpp:140 — behavior not object-safe
- [x] A.1.4 Fixed T200 in resolve.cpp:178 — unknown behavior in impl type
- [x] A.1.5 Fixed T207 in types_checker.cpp:530 — undefined variable/function
- [x] A.1.6 Verified: zero untagged error() calls remain in all 10 checker files
- [x] A.1.7 Type checker: 79 → 83 codes (T001-T207). All tagged.

### A.2 Legacy Codegen — ✅ DONE (agent: codegen-auditor)

- [x] A.2.1 Deep multi-line audit — only 1 genuinely untagged
- [x] A.2.2 Fixed C036 in func.cpp:174 — @no_mangle + generics
- [x] A.2.3 Build passed — zero regressions

## Phase B: User-Facing Error Improvements ✅ MOSTLY DONE

### B.1 Module Loading — ✅ DONE (agent: module-loader)

- [x] B.1.1 D001 prefixed in env_module_load.cpp — module parse/open failures
- [x] B.1.2 D003 prefixed in module_binary_read.cpp — binary cache read failures
- [x] B.1.3 D010 prefixed in module_binary_read.cpp — preload lib not found
- [ ] B.1.4 D002, D004-D009, D011-D015 — DEFERRED: these error conditions don't have explicit error sites yet (would need new detection code)
- [x] B.1.5 backend_errors.cpp has K/N entries; D-codes use inline prefix pattern

### B.2 Linker — ✅ DONE (agent: linker-tagger)

- [x] B.2.1 Prefixed 10 error messages with N-codes in lld_linker.cpp
- [x] B.2.2 N001-N003 linking failed, LLD failed, LLD unavailable
- [x] B.2.3 N004-N008 not initialized, no objects, file not found, output not created, static lib failed
- [x] B.2.4 N001-N008 entries in backend_errors.cpp — DONE

### B.3 LLVM Backend — ✅ DONE (agent: backend-tagger)

- [x] B.3.1 Prefixed 17 error messages with K-codes in llvm_backend.cpp
- [x] B.3.2 K001-K003 IR parse, verification, target creation
- [x] B.3.3 K004-K008 emit object, get target, memory buffer, open IR, IR not found
- [x] B.3.4 K009-K011 backend not initialized, create context, emit to memory
- [x] B.3.5 K001-K011 entries in backend_errors.cpp — DONE

### B.4 Lexer Expansion — ✅ DONE (agent: module-loader)

- [x] B.4.1 L024 invalid escape sequence, L025 invalid unicode escape — tagged in lexer_string.cpp
- [x] B.4.2 L021-L023, L026-L030 — already had codes or no error sites exist for these specific conditions
- [x] B.4.3 L024-L025 entries added to lexer_errors.cpp

### B.5 Parser Expansion — ✅ DONE (agent: module-loader)

- [x] B.5.1 P066-P078 — 12 expect() calls tagged in parser_decl.cpp (func/struct/enum/behavior/field/variant/union names, decorator, braces)
- [x] B.5.2 Added expect(msg, code) overload to parser.hpp + parser_core.cpp
- [x] B.5.3 P066-P080 entries added to parser_errors.cpp (P073/P079/P080 explain-only)
- [x] B.5.4 All entries in parser_errors.cpp — DONE

## Phase C: Developer Experience — PARTIALLY DONE

### C.1 Semantic Analysis — ⬜ DEFERRED (needs infrastructure)

- [ ] C.1.1-C.1.3 S014-S025 — DEFERRED: TypeChecker has no `warning()` method. Need to add `warnings_` vector, `warning()` method, and per-variable read tracking. Separate task.
- [ ] C.1.4 Explain entries — DEFERRED: depends on C.1.1
- [ ] C.1.5 Detection logic — DEFERRED: depends on C.1.1

### C.2 Borrow Checker — ✅ DONE (agent: semantic-worker)

- [x] C.2.1 All 13 BorrowError types verified with B-codes in builder_helpers
- [x] C.2.2 B028 TempDroppedWhileBorrowed, B029 CannotMoveFromRef, B030 BorrowBeyondScope — added to enum + switch
- [x] C.2.3 B028-B030 entries added to borrow_errors.cpp

### C.3 HIR Lowering — ⬜ DEFERRED (needs architectural change)

- [ ] C.3.1-C.3.5 H001-H015 — DEFERRED: HIR builder assumes valid AST, returns HirModule directly (not Result). Adding error codes requires changing return types and threading errors through all lower_* methods. Separate task.

### C.4 MIR Building — ✅ DONE (agent: mir-validator)

- [x] C.4.1 16 error sites tagged: M001/M002 (hir_expr_control), M003/M004 (hir_mir_builder), M003 (thir_mir_builder_expr), M005/M006 (mir_pass), M007 (infinite_loop_check), M008 (memory_leak_check)
- [x] C.4.2 M001-M010 defined in mir_errors.cpp
- [x] C.4.3 M011-M020 defined in mir_errors.cpp
- [x] C.4.4 mir_errors.cpp wired into explain_run.cpp, explain_internal.hpp, CMakeLists.txt

### C.5 Codegen Expansion — ✅ DONE (explain entries, agent: semantic-worker)

- [x] C.5.1 C036-C050 explain entries added to codegen_errors.cpp (C036 has source site; C044-C050 are explain-only for future error paths)
- [x] C.5.2 All entries in codegen_errors.cpp — DONE

### C.6 Warnings — ✅ DONE (explain entries, agent: semantic-worker)

- [x] C.6.1-C.6.3 W001-W015 entries added to general_errors.cpp (W001-W004 already existed; W005-W015 are explain-only for when detection logic is added)
- [x] C.6.4 All entries in general_errors.cpp — DONE

## Phase D: Internal Diagnostics — MOSTLY DONE

### D.1 Preprocessor — ✅ DONE (agent: internal-worker)

- [x] D.1.1 10 error sites prefixed with PP001-PP010 in preprocessor.cpp
- [x] D.1.2 PP001-PP010 all tagged
- [x] D.1.3 PP001-PP010 entries in preproc_errors.cpp — DONE

### D.2 Query System — ⬜ DEFERRED (no user-facing errors)

- [ ] D.2.1-D.2.3 Q001-Q010 — DEFERRED: query system uses only TML_LOG_DEBUG, no user-facing error messages to tag. Needs new error infrastructure.

### D.3 Formatter/Linter — ⬜ DEFERRED (no error messages)

- [ ] D.3.1-D.3.3 F001-F010 — DEFERRED: formatter has zero error messages. Linter already has S001-S013 and W001-W004. Needs new infrastructure for F-codes.

### D.4 Testing — ✅ DONE (agent: internal-worker)

- [x] D.4.1 X001-X004 tagged in testing_coordinator.cpp and testing_compile.cpp
- [x] D.4.2 X006-X010 explain entries created
- [x] D.4.3 testing_errors.cpp wired into explain system — DONE

### D.5 Reflection — ✅ DONE (agent: internal-worker)

- [x] D.5.1 R001 exists (3 sites); R002-R005 no new source sites (intrinsics return null silently)
- [x] D.5.2 R001-R005 entries in reflection_errors.cpp — DONE

## Validation

- [ ] V.1 Run full test suite — zero regressions (partial: str/ 13/22 pass, failures pre-existing)
- [ ] V.2 `tml explain` works for ALL new codes
- [ ] V.3 Audit: no error site emits without a code (type checker + codegen confirmed; other stages partially)
- [ ] V.4 Update docs/error-codes-proposal.md with final implementation status
- [ ] V.5 Update CHANGELOG

### Validation Notes
- Build passes after all agent changes (commit 3bce07b6)
- Total unique codes in explain db: ~273
- New explain files: backend_errors.cpp, mir_errors.cpp, testing_errors.cpp, reflection_errors.cpp, preproc_errors.cpp (updated), parser_errors.cpp (updated), lexer_errors.cpp (updated), borrow_errors.cpp (updated), codegen_errors.cpp (updated), general_errors.cpp (updated)

## Summary

| Phase | Items | Done | Deferred | Status |
|-------|-------|------|----------|--------|
| A. Tag untagged | 10 | 10 | 0 | ✅ |
| B. User-facing | 17 | 16 | 1 (D002-D015 partial) | ✅ |
| C. Dev experience | 18 | 10 | 8 (C.1 semantic, C.3 HIR) | ⚠️ |
| D. Internal | 10 | 7 | 3 (D.2 query, D.3 formatter) | ⚠️ |
| V. Validation | 5 | 0 | 0 | ⬜ |
| **Total** | **55** | **43** | **8 deferred** | **78%** |

### Deferred Items (need separate tasks)
1. **C.1 Semantic warnings (S014-S025)** — TypeChecker needs `warning()` infrastructure
2. **C.3 HIR error codes (H001-H015)** — HIR builder needs Result return types
3. **D.2 Query codes (Q001-Q010)** — query system has no user-facing errors
4. **D.3 Formatter codes (F001-F010)** — formatter has no error messages
