# phase44a Fallout Survey — Standalone-Mode Failures (item 1.3)

Scope: `core/alloc`, `std/collections`, `core/str` run standalone-per-file
(`--no-suite`, the mode where the false-pass lived) against the rebuilt
compiler with the phase44a dispatcher fix
(generate_entry.cpp / mir_codegen.cpp / essential.c / testing_dispatcher_gen.cpp).
Goal: find anything that was silently passing behind the discarded-catch-result
bug, or otherwise hidden by suite-mode packing.

Raw logs: `.sandbox/phase44a_alloc_standalone.log`,
`.sandbox/phase44a_collections_standalone.log` (fail-fast, stopped at 48/103),
`.sandbox/phase44a_collections_standalone_full.log` (`--no-fail-fast`, full 103/103),
`.sandbox/phase44a_str_standalone.log`, plus isolated single-file reruns via
`mcp__tml__test` (foreground, no concurrent load) for each of the three
failing files.

## Summary

| Suite | Files | Standalone result | Compared to `scripts/known-failures.txt` |
|---|---|---|---|
| `core/alloc` | 45 | **45/45 pass** | clean, nothing new |
| `core/str` | 33 | **33/33 pass** | clean, nothing new |
| `std/collections` | 103 | **100/103 pass**, 3 fail | 3 revealed failures, NOT in known-failures.txt |

Zero false-passes were found for the specific bug this phase fixed (a panicking
`@test` silently reported as `test_pass`) — `core/alloc` and `core/str` are
fully clean standalone, and none of the 3 `std/collections` failures look like
a swallowed panic; they are a different, more serious failure class (below).

## Revealed failures (std/collections, standalone mode only)

| Test file | Symptom (isolated, foreground reruns) | Root-cause classification |
|---|---|---|
| `lib/std/tests/collections/deque_extras.test.tml` | 1 run: `CRASH: HEAP_CORRUPTION (0xC0000374)`, exit -2 | Memory-safety bug, standalone-mode-only (composition-sensitive) |
| `lib/std/tests/collections/deque_extras2.test.tml` | 3 runs: `X002 TIMEOUT (10000ms)` ×2, `HEAP_CORRUPTION (0xC0000374)` ×1 — **nondeterministic across identical reruns** | Same class as above; nondeterminism between hang and crash is a classic heap-corruption signature (corrupted allocator metadata deadlocks the heap lock in some runs, trips the debug heap's corruption check in others) |
| `lib/std/tests/collections/iter_ref.test.tml` | 1 run: `X003 CRASH`, exit code `-1073740791` = `0xC0000409` (`STATUS_STACK_BUFFER_OVERRUN`) | Same crash code the phase44a fix (item 1.1/1.2) was supposed to eliminate — see "Important cross-reference" below |

None of these three files appear in `scripts/known-failures.txt`, and none were
part of the previously-verified-green `std/collections` consolidated suite
runs referenced there (96/96, 96/96, 102/102) — those were **suite-packed**
runs (many files per EXE); this is the first systematic **standalone**
(1 file per EXE) run of this suite. All three fail only standalone; they were
never observed failing in suite-packed mode, matching the exact
"composition-sensitivity" signature called out in the original phase44a bug
report (evidence: a standalone run behaved differently than the same body
packed into a suite).

**Not test bugs.** `deque_extras.test.tml` and `deque_extras2.test.tml` exercise
`Deque::insert/remove/retain/drain/rotate_left/rotate_right/binary_search` with
straightforward, correct, monotonically-bounded loops (reviewed
`lib/std/src/collections/deque.tml:283-446` line by line — no unbounded loop,
no off-by-one that would explain a hang). `iter_ref.test.tml` exercises the
phase26e "borrowing iterators" feature (`List.iter_ref()`, `HashMapIter.value_ref()`)
added in commit `84e3507e`; its assertions are the documented contract
(iter_ref must not bump refcounts). The failures are not explained by anything
in the `.tml` source — they point at the standalone-mode codegen/runtime path.

## Important cross-reference: is the phase44a fix incomplete for this path?

`iter_ref.test.tml`'s crash code (`0xC0000409` / `STATUS_STACK_BUFFER_OVERRUN`)
is **exactly** the crash class documented in
`.rulebook/tasks/phase44a_test-dispatcher-false-pass/tasks.md` item 1.1, third
bullet: a panic's `longjmp` unwinding across the (now-removed) `__try/__except`
scope in `tml_run_test_seh`. Item 1.2 states this was fixed by dropping that
SEH scope in `compiler/runtime/core/essential.c`.

Two explanations were considered:

1. **A genuine assertion failure inside `iter_ref.test.tml`** (i.e. `iter_ref()`
   or `value_ref()` really does bump a refcount it shouldn't, tripping
   `assert_eq(h.strong_count(), ...)`) combined with **the SEH-crossing fix not
   being fully effective** for this specific stack shape (closures over
   `Shared[I64]`, generic borrowing-iterator locals) — i.e. item 1.2's fix
   was verified against a minimal repro (`@test` returning a literal `7`) but
   not against a realistic stack frame with drop glue and generics.
2. An unrelated, pre-existing memory-safety bug in the borrowing-iterator
   codegen itself (a real stack buffer overrun in `ListRefIter`/`HashMapIter`
   generated code), independent of the entry-wrapper fix.

Both point at the same follow-up: reproduce `iter_ref.test.tml` standalone with
`--nocapture` and a debugger/Application-Verifier attach to capture the actual
panic message (if any) before the crash, then determine whether the crash
happens *during* a panic unwind (→ explanation 1, phase44a fix needs a second
pass) or with no panic at all (→ explanation 2, a codegen bug in the
borrowing-iterator feature). This was not resolved in this survey — attempts
to capture raw subprocess stdout via `--nocapture` did not surface additional
detail in the coordinator log (output likely goes to the test's own captured
stream, not the compile-time log), and reproducing under a debugger is out of
scope for this survey pass.

## Nondeterminism note (methodology)

`deque_extras2.test.tml` was rerun 3 times in isolation (foreground,
`mcp__tml__test`, no concurrent load) and produced two different failure
modes (timeout twice, heap corruption once). This rules out an environmental
confound (e.g. CPU contention from the parallel background suite runs used
earlier in this survey inflating a legitimate-but-slow test past the 10s
watchdog) — a resource-starvation artifact would not also produce a
Windows heap-corruption crash. The nondeterminism itself is the evidence for a
real memory-safety defect (corrupted allocator/heap metadata manifests
differently depending on heap layout/timing), not a flaky test.

## What was NOT found

- No case where a test file printed a panic message and was still reported
  `test_pass` (the specific defect item 1.1/1.2 fixed). `core/alloc` (the
  suite where the original evidence — `shared_get_sound.test.tml` — lived)
  is fully green standalone, 45/45.
- No new failures in `core/str` (33/33).
- No compile errors (K001-class) in any of the three surveyed suites beyond
  what's already catalogued in `scripts/known-failures.txt` (none of the K001
  entries there touch alloc/collections/str).
