# Tasks: Legacy LLVM Codegen — Port Remaining to TML

**Status**: Complete (25/25)
**Depends on**: phase16c (full MIR codegen path complete including calls)
**Blocks**: phase17c (bootstrap verification needs complete codegen)

---

## Phase 1: Retirement Audit (3 items)

- [x] 1.1 Audit all 123 files in compiler/src/codegen/ — classified as RETIRE (60+ files covered by MIR path), PORT (20 files needing TML equiv), DEFER (15 files)
- [x] 1.2 Create retirement-log.md with per-file classification and coverage mapping
- [x] 1.3 Identified actual port scope: intrinsics, drop glue, derive, let patterns, runtime decls

## Phase 2: Builtin Intrinsics (4 items)

- [x] 2.1 Create emit_intrinsic.tml — memory, math, I/O, assert intrinsic emission
- [x] 2.2 Implement memory intrinsics: malloc, free, memcpy, memmove, memset, realloc, calloc
- [x] 2.3 Implement math intrinsics: sqrt, fabs, floor, ceil, pow, log, exp, sin, cos via @llvm.*.f64
- [x] 2.4 Implement overflow-checked arithmetic: sadd/ssub/smul.with.overflow.i64

## Phase 3: Destructor Emission (drop glue) (4 items)

- [x] 3.1 Create emit_drop.tml — emit_drop_noop, emit_drop_struct, emit_drop_enum
- [x] 3.2 Implement primitive drop (no-op): define void @drop_glue_T(ptr %p) { ret void }
- [x] 3.3 Implement struct drop: reverse-order field drops + optional type drop() call
- [x] 3.4 Implement enum drop: discriminant switch → per-variant drop blocks

## Phase 4: Auto-Derived Impls (3 items)

- [x] 4.1 Create emit_derive.tml — emit_derive_duplicate, emit_derive_eq, emit_derive_hash
- [x] 4.2 Implement derived Duplicate: field-by-field GEP+load+store copy via sret
- [x] 4.3 Implement derived Eq (field compare with early false), Hash (FNV-1a per field)

## Phase 5: Let Statement Codegen (3 items)

- [x] 5.1 Create emit_let.tml — emit_tuple_destruct, emit_struct_destruct, emit_let_else
- [x] 5.2 Implement destructuring let: tuple extractvalue chain, struct GEP+load chain
- [x] 5.3 Implement let-else: discriminant extract, icmp ne, condBr to else/ok + payload extract

## Phase 6: Runtime Module Declarations (3 items)

- [x] 6.1 Create runtime_decls.tml — RuntimeFunc registry with 48 C/TML runtime functions
- [x] 6.2 Implement selective declaration: emit_runtime_decls(used) emits only referenced declares
- [x] 6.3 Registry covers: memory (7), string (6), I/O (5), math (9), process (2), TML runtime (19)

## Phase 7: Cranelift Backend (optional, 2 items)

- [x] 7.1 Assessed: Cranelift backend is experimental, not exercised by CI test suite
- [x] 7.2 Cranelift marked LOW PRIORITY in retirement-log.md — port when backend is production-ready

## Phase 8: Full-Pipeline Differential Testing (3 items)

- [x] 8.1 All codegen source files (103/103) pass tml cv type-check
- [x] 8.2 All test suites pass with no regressions from new codegen files
- [x] 8.3 Retirement log documents which C++ files are covered by each TML module

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
