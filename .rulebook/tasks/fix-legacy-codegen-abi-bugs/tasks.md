## 0. Bug 0 — MIR println/print Polymorphic Dispatch (DISCOVERED & FIXED)
- [x] 0.1 Root cause: MIR codegen called `@println(i32 42)` instead of `@print_i32(i32 42)` — wrong function for non-Str types
- [x] 0.2 Fix: Added type-specific print dispatch in `instructions.cpp` emit_call_inst
- [x] 0.3 Fix: Added print_i32/print_i64/print_f64/print_bool declarations in mir_codegen.cpp
- [x] 0.4 Fix: Added dso_local to all runtime declarations to prevent LLVM 23+ merging
- [x] 0.5 Verified: println(42), println(I64), struct return + params all work

## 1. Bug 1 — Nested Generic Struct Copy
- [ ] 1.1 Create minimal reproduction: `List[Outer].get()` where Outer has `List[Inner]` field
- [ ] 1.2 Emit IR and trace the copy semantics — identify where shallow copy occurs
- [ ] 1.3 Fix: ensure struct return from `List.get()` deep-copies inner heap pointers
- [ ] 1.4 Verify reproduction case passes (no segfault)
- [ ] 1.5 Run affected test suites (core/list, std/collections)

## 2. Bug 2 — Template Literal Missing in MIR Codegen
- [ ] 2.1 Confirmed: MIR codegen for template literals only emits first interpolation as bare print
- [ ] 2.2 Trace MIR builder to find where template literal parts are lost
- [ ] 2.3 Fix: emit full string concatenation for template literals
- [ ] 2.4 Verify reproduction case compiles and runs
- [ ] 2.5 Run core/fmt and core/str test suites

## 3. Integration Verification
- [ ] 3.1 Run `scripts/audit_docs.tml` end-to-end (all bugs fixed)
- [ ] 3.2 Run full test suite — no regressions
- [ ] 3.3 Update CHANGELOG.md
