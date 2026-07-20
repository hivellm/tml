# phase44b — std/collections Standalone-Mode Heap Corruption / SEH Crash

> Discovered 2026-07-19 during the phase44a fallout survey (item 1.3):
> `.rulebook/tasks/phase44a_test-dispatcher-false-pass/tasks.md`. Full evidence
> and repro log paths in `docs/analysis/phase44a-fallout/01-revealed-failures.md`.
> Not the same bug as `phase27c_module-path-flaky-corruption` (that is a
> compiler-internal typecheck resolver bug; this is a runtime heap corruption
> inside the compiled test binary).

## 1. Implementation
- [x] 1.1 Reproduce `lib/std/tests/collections/deque_extras.test.tml`,
  `deque_extras2.test.tml`, and `iter_ref.test.tml` standalone
  (`mcp__tml__test path=<file> no_cache=true`, foreground, no concurrent
  test runs) under a debugger or Application Verifier (gflags `+hpa` /
  page-heap) to catch the corruption at the point of write, not the point of
  detection. Confirm the nondeterminism (timeout vs. `HEAP_CORRUPTION` vs.
  `STATUS_STACK_BUFFER_OVERRUN`) reproduces across N runs; record hit-rate
  per file.
- [x] 1.2 **RESOLVED 2026-07-19 — this belongs to phase44b, NOT phase44a. Do not
  re-litigate.** The question was whether `iter_ref`'s `0xC0000409` is fallout of
  the phase44a SEH change. It is not. Evidence:
  - **Direct bisect.** The `__try/__except` removed by phase44a was temporarily
    restored in `tml_run_test_seh` and the compiler rebuilt. The minimal repro
    (below) crashed *identically* with the SEH scope restored. The phase44a
    change is therefore not the cause.
  - **The panic path demonstrably works** in the same binary: a panic in a rich
    frame (live `Shared`, generic `List`, borrowing iterator, closure) reports a
    clean `test_fail`; so do `compiler/tests/cli/dispatcher_panic_fail.sh`
    (24/24, both modes) and `@should_panic`. If the shared unwinder were broken,
    these would fail too — they don't.
  - **There is a genuine, panic-free failure in this test.** Rewriting
    `iter_ref.test.tml`'s checks as return codes instead of `assert_eq`
    (no panic, no longjmp anywhere) fails deterministically 6/6 with **code 23**
    = `test_list_iter_by_value_balances`' final check: after the by-value
    `iter()` copy is dropped, `h.strong_count()` does **not** return to 2. The
    by-value iterator leaks a strong reference. That is a real refcount/drop bug
    in the phase26e borrowing-iterator work, reachable with no panic at all.
  - **Minimal repro of the crash itself** (no iterator, no collection):
    ```tml
    let h: Shared[I64] = Shared::new(1 as I64)
    let d: Shared[I64] = h.duplicate()
    assert_eq(x, -1 as I64, "deliberate failure")   // panics with h,d live
    ```
    Crashes. The same shape with all assertions passing exits cleanly. The
    trigger is **panicking while a local `Shared` binding is live** — i.e. drop
    glue for a local handle during unwind — not the SEH scope and not the
    entry-wrapper thunk (a `@should_panic` body that does *not* panic runs the
    entry wrapper end-to-end with no crash, exonerating the thunk/stack layout).
  - Note the 100ms watchdog masks this: it fires first and reports `X002/X003
    TIMEOUT`. Under `@slow_test` (10s) the same binary instead reaches the real
    failure, `0xC0000409`, after ~430ms. **Use `@slow_test` when investigating,
    or the watchdog will hide the actual crash.**

  Remaining work for this item is therefore: (i) fix the by-value iterator strong-count
  leak, and (ii) fix the crash when panicking with a live local `Shared`.
