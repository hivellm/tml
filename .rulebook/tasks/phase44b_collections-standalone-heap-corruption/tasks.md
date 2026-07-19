# phase44b — std/collections Standalone-Mode Heap Corruption / SEH Crash

> Discovered 2026-07-19 during the phase44a fallout survey (item 1.3):
> `.rulebook/tasks/phase44a_test-dispatcher-false-pass/tasks.md`. Full evidence
> and repro log paths in `docs/analysis/phase44a-fallout/01-revealed-failures.md`.
> Not the same bug as `phase27c_module-path-flaky-corruption` (that is a
> compiler-internal typecheck resolver bug; this is a runtime heap corruption
> inside the compiled test binary).

## 1. Implementation
- [ ] 1.1 Reproduce `lib/std/tests/collections/deque_extras.test.tml`,
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
- [ ] 1.2b **Test the unifying hypothesis FIRST — the scope may be wider than
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
- [ ] 1.3 For `deque_extras`/`deque_extras2`: trace the corrupted allocation
  back to its owner using the debugger findings from 1.1. Likely candidates
  to rule in/out first: `Deque::destroy()` double-free or use-after-free
  interacting with the per-test entry-wrapper's stack-allocated locals
  (`lib/std/src/collections/deque.tml`), and any difference in how the
  "cached stdlib codegen state" (seen in test logs: "All suites will use
  cached stdlib codegen state") is captured for a single-file compile vs. a
  multi-file suite compile — this composition-sensitivity is the same shape
  as the original phase44a bug evidence and should be checked as a common
  cause across all 3 files.
- [ ] 1.4 Fix the confirmed root cause(s). If two independent bugs are found
  (one per hypothesis in the proposal), fix both; do not stop after the first.
- [ ] 1.5 Verify: all 3 files pass standalone, 20/20 clean reruns each
  (foreground, no-cache, no concurrent load) — nondeterministic bugs need
  more than one green run to trust. Re-run full `std/collections` standalone
  (103/103) and suite-packed mode to confirm no regression either way.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation (note in
  ADR-004 if the entry-wrapper thunk contract changes)
- [ ] 2.2 Write tests covering the new behavior — a regression fixture that
  pins the fixed behavior (whatever the root cause turns out to be, e.g. a
  minimal repro isolating the specific codegen pattern that corrupted the
  heap)
- [ ] 2.3 Run tests and confirm they pass
