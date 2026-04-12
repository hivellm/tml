# Tasks: Fix Codegen Blockers — K001 + Build Parser + tml cv

**Status**: Planned (0/18)
**Blocks**: ALL compiler-tml runtime tests
**Priority**: CRITICAL — no compiler-tml test can run at runtime until K001 is fixed
**Risk**: Medium — codegen changes require careful IR verification

---

## Phase 1: K001 — Enum Discriminant Type Mismatch (7 items)

- [ ] 1.1 Write minimal reproducer: `.tml` file with 16-variant enum + `Heap[T]` field that triggers `insertvalue i64 vs i32` error
- [ ] 1.2 Read `emit_enum_def()` in `compiler/src/codegen/mir/type_emitter.cpp` — trace how the LLVM struct type `{ i32, [N x i8] }` is generated for the `Type` enum
- [ ] 1.3 Read `instructions.cpp:358` — trace the `insertvalue ... undef, i32 <disc>, 0` emission and identify why it hardcodes `i32`
- [ ] 1.4 Fix: extract actual discriminant type from the LLVM struct definition instead of hardcoding `i32` — use `struct_type->getElementType(0)` or equivalent
- [ ] 1.5 Fix sub-issue: if `emit_enum_def` generates `{ i64, ... }` for 16+ variant enums, fix it to always use `i32` discriminant (matching Rust's enum layout)
- [ ] 1.6 Verify: `tml.exe build .sandbox/test_list_type.tml` compiles without K001 error
- [ ] 1.7 Verify: `tml.exe test --suite=compiler-tml` — unify_primitives and unify_basic tests pass at RUNTIME (not just type-check)

## Phase 2: K001b — Runtime ACCESS_VIOLATION Crash (3 items)

- [ ] 2.1 After K001 fix, re-run `unify_primitives.test.tml` and `unify_basic.test.tml` — check if crash persists
- [ ] 2.2 If crash persists: use `--emit-ir` to inspect the generated LLVM IR for `infer_ctx_new()` — check struct layout of `InferCtx { List[Type], HashMap[Str,I64], List[TypeError], I64 }`
- [ ] 2.3 If struct layout is wrong: fix `emit_struct_def()` for structs containing `List[EnumType]` fields — verify field offsets match ABI expectations

## Phase 3: Build Parser — Generic Static Method Syntax (3 items)

- [ ] 3.1 Reproduce: `tml build .sandbox/test_list_type.tml` fails with parse error at `List[Heap[Type]]::new(2)` line
- [ ] 3.2 Fix C++ parser: in `parse_primary_expr()` or `parse_postfix_expr()`, when seeing `Ident [ GenericArgs ] ::`, parse as generic-type + static-method call rather than index expression
- [ ] 3.3 Verify: `tml build` accepts `List[Heap[Type]]::new(2)` syntax without parse error

## Phase 4: `tml cv` — Replace Subprocess with In-Process Check (2 items)

- [ ] 4.1 In `cmd_coverage.cpp`, replace `_popen("tml.exe check ...")` with direct call to the query system (`QueryEngine` or `run_check()` from the same process)
- [ ] 4.2 Verify: `tml cv compiler-tml` shows correct type-check pass count (expected: 84/84 or close)

## Phase 5: Validation (3 items)

- [ ] 5.1 Run `tml test --suite=compiler-tml --no-fail-fast` — all test files compile AND run without crashes
- [ ] 5.2 Run `tml cv compiler-tml` — full report with accurate type-check and coverage data
- [ ] 5.3 Run full compiler test suite (`tml test --suite=compiler`) — verify no regressions in existing 477 passing tests

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
