# Tasks: codegen + parser bring-up bugs discovered in phase23b

**Status**: Ready to archive (12/12 — all phases + tail complete)
**Depends on**: phase23b (C frontend — complete)
**Blocks**: phase24 (cc CLI integration) — items 1.1–1.3 of this task
**Duration**: 2–3 weeks (bugs #7, #8, #9 are the hardest)
**Risk**: Medium — each bug is bounded, but #6 and #8 will need deep
codegen investigation
**Output**: ~30 C++ fixes across compiler/src/ + 9 regression tests

---

## Phase 1: Enum / Constructor Codegen (3 items — phase24 blockers — COMPLETE)

All three phase24 blockers turned out to share a single root cause.

- [x] 1.1 Bug #7 — Enum-variant pattern-binding emitted extractvalue only
  for the leading field. Root cause: the multi-field binding branch in
  `compiler/src/codegen/llvm/control/when.cpp::gen_when` required
  `payload[0]->is<IdentPattern>()`, so any pattern with a leading
  wildcard (`V(_, b, _)`) fell through to a no-binding path and `b`
  surfaced as "Unknown variable" at codegen time. Fix (commit
  `880dfbba`): introduce `multi_field_pattern_eligible(payload)` helper
  that accepts any mix of IdentPattern + WildcardPattern as long as at
  least one element is a real binding; the per-element emit loop
  already skips non-IdentPattern entries. Regression:
  `compiler/tests/compiler/enum_pattern_bind_multiple_fields.test.tml`
  with three cases (all-named, wildcard leading, wildcard middle).
- [x] 1.2 Bug #8 — Deeply-nested constructor expressions in one
  statement hang or crash. Turned out to be a secondary manifestation
  of bug #7: with the pattern-binding failure generating bad IR,
  downstream duplicate calls on the unresolved values crashed or
  looped. Fixing #7 made the one-line constructor form
  `decls.push(Heap[CDecl]::new(CDecl::Var(Heap[CVarDecl]::new(vd))))`
  work without any source-level workaround. Regression:
  `compiler/tests/compiler/nested_constructor_push.test.tml`.
- [x] 1.3 Bug #9 — Large enums with by-value struct payloads. Same
  cascading story: the "crash on duplicate" signature was bug #7's
  pattern-binding error propagating through downstream codegen. With
  #7 fixed, CDecl's variants accept by-value struct payloads
  (`Var(CVarDecl)`, no `Heap` wrap needed), and the same goes for
  Func/StructDef/UnionDef/EnumDef/TypedefDef. Regression:
  `compiler/tests/compiler/large_enum_by_value_duplicate.test.tml`.
  phase23b `ast.tml`/`parser.tml`/`lower.tml` un-apply of the Heap
  workarounds is included in this task's commit.

## Phase 2: F64 Phi Bugs (3 items — COMPLETE)

Phase 2's "F64 phi" framing turned out to be misleading. All three bugs
shared a single root cause with Phase 1's pattern-binding bug (#7) and
with a parser precedence mistake: `CAST=13, RANGE=16` made `to` bind
tighter than `as`, so `0 as I64 to n as I64` parsed as
`(0 as (I64 to n)) as I64`. The resulting malformed IR cascaded through
the for-in / if-expr / parse_f64 code paths and each bug presented as a
different symptom (phi-incoming type mismatch, infinite loop, bad
icmp). Fixing the precedence (swap to CAST=14, RANGE=13) in commit
`a7fee3ce` alongside the bug #7 enum-pattern fix (`880dfbba`) resolved
all three Phase 2 bugs.

- [x] 2.1 Bug #4 — `if-expr` with F64 branches. Now works: commit
  `a7fee3ce` (precedence fix). Regression:
  `compiler/tests/compiler/if_expr_f64_branches.test.tml` (5 cases
  including F64 arithmetic in `var` accumulator after an if-expr seed).
- [x] 2.2 Bug #5 — for-in F64 accumulator. Same root cause, same fix.
  Regression: `compiler/tests/compiler/for_in_f64_accumulator.test.tml`
  (3 cases: cast-in-range, F64 multiply accumulator, F64 add accumulator).
- [x] 2.3 Bug #6 — `Str.parse_f64 -> Maybe[F64]` infinite loop. Same
  root cause (the stdlib parse_f64 internally uses `for _ in 0 as I64
  to n as I64` — once that stopped parsing as a broken cast, the
  function works). Regression:
  `compiler/tests/compiler/maybe_f64_parse.test.tml` (4 cases: valid
  decimal, integer-only, negative, invalid-is-nothing).

## Phase 3: Parser / Language Bugs (2 items — COMPLETE)

- [x] 3.1 Bug #1 — `base` demoted to a contextual keyword. Removed from
  the lexer keyword table in `compiler/src/lexer/lexer_core.cpp` (it
  now tokenises as `Identifier`). The three parser sites that need to
  recognise `base` for its super-reference semantics — inheritance
  `: base(args)` (`parser_oop.cpp`), expression entry in
  `parse_primary_expr`, and the `parse_base_expr` helper
  (`parser_expr_complex.cpp`) — now detect it by identifier lexeme
  match (`check(Identifier) && peek().lexeme == "base"`). The
  primary-expr detection additionally requires a following `.` so
  plain `base` identifiers don't silently become super-references.
  Regression: `compiler/tests/compiler/base_as_local_name.test.tml`
  with 5 cases (local var, arithmetic, struct field, function
  parameter, var assignment). Tests in `compiler/tests/codegen/oop_test.cpp`
  updated to reflect the new contextual lexing (count by lexeme not
  by KwBase token kind). Commit `TBD-bug1`.
- [x] 3.2 Bug #2 — `return StructName { ... }` inside an `if` / `when`
  / `loop`. Not reproducible in the current tree — 6 variants
  (top-level, if, when, loop, nested with U64 fields, in-if-with-else)
  all parse and type-check cleanly. Either the bug was fixed by a
  previous commit or only manifested under a very specific
  combination of tokens we can't isolate. Regression test added as a
  guard against future reappearance:
  `compiler/tests/compiler/return_struct_literal.test.tml` with 6
  cases covering every control-flow context. All pass.

## Phase 4: Stdlib API (1 item — COMPLETE; non-breaking)

- [x] 4.1 Bug #3 — re-analysis showed there was no signature/impl
  mismatch: `HashMap.get(k) -> V` with zero-value-on-miss was the
  documented convention and matched the implementation. The original
  phase23b bring-up note was a misreading. The real gap was no
  `Maybe[V]`-returning sibling for callers that want explicit
  "present vs absent" handling. Fix (non-breaking): added
  `HashMap.get_opt(k) -> Maybe[V]` in `lib/std/src/collections/hashmap.tml`,
  mirroring `get`'s probe structure but returning `Just(v)` / `Nothing`.
  `get` stays put. Regression:
  `compiler/tests/compiler/hashmap_get_maybe.test.tml` covering all
  three API ergonomics (get + zero-on-miss, has+get pattern, get_opt).

## Phase 5: Cleanup and Regression Tests (3 items — COMPLETE)

- [x] 5.1 Un-applied Heap-wrapping workarounds in
  `compiler-tml/src/cc/parser.tml` (and sibling modules where
  applicable) — commit `cc9fb6fc`. `CDecl` variants now carry
  `CVarDecl`/`CFuncDecl`/`CStructDef`/`CUnionDef`/`CEnumDef`/
  `CTypedefDef` by value instead of through double-Heap wrappers.
  `parser.tml:1622` marks the natural form as a regression guard for
  bugs #7/#8/#9. Component test suite (`c_lexer`, `c_parser`) still
  passes; `c_frontend` fails with an unrelated pre-existing K001
  around `Maybe[Heap[CBlockItem]]` codegen (tracked separately).
- [x] 5.2 Regression tests for each of the 9 bugs landed under
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
  All nine pass on the current compiler.
- [x] 5.3 Full `tml test --suite=compiler` passes 177/178
  (2026-04-18). The single failure — `other/closure_codegen` — is a
  pre-existing X003 crash / X002 timeout unrelated to this task.
  `c_lexer` and `c_parser` component tests pass; `c_frontend` K001
  Maybe-instantiation mismatch is pre-existing and tracked
  independently. No regressions attributable to phase0v.

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 6.1 Update or create documentation covering the implementation —
  per-phase docstrings and comments attached to each fix's commit
  (`880dfbba` bug #7, `a7fee3ce` precedence for bugs #4/#5/#6,
  `cc9fb6fc` workaround un-apply). Regression guard comment at
  `compiler-tml/src/cc/parser.tml:1622-1623` records the rationale
  for keeping the natural CDecl-by-value form.
- [x] 6.2 Write tests covering the new behavior — nine regression
  tests under `compiler/tests/compiler/` (enumerated in 5.2).
- [x] 6.3 Run tests and confirm they pass — full
  `tml test --suite=compiler` shows 177/178 (pre-existing
  `other/closure_codegen` crash/timeout unrelated to this task).