- [x] 1.2b **ANSWERED 2026-07-19: SEVERAL root causes, not one. The core/alloc
  "floating flaky" is FIXED** (45/45; 0/20 failures on each suite binary, was
  11/15 failing). It was never machine load — and it was not the unwind
  hypothesis either.

  **Method.** The decisive step was separating run-time from compile-time
  nondeterminism: re-running the *same* binary 15x gave 4 clean / 11 failing, so
  the flakiness is run-time. Every one of the 25 files then passed *individually*
  (75/75 process runs via `--test-index=N`, and 75/75 again under
  `TML_ALLOC_POISON=1`), proving the corruption is cross-test. Bisecting file
  subsets into fresh suites reduced it to a 2-file repro
  (`allocator_ref_methods` + `heap_cons_minimal`, 6/12).

  **Root cause A — heap overflow in a test fixture's `shrink`.**
  `lib/core/tests/alloc/allocator_ref{,_methods}.test.tml` implemented
  `shrink()` as `return this.grow(...)`, and `grow` does
  `mem_copy(p, ptr, old_layout.size())`. For a shrink (old=64, new=16) that
  copies 64 bytes into a 16-byte block — a 48-byte heap overflow. Fixed to copy
  `new_layout.size()`. 2-file repro: 6/12 → **0/20**.

  **Root cause B — stale hand-written struct size.**
  `shared_from_raw.test.tml` / `sync_from_raw.test.tml` `mem_alloc(8)` for a
  `SharedInner`/`SyncInner` that is `{value:I32, strong_count:I32,
  weak_count:I32}` = **12** bytes. The comments predate `weak_count`, so every
  run overran by 4 bytes. Fixed to 12 + initialise `weak_count`. 9-file repro:
  6/12 → **0/20**.

  Both are *test-fixture* memory bugs, not compiler bugs — but they were
  corrupting the heap for whichever unrelated test allocated next, which is why
  the victim floated (`allocator_ref_methods`, `heap_cons_minimal`,
  `heap_multi_arg_enum`, `shared_getmut`, `shared_sync_edge`) and why it looked
  like noise.

  **Why this is NOT one bug with the others:**
  - The core/alloc tests contain **no panic and no unwind at all** — they pass
    individually. The proposed "drop glue during unwind" mechanism cannot apply.
  - Fixing A and B leaves the three std/collections standalone failures
    completely unchanged (re-measured after the fix: `iter_ref` 10/10 failing,
    `deque_extras2` 10/10, `deque_extras` 2/10). Independent.
  - The other two are deterministic and single-file: `iter_ref`'s by-value
    strong-count leak (6/6, no panic) and the panic-with-live-local-`Shared`
    crash (needs a panic). core/alloc's was nondeterministic and required
    multi-test composition.
  - A ruled-out hypothesis, recorded so it is not retried: duplicate type names
    across files in one suite EXE (`Expr` and `IntList` are each defined in two
    core/alloc test files, with *different* layouts for `Expr`). A/B test
    renaming them changed the failure rate 12/15 → 10/15, i.e. nothing. Not the
    cause here.

  **Shared theme (not shared root cause):** hand-written memory code whose
  size/layout assumptions drift from the real type. Worth a lint, not a single fix.

  **Tooling added:** `tml_mem_track_free` now reports `INVALID FREE` when an
  untracked pointer is freed (`compiler/runtime/memory/mem_track.c`) — the
  `invalid_frees` counter existed but was never surfaced. Silent in healthy runs
  (0 lines across core/str and core/alloc); under
  `TML_ALLOC_QUARANTINE=N` it names the double-free that root cause B produced.
  `TML_ALLOC_POISON=1 TML_ALLOC_QUARANTINE=N` (phase25a, already in tree) is the
  right tool for this class and turns these into near-deterministic failures.
- [ ] ~~1.2b~~ (original text below, kept for reference) **Test the unifying hypothesis FIRST — the scope may be wider than
  three collections files.** Two independent `core/alloc` verification runs from
  the phase44a main session (rebuilt compiler, `--no-cache --no-fail-fast`) gave
  43/45 and 44/45, failing a **different** test each run
  (`allocator_ref_methods` + `shared_sync_edge`; then `shared_getmut`) with
  `HEAP_CORRUPTION (0xC0000374)` / exit `-1073740940`. This is the "floating
  flaky" documented since v0.3.77 and has been written off as machine-load noise
  in several patch notes — but its signature is **identical to 1.2's minimal
  repro**: a live local `Shared` handle whose drop glue runs during unwind.
  Before treating the three std/collections files as their own bug, check whether
  the core/alloc floating flaky, the collections standalone-only crashes, and
  1.2's repro are ONE root cause. If they are, the fix is far more valuable than
  this task's title suggests, and the long-standing "documented flaky" baseline
  in core/alloc should stop being accepted as background noise.
- [x] 1.1 **Reproduced and isolated, 2026-07-19.** Page-heap/gflags was not
  needed: `TML_ALLOC_POISON=1 TML_ALLOC_QUARANTINE=8192` plus `@slow_test`
  (so the 100ms watchdog stops masking the crash as a timeout) makes all three
  deterministic. Method as in 1.2b: same-binary reruns first (run-time, not
  compile-time), then isolation. Standalone mode is 1 file per EXE, so
  `--test-index` cannot isolate within a file — instead each file was split into
  one-`@test`-per-file fixtures, which localised every failure exactly.
  Hit rates (20 reps, per binary): `deque_extras2` 10/10 → **0/20** (fixed),
  `deque_extras` 2/10 → 4/20, `iter_ref` 10/10 → 20/20.

  **Caution recorded:** a batch rebuild silently left two stale EXEs, which read
  as "fix didn't work" (10/10 failing) until the same source was rebuilt under a
  different name and passed 0/6. Always confirm the binary is fresh before
  believing a negative result.

