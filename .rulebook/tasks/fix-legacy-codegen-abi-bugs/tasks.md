## 1. Bug 1 — Nested Generic Struct Copy
- [ ] 1.1 Create minimal reproduction: `List[Outer].get()` where Outer has `List[Inner]` field
- [ ] 1.2 Emit IR and trace the copy semantics — identify where shallow copy occurs
- [ ] 1.3 Fix: ensure struct return from `List.get()` deep-copies inner heap pointers
- [ ] 1.4 Verify reproduction case passes (no segfault)
- [ ] 1.5 Run affected test suites (core/list, std/collections)

## 2. Bug 2 — Struct Return + Str Param ABI
- [ ] 2.1 Create minimal reproduction: `func f(s: Str) -> MyStruct` vs `func f() -> MyStruct`
- [ ] 2.2 Compare LLVM IR of both — identify calling convention difference
- [ ] 2.3 Compare with Rust reference IR for equivalent pattern
- [ ] 2.4 Fix: correct sret / register return convention when params present
- [ ] 2.5 Verify reproduction case passes
- [ ] 2.6 Run full test suite for regressions

## 3. Bug 3 — Template Literal Type Confusion in Loops
- [ ] 3.1 Create minimal reproduction: `println(\`{i}: {s}\`)` in loop with I64 + Str
- [ ] 3.2 Emit IR and trace which SSA value gets wrong type
- [ ] 3.3 Fix: ensure each template interpolation gets independent SSA temporaries
- [ ] 3.4 Verify reproduction case compiles and runs
- [ ] 3.5 Run core/fmt and core/str test suites

## 4. Integration Verification
- [ ] 4.1 Run `scripts/audit_docs.tml` end-to-end (all three bugs fixed)
- [ ] 4.2 Run full test suite — no regressions
- [ ] 4.3 Update CHANGELOG.md
