# phase26e 1.2 — Borrow-lifetime binding design (from 2026-07-19 research)

Goal: `let r = c.get_ref(i); c.push(x); use(*r)` = compile error (B009-class),
zero false positives on the existing corpus.

## Current state (verified)

- `let r = s.get_ref()` creates NO borrow link: `check_method_call`
  (borrow/checker_expr.cpp:357) checks receiver+args and returns — the
  two-phase machinery is a dormant stub (`begin/end_two_phase_borrow` set/reset
  a flag; `reserve/activate_two_phase_borrow` in checker_ops.cpp:879-904 are
  fully written but NEVER called).
- The checker has NO method resolution at call sites (comment at
  checker_expr.cpp:369-395). Free functions DO resolve via
  `type_env_->lookup_func` (checker_expr.cpp:319-322).
- Elision Rule 3 (output lifetime = self) is documented (checker_core.cpp:52-70)
  and drives only the in-callee E031 diagnostic; nothing propagates to callers.
- `lowlevel { }` blocks are INVISIBLE: `check_expr` dispatch
  (checker_expr.cpp:43-90) has no LowlevelExpr arm — falls to a no-op.
- Linking fields already exist: `PlaceState.borrowed_from` (checker.hpp:596),
  `BorrowChecker::ref_to_borrowed_` (checker.hpp:1301),
  `create_borrow_with_projection` (checker_nll.cpp:418). NLL extends a borrow
  to the ref's last use automatically.

## Minimal design

1. **Method-sig access (the real cost):** in `check_method_call`, resolve the
   receiver's static type via `type_env_` and look up the method by name —
   narrow interim (option b from research), NOT full side-table threading.
   Needed bits: (i) return type is `ref`/`mut ref`, (ii) receiver param is
   `this` vs `mut this`.
2. **Ref-returning method call as receiver borrow:** when the resolved return
   is `ref T`/`mut ref T` and the receiver is a place (ident/field chain):
   `create_borrow_with_projection(receiver, Shared, loc, lhs_place)` +
   `ref_to_borrowed_[lhs] = receiver` + `borrowed_from`. NLL handles the rest.
   Only fire when resolution is CONFIDENT (unresolvable method ⇒ do nothing —
   under-borrow is the safe direction; over-borrow = false positives).
3. **Mutating method call requires unique access:** when the resolved receiver
   param is `mut this`, require/create a short-lived unique borrow of the
   receiver for the call (wire the dormant reserve/activate two-phase pair).
   Existing conflict machinery (`check_can_borrow`) then emits B009 when a
   live interior ref exists. Same confidence guard.
4. **Lowlevel recursion NOT required** for the call-site error (the callee body
   stays trusted); optional follow-up only.

## Blast-radius protocol (mandatory)

The failure mode is FALSE POSITIVES (legit code newly rejected). After wiring:
run full compiler+core+std typecheck/test sweep; every new B-error in existing
code is a bug in the wiring (fix or narrow the trigger), NOT a migration item.
Ship only at zero new errors on the corpus.

## Tests

- gtest `compiler/tests/frontend/borrow_test.cpp` `check_error` cases —
  NOTE: tml_tests does not configure under Zig CC locally (pre-existing
  googletest issue) — so ALSO add executable negative fixtures: .tml files in
  compiler/tests/borrow/ compiled via `tml check`, asserted non-zero exit via a
  shell script (model: compiler/tests/cli/*.sh).
- Positive cases (must stay legal): get_ref → read → push AFTER last use of
  ref (NLL); two get_refs coexisting; get_ref in a block that ends before the
  mutation.