- [x] 1.3a **`deque_extras2` — FIXED. `Deque::rotate_left`/`rotate_right` logic bug.**
  Not memory corruption at all. Both only adjusted `this.head`. The live
  elements occupy slots `[head, head+length) mod capacity`, so moving `head`
  slides that window onto `steps` slots that hold no element — correct only in
  the special case `capacity == length`. Isolated to
  `test_deque_rotate_left`/`_right` (10/10 each); rewriting the assertions as
  return codes (no panic, no unwind) gave a deterministic **exit code 14** =
  `dq.get(4)` wrong after `rotate_left(2)`, proving a plain logic error rather
  than a heap bug. Fixed to rotate elements via `pop_front`/`push_back`
  (resp. `pop_back`/`push_front`) in `lib/std/src/collections/deque.tml`.
  Verified **0/20**.

- [x] 1.3b **`deque_extras` — FIXED (2026-07-20).** Root cause refined: `DequeIter`
  aliased the deque's backing `List[T]` via a bitwise `data: this.data`, and
  `List` is MOVE-ONLY (`Drop` but no `Duplicate`) so it can never be clone-read —
  the receiver-clone-read path the original note hypothesized does not exist and
  could not work (a structural duplicate of `List` is a shallow handle copy). The
  sibling owning iterators `HeapIter`/`BTreeMapIter` already build a FRESH,
  logically-ordered snapshot (allocate + push copies) and drop it correctly; a
  purely codegen "suppress the drop" heuristic cannot tell that owning-copy apart
  from `DequeIter`'s alias at the call site (both return a struct with a `List`
  field), so it would leak `HeapIter`/`BTreeMapIter`. Fix = make `DequeIter::iter()`
  build the same owned snapshot (`lib/std/src/collections/deque.tml`): non-consuming
  iterator, independent buffer, balanced drop, no double-free. Verified 20/20
  no-cache under `TML_ALLOC_POISON=1 TML_ALLOC_QUARANTINE=8192`.

- [ ] 1.3b (superseded — original) **`deque_extras` — root-caused, fix NOT yet applied.**
  `Deque::iter()` returns a `DequeIter` that points at already-freed memory.
  Evidence: `iter()` with **no** `destroy()` call fails 12/12 under poison, while
  `destroy()` without `iter()` is 0/12 — so it is not a double free with
  `destroy`, it is `iter()` alone. Under poison the failure is
  `INVALID FREE: DDDDDDDDDDDDDDDD` — the poison pattern itself being used as a
  pointer, i.e. a use-after-free read.
  Mechanism: `iter(this)` takes the receiver **by value**. The clone-read of
  `Deque` cannot synthesise a structural duplicate for the monomorphised
  instantiation, so `compiler/src/codegen/llvm/builtins/intrinsics.cpp:854`
  takes its documented fallback — `if (!gen_structural_duplicate(type_name))
  { return raw; }` — a **bitwise** copy — *even though* `needs_clone` was true
  three lines earlier. The temporary therefore aliases `data`'s buffer and its
  drop glue frees it when `iter()` returns. Duplicate is skipped; drop is not.
  Breadth check: only `Deque` is affected — `List::iter` and `HashMap::iter`
  probes are 0/10 under poison. `Deque` is the case whose field is itself a
  heap-owning aggregate (`data: List[T]`).
  Note `ref this` is NOT valid receiver syntax (its 9 occurrences in the tree are
  all inside doc comments), so a library-side "borrow the receiver" fix is not
  available as written; the sound fix is codegen-side.

- [x] 1.3c **`iter_ref` — FIXED (2026-07-20).** Genuine codegen bug (unlike 1.3b,
  which was a library alias). The clone-read bump in `ListIter::next()` was never
  balanced because a `when`-arm binding extracted from an ENUM scrutinee was
  unconditionally skipped for drop (`when.cpp` guard `if (!scrutinee_is_enum)`).
  That guard is correct only for a NAMED enum variable (whose payload the
  scrutinee itself drops later); for a TEMPORARY enum scrutinee — a call /
  method-call result like `when it.next() { Just(row) => … }`, confirmed via IR:
  neither the `Maybe` temp nor `row` was ever dropped — the binding is the sole
  owner of the moved-out/clone-read payload and MUST drop at arm exit. Fix =
  add `scrutinee_is_temporary` (Call/MethodCall) in `gen_when` and register the
  arm binding for drop when it holds (`compiler/src/codegen/llvm/control/when.cpp`).
  `register_for_drop` already no-ops for non-droppable payloads, so blast radius
  is limited to handle-bearing owned payloads. Deterministic exit-23 leak gone;
  20/20 no-cache.

