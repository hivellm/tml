## 0. Bug 0 — MIR println/print Polymorphic Dispatch (FIXED — 90137ba3)
- [x] 0.1 Root cause: MIR codegen called `@println(i32 42)` instead of `@print_i32(i32 42)`
- [x] 0.2 Fix: Added type-specific print dispatch in MIR instructions.cpp emit_call_inst
- [x] 0.3 Fix: Added print_i32/print_i64/print_f64/print_bool declarations in mir_codegen.cpp
- [x] 0.4 Fix: Added dso_local to all runtime declarations to prevent LLVM 23+ merging
- [x] 0.5 Verified: println(42), struct return + params all work

## 1. Bug 1 — Missing Static Method Call Arguments (FIXED — 99876997)
- [x] 1.1 Root cause: `List[T]::new()` call emitted with 0 args but definition has 1 param (initial_capacity)
- [x] 1.2 On Win64, callee read rcx (previous function's arg) instead of its parameter → segfault
- [x] 1.3 Fix: Fill default values (0/null) for missing args in method_static_dispatch.cpp
- [x] 1.4 Verified: List[Violation]::new(), scan() with Str param, test_generic_v7.tml all work
- [x] 1.5 Regression: std/collections 70/76 (6 pre-existing failures, unchanged)

## 2. Bug 2 — Template Literal Support (FIXED — 1a16ea9e + 8745ebeb + 6c91fbca)
- [x] 2.1 HIR desugaring: TemplateLiteralExpr → str_concat_opt + val.to_string() chain (1a16ea9e)
- [x] 2.2 Legacy codegen: Text type println fix — extract data_ptr from handle (8745ebeb)
- [x] 2.3 MIR native: inline to_string via snprintf, removed legacy fallback (6c91fbca)
- [x] 2.4 Verified: `{i}: {s}` in loops, all primitive types (I64, F64, Bool, Str, mixed)
- [x] 2.5 `tml run`, `tml build`, `tml test` all use MIR path — no legacy fallback needed

## 3. Bug 3 — Nested Struct Shallow Copy / Heap Corruption (FIXED — 2bdbcf22 + e1e9701f)
- [x] 3.1 Root cause 1: `reports.push(report)` shallow-copied inner List handle, then `report` was dropped → dangling ptr
- [x] 3.2 Fix: Mark struct args as consumed when passed to methods (2bdbcf22)
- [x] 3.3 Root cause 2: `let r = reports.get(ri)` returned copy, field drops freed shared List handle
- [x] 3.4 Fix: Suppress field-level drops for let bindings from method/function calls (e1e9701f)
- [x] 3.5 Verified: audit_docs.tml runs end-to-end (901 files, 103KB report, exit 0)

## 4. Missing Enum Type Definitions (FIXED — 477e0287)
- [x] 4.1 Root cause: RefCell__I32 referenced %struct.BorrowState but enum type was never emitted
- [x] 4.2 Fix: Auto-emit enum types from module registry when referenced as struct field dependency
- [x] 4.3 Verified: core/cell RefCell tests no longer fail with "Cannot allocate unsized type"

## 5. Non-Lazy Stdlib for Tests (FIXED — 6913d3a1)
- [x] 5.1 Root cause: pre-compiled stdlib used lazy_library_defs=true, missing functions only used by specific tests
- [x] 5.2 Fix: Set lazy_library_defs=false to emit ALL public library functions
- [x] 5.3 Fix: Added recovery loop for pending library methods/funcs in verify phase
- [x] 5.4 Expected: ~80+ test SKIPs resolved (AllocError, RefCell, FormatSpec, etc.)

## 6. Meta Cache Cleanup (DONE)
- [x] 6.1 Deleted 22 stale .tml.meta files with incompatible version 7
- [x] 6.2 Verified: zero "Incompatible module meta version" warnings

## Integration Verification
- [x] core/str 22/22, core/num 50/50, std/collections 70/76
- [x] test_generic_v7.tml passes (original crash case)
- [x] audit_docs.tml runs end-to-end (901 files scanned, 103KB output)
- [x] Template literals work natively via MIR (all types, loops, standalone programs)
- [x] No meta cache warnings
- [ ] Full test suite with non-lazy stdlib (pending rebuild)
