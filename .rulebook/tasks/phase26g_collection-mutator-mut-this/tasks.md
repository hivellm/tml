# phase26g — Collection Mutators: unique-access requirement (unlocks get_ref-then-push detection)

> Prerequisite context: phase26e Cluster D landed the interior-ref borrow
> wiring (zero false positives); it cannot see push-class invalidation while
> mutators are `this`. Blast radius of a naive `mut this` flip measured at
> 20+ std tests ("not declared as mutable"). USER DECISION REQUIRED before
> implementation (see proposal): `mut this` migration vs `@invalidates_refs`
> attribute route.

## 1. Implementation
- [x] 1.1 Decision presented + recorded (2026-07-19): USER CHOSE **`mut this` (Rust-style mutability)** over the `@invalidates_refs` attribute. Rulebook decision #13. Breaking change accepted; call sites migrate `let`→`var`.
- [x] 1.2 Mutators flipped to `mut this` (2026-07-19): List (20 mutators) + HashMap (8: set/remove/clear/get_or_set/retain/drain_keys/drain_values/extend_from) + Buffer (8: write_*/clear/reset_read/set/fill/fill_all). **BTreeMap, Deque, and class_collections (HashSet/Queue/Stack/Vec/LinkedList) mutators were ALREADY `mut this`** — the codebase was largely Rust-style already. Reads (get/get_ref/len/contains/iter/...) stay `this`.
- [x] 1.3 Call-site migration — **NEAR-ZERO, the breaking-change premise does not hold in this compiler.** MEASURED: calling a `mut this` mutator on an OWNED `let` binding (`let l = List::new(4); l.push(1)`) compiles cleanly — the receiver-mutability model does NOT require `var` for owned values. Sweep produced ZERO "not declared as mutable" across std/collections, std/json, core/hash. So no `let`→`var` cascade exists; the "20+ tests" prior estimate does not reproduce on this HEAD.
- [x] 1.4 Invalidation error — **DELIVERED for List + Deque (Cluster D) AND HashMap + BTreeMap (phase26g closeout).** `let r = c.get_ref(i); c.mutate(); use(*r)` now rejects (B009) for List/Deque (`get_ref -> ref T`). **HashMap/BTreeMap `get_ref -> Maybe[ref V]` gap CLOSED:** the interior-ref borrow now propagates through pattern matching. `check_method_call` detects a ref carried inside a generic wrapper (`ref_kind_in_type` recurses type-args — `Maybe[ref V]`/`Maybe[mut ref V]`; plain `Maybe[V]` carries none, so no borrow is manufactured). `check_when` captures the receiver borrow from a direct method-call scrutinee and, on a `Just(r)` arm, binds `r` (via new `bind_interior_ref_payload`) tying the borrow to it; `check_stmt` now visits `LetElseStmt` (previously UNvisited — init/else/pattern were entirely unchecked) via new `check_let_else` doing the same for `let Just(r) = ... else`. So `when m.get_ref(k){Just(r)=> m.set(...); use(*r)}` and the let-else form both reject with B009. Positives stay green: read-only-in-arm + mutate-after, released-reborrow, plain `Maybe[V]` (get_opt) with in-arm mutation. Under-borrow safe (confident direct-method-call shapes only).
- [x] 1.5 Blast-radius sweep (final, 2026-07-19 closeout): std/collections clean (94 pass; 1 pre-existing X002 flaky), std/json 23, core/hash 14, core/alloc 44 (floating flaky), std/http samples + let-else-heavy stdlib self-builds = ZERO new borrow errors. borrow_interior_ref.sh **6/6**. Determinism ×10: 28/28 on re-run (first-run single dip `tml_condmove_when` 8/10 recovered ×20 = 20/20 under the poison allocator — catalogued adversarial flaky). std/text `text_search_transform` K001 proven PRE-EXISTING phase27a Class 2 (see closeout notes) and ADDED to scripts/known-failures.txt with full provenance; the three stale std/collections K001 lines REMOVED from known-failures (fixed by phase27a Class 2 at d39333dd, verified green 3× at three HEADs). **Zero NEW errors attributable to phase26g.**

