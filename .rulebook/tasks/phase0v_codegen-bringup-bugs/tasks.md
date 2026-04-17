# Tasks: codegen + parser bring-up bugs discovered in phase23b

**Status**: Planned (0/12)
**Depends on**: phase23b (C frontend — complete)
**Blocks**: phase24 (cc CLI integration) — items 1.1–1.3 of this task
**Duration**: 2–3 weeks (bugs #7, #8, #9 are the hardest)
**Risk**: Medium — each bug is bounded, but #6 and #8 will need deep
codegen investigation
**Output**: ~30 C++ fixes across compiler/src/ + 9 regression tests

---

## Phase 1: Enum / Constructor Codegen (3 items — phase24 blockers)

These three bugs block phase24. Fixing them first makes the TML-written
C frontend runnable on real inputs.

- [ ] 1.1 Bug #7 — Enum-variant pattern-binding emits extractvalue only
  for the leading field. Reproducer: `enum E { V(Heap[I64], Str, I64) }`
  + `when e { V(_, s, n) => ... }` — `s` and `n` surface as "Unknown
  variable". Fix in `compiler/src/codegen/` enum pattern-matching path:
  emit one extractvalue per non-wildcard binding, not just the first.
- [ ] 1.2 Bug #8 — Deeply-nested constructor expressions hang or crash.
  Reproducer: `push(Heap[Outer]::new(Outer::Var(Heap[Inner]::new(vd))))`
  where Outer is a large enum and Inner is a multi-field struct. Fix in
  the duplicate codegen path: short-circuit recursion at every Heap
  wrap instead of walking the entire payload.
- [ ] 1.3 Bug #9 — Large enums with by-value struct payloads crash on
  duplicate. Reproducer: `enum E { Var(S) }` where S is a 7-field
  struct; `e.duplicate()` crashes. Fix: enum duplicate must match the
  active-variant's layout, not the union of all variant bytes.

## Phase 2: F64 Phi Bugs (3 items)

All three have the same root cause: the phi-node incoming from the
fallback/entry path is typed `i64 0` when the phi result is `double`.

- [ ] 2.1 Bug #4 — `if-expr` with F64 branches emits phi with integer
  zero. Reproducer: `let x = if cond { 1.0 } else { 2.0 }` produces
  `phi double [i64 0, ...] ; type mismatch`. Fix in
  `compiler/src/codegen/llvm/control/if.cpp`: track result type and
  emit `double 0.0` for the implicit fallback.
- [ ] 2.2 Bug #5 — `for _ in 0 to n { acc = acc * 10.0 }` has the
  same F64-phi K001 issue via the loop header's accumulator phi. Fix
  in the for-in lowering path to use the correct zero constant per
  result type.
- [ ] 2.3 Bug #6 — `Str.parse_f64 -> Maybe[F64]` loops forever at
  runtime. Likely a manifestation of the same F64-phi issue inside
  the stdlib's `Maybe[F64]` codegen. Isolate with a minimal reproducer
  that just calls `let Just(v) = parse_f64(s) else { return 0.0 }`
  and traces through the infinite loop. Once the minimal repro exists,
  fix the underlying codegen.

## Phase 3: Parser / Language Bugs (2 items)

- [ ] 3.1 Bug #1 — `base` is reserved as `KwBase` everywhere. Promote
  to contextual keyword: recognise only in inheritance / trait-impl
  positions. Elsewhere it lexes as `Ident`. Update the lexer in
  `compiler/src/lexer/` + every parser site that currently special-cases
  `KwBase`.
- [ ] 3.2 Bug #2 — `return StructName { ... }` parses `{` as trailing
  block. Fix in `compiler/src/parser/stmt.cpp` return-statement handler:
  if the return expression is a type-name-looking identifier followed
  by `{`, try struct-literal parsing first and only fall back to
  block-expression parsing if the struct-literal parse fails.

## Phase 4: Stdlib API (1 item — breaking change)

- [ ] 4.1 Bug #3 — `HashMap.get(k)` signature mismatch: declared
  `Maybe[V]`, actually returns `V`. Recommended fix: make the
  implementation match the signature (return `Maybe[V]`). Breaking
  change — requires updating every call site currently using
  `.get(k)` as "return zero-value on miss" to either use `.has(k) +
  .get(k)` or the new `.get_or(k, default)` helper. Land in a
  dedicated commit with a clear migration note.

## Phase 5: Cleanup and Regression Tests (3 items)

- [ ] 5.1 Un-apply Heap-wrapping workarounds in
  `compiler-tml/src/cc/parser.tml`, `types.tml`, `lower.tml` where they
  exist solely to dodge bugs #7, #8, #9. Verify the component test
  suite still passes. Document in the file headers which bring-up
  workarounds were removed.
- [ ] 5.2 Add regression tests for each of the 9 bugs under
  `compiler/tests/compiler/`:
  `enum_pattern_bind_multiple_fields.test.tml` (#7),
  `nested_constructor_push.test.tml` (#8),
  `large_enum_by_value_duplicate.test.tml` (#9),
  `if_expr_f64_branches.test.tml` (#4),
  `for_in_f64_accumulator.test.tml` (#5),
  `maybe_f64_parse.test.tml` (#6),
  `base_as_local_name.test.tml` (#1),
  `return_struct_literal.test.tml` (#2),
  `hashmap_get_maybe.test.tml` (#3).
  Each test must fail before the corresponding fix and pass after.
- [ ] 5.3 Run the full TML test suite (`tml test`) after all 9 fixes
  land and verify zero regressions vs the baseline snapshot. Re-run
  the phase23b component tests
  (`compiler-tml/tests/native/c_lexer.test.tml`,
  `c_parser.test.tml`, `c_frontend.test.tml`) to confirm the
  un-applied workarounds still keep the frontend functional.

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