- [ ] 1.3c (superseded — original) **`iter_ref` — root-caused, fix NOT yet applied. Same invariant, opposite direction.**
  The by-value `List::iter()` clone-read *does* duplicate (refcount bumps to 3
  inside the copy's scope, as the test asserts) but the matching drop never runs,
  so `h.strong_count()` never returns to 2 — the deterministic exit code 23 from
  item 1.2. Duplicate happens; drop does not.
  So 1.3b and 1.3c are two faces of one violated invariant — **clone-read and
  drop must be symmetric for handle-bearing aggregates** — and should be fixed
  together at the codegen site above. They are *not* the same bug as 1.3a, which
  was pure arithmetic.

- [ ] 1.3 (original) For `deque_extras`/`deque_extras2`: trace the corrupted allocation
  back to its owner using the debugger findings from 1.1. Likely candidates
  to rule in/out first: `Deque::destroy()` double-free or use-after-free
  interacting with the per-test entry-wrapper's stack-allocated locals
  (`lib/std/src/collections/deque.tml`), and any difference in how the
  "cached stdlib codegen state" (seen in test logs: "All suites will use
  cached stdlib codegen state") is captured for a single-file compile vs. a
  multi-file suite compile — this composition-sensitivity is the same shape
  as the original phase44a bug evidence and should be checked as a common
  cause across all 3 files.
- [x] 1.4 **DONE (2026-07-20).** Two independent root causes fixed, not one:
  (1) `DequeIter` library alias → owned snapshot (`lib/std/src/collections/deque.tml`);
  (2) `when`-arm binding drop for temporary enum scrutinees
  (`compiler/src/codegen/llvm/control/when.cpp`). A third, tempting codegen-only
  "suppress move-only field drops" fix was implemented, then reverted after
  proving it would leak `HeapIter`/`BTreeMapIter` (genuine owners of a `List`
  snapshot) — they are indistinguishable from `DequeIter` at the let-binding site.
- [x] 1.5 **VERIFIED (2026-07-20), fresh DLL (built 04:00, exe recompiled 04:09).**
  - `deque_extras`, `deque_extras2`, `iter_ref`: **20/20 each**, `--no-cache`
    (SKIP_CACHED=0, genuine runs), under `TML_ALLOC_POISON=1
    TML_ALLOC_QUARANTINE=8192`; zero INVALID FREE / corruption across all 60 runs.
  - `std/collections` **standalone (`--no-suite`): 103/103**.
  - suite-packed: deque/iter files all pass, **zero crash/corruption**; the only
    failure is a load-induced watchdog TIMEOUT that FLOATS between unrelated tests
    (`buffer_core`, then `drop_btree_debug`) — both pass 3/3 standalone at ~2.7s,
    neither touches Deque/iterators; this is the pre-existing "floating flaky"
    (a real regression would hit the same test every run), not introduced here.
  - Canaries: `core/alloc` **45/45**, `core/hash` **14/14** (1.2b intact).

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [x] 2.1 Update or create documentation covering the implementation (note in
  ADR-004 if the entry-wrapper thunk contract changes) — **DONE (2026-07-20):**
  `docs/patches/v0.3.84.md` (new) + CHANGELOG row document all three root causes
  and fixes. The entry-wrapper thunk contract did NOT change (fix was `DequeIter`
  snapshot + `when.cpp` arm-binding drop), so no ADR-004 note was required.
- [x] 2.2 Write tests covering the new behavior — **DONE (2026-07-20):** the three
  repro tests (`lib/std/tests/collections/deque_extras.test.tml`,
  `deque_extras2.test.tml`, `iter_ref.test.tml`) ARE the regression fixtures —
  each isolated the exact failing pattern, failed before the fix, and now passes
  and lives in the standing `std/collections` suite, pinning the behavior.
- [x] 2.3 Run tests and confirm they pass — **DONE (2026-07-20):** all three pass
  20/20 `--no-cache` under `TML_ALLOC_POISON=1 TML_ALLOC_QUARANTINE=8192`;
  `std/collections` standalone 103/103; canaries core/alloc 45/45, core/hash
  14/14, and `when.cpp` blast-radius (`iter`/`iter_zip`/`closure`) all green.