### Root cause of the 1.4 caveat + the fix (compiler, small confident)
The `mut this` flip alone did NOT enable detection. `compiler/src/borrow/checker_expr.cpp` computed `recv_is_mut_this` (the receiver takes `mut this`) but discarded it (`(void)recv_is_mut_this;`) — conflict checking ran ONLY for methods returning a ref (`get_ref`/`get_mut`). Fix part 1 (checker_expr.cpp): added an `else if (recv_is_mut_this)` branch — a `mut this` mutator that returns no ref takes a transient unique borrow of the receiver that conflicts with any LIVE interior-ref borrow this wiring created. Fix part 2 (checker_stmt.cpp): the interior-ref borrow was bound to the LHS whenever `pending_ref_return_` was set, even when the ref was consumed by a wrapping expr (`let x: I64 = *c.get_ref(i)`), leaving a phantom borrow live → false B009 on a later mutation. Gated the bind on the initializer being DIRECTLY a `MethodCallExpr`. Both small, in the existing phase26e wiring; compiler rebuilt.

### phase26g closeout (2026-07-19)
- Item 1 (Maybe[ref V] propagation) DONE — see 1.4 above. Fixtures:
  compiler/tests/cli/borrow_interior_ref.sh now **6/6** (added
  neg_get_ref_then_insert_map.tml [when] + neg_get_ref_then_insert_btree.tml
  [let-else]). Positives: lib/std/tests/collections/map_interior_ref.test.tml
  (when read-only + mutate-after, let-else read-then-mutate, plain get_opt).
  Blast-radius sweep ZERO new borrow errors: std/collections (94 pass, 1
  pre-existing X002 flaky), core/alloc (44, floating flaky), std/json (23),
  core/hash (14), std/http samples, + let-else-heavy stdlib self-builds
  (hashmap/btreemap/list/json/http app+h2 = 0). Determinism ×10: first run
  27/28 (tml_condmove_when 8/10 adversarial flaky; single-target ×20 under the
  poison allocator = 20/20 recovery), re-run **28/28**. Files:
  compiler/src/borrow/checker_expr.cpp (ref_kind_in_type, check_when capture +
  bind_interior_ref_payload), checker_stmt.cpp (check_let_else + LetElseStmt
  dispatch), include/borrow/checker.hpp (2 method decls).
- Item 2 (std/text K001) — VERDICT: **PRE-EXISTING phase27a Class 2 (ptr-vs-i64),
  NOT a regression from Cluster G.** Evidence: `--suite=std/text` fails at HEAD
  with `[K001] ir:1646:13: '%t755' defined with type 'i64' but expected 'ptr'`
  in text_search_transform.test.tml (replace_all → std::text buffer-math:
  repeat/replace/text_copy_bytes ptr/i64 casts). Swapping in the pre-84e3507e
  text.tml (concat/as_str_ref reverted) reproduces the SAME K001 → not a
  text-source regression. Between 84e3507e..ced02379 there are only 2 docs
  commits (NO compiler/codegen change), so the compiler codegen is identical to
  84e3507e → the K001 was present then; the Cluster G "std/text 0 fail" was a
  cached/composition-limited run (the AST `--emit-ir` path emits VALID IR;
  only the test-compile codegen_unit path trips it — path/composition
  sensitivity classic to phase27a). Phase26g adds no codegen. Catalogued, NOT
  fixed (out-of-scope phase27a debt per closeout instructions).

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [x] 2.1 Update or create documentation covering the implementation (2026-07-19): `docs/patches/v0.3.82.md` + CHANGELOG 0.3.82 row (incl. the honest note that binding-level `var` enforcement for `mut this` receivers is NOT yet type-checker-enforced — future decision); borrow_interior_ref.sh header comments document the covered/uncovered shapes; known-failures.txt updated with full provenance notes.
- [x] 2.2 Write tests covering the new behavior (borrow_interior_ref.sh 6/6 +
      map_interior_ref.test.tml positives)
- [x] 2.3 Run tests and confirm they pass
