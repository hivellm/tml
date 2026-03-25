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

## 2. Bug 2 — Template Literal MIR Codegen (DEFERRED — feature gap)
- [x] 2.1 Confirmed: MIR path has NO template literal support at all
- [x] 2.2 Legacy codegen handles via Text::new() + push_str() + Display::to_string() dispatch
- [ ] 2.3 Fix requires: desugaring TemplateLiteralExpr in HIR builder OR porting to MIR codegen
- [ ] 2.4 This is a feature gap, not a bug fix — requires ~200 lines of new code
- [ ] 2.5 Workaround: test suite uses legacy codegen (works), standalone programs need --legacy flag

## 3. Integration Verification
- [x] 3.1 core/str 22/22, core/num 50/50, std/collections 70/76
- [x] 3.2 test_generic_v7.tml passes (was the original crash case)
- [ ] 3.3 audit_docs.tml blocked by template literal support in MIR
