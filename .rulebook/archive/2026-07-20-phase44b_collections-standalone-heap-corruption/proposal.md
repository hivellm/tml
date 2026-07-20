# Proposal: phase44b_collections-standalone-heap-corruption

## Why

The phase44a fallout survey (item 1.3) ran `core/alloc`, `std/collections`,
and `core/str` standalone-per-file (`--no-suite`, the mode where the
false-pass bug lived) against the rebuilt compiler. `core/alloc` (45/45) and
`core/str` (33/33) are fully clean. `std/collections` shows 3 of 103 files
failing **only in standalone mode** — never in suite-packed mode, where this
suite was previously verified green (96/96, 96/96, 102/102 per
`scripts/known-failures.txt`):

- `lib/std/tests/collections/deque_extras.test.tml`
- `lib/std/tests/collections/deque_extras2.test.tml`
- `lib/std/tests/collections/iter_ref.test.tml`

Repeated isolated (foreground, no concurrent load) reruns show **nondeterministic**
failure modes for the same file: `deque_extras2.test.tml` alone produced an
`X002` timeout twice and a `HEAP_CORRUPTION (0xC0000374)` crash once across
three runs. `deque_extras.test.tml` produced `HEAP_CORRUPTION (0xC0000374)`.
`iter_ref.test.tml` produced an `X003` crash with exit code `-1073740791`
(`0xC0000409` / `STATUS_STACK_BUFFER_OVERRUN`) — the **exact same crash class**
documented in `.rulebook/tasks/phase44a_test-dispatcher-false-pass/tasks.md`
item 1.1 (a panic's `longjmp` unwinding across an SEH scope), which item 1.2
claimed to have fixed by removing the vestigial `__try/__except` in
`tml_run_test_seh` (`compiler/runtime/core/essential.c`).

This nondeterminism (hang vs. crash across identical reruns of the same test)
is the classic signature of corrupted heap/allocator metadata, not a flaky
test or a resource-contention artifact — full detail and evidence in
`docs/analysis/phase44a-fallout/01-revealed-failures.md`. It is NOT the same
bug as `phase27c_module-path-flaky-corruption` (that one is a compiler-internal
use-after-free in the module-path resolver during typecheck; this one is a
runtime heap corruption inside the *compiled test binary* itself, during
`Deque`/`List`/`HashMap` iterator and collection-method execution).

Two competing root-cause hypotheses need to be resolved, both requiring more
than a survey pass (debugger/Application-Verifier attach):
1. The phase44a entry-wrapper fix (item 1.2's new per-test `i32()` thunk in
   `generate_entry.cpp` / `mir_codegen.cpp`) was verified against a minimal
   repro (`@test` returning a literal `7`) but may not hold for a realistic
   stack frame with drop glue, closures, and generic borrowing-iterator locals
   — i.e. the SEH-crossing crash it claimed to fix might still reproduce for
   more complex test bodies.
2. An independent, pre-existing memory-safety bug in the `Deque`/borrowing
   -iterator codegen or the runtime allocator, exposed only when a test file
   is compiled alone (standalone/1-file-per-EXE) rather than packed into a
   suite — matching the "composition-sensitivity" signature from the original
   phase44a bug report.

## What Changes

- Root-cause the heap corruption / SEH-crossing crash using a debugger or
  Application Verifier attach on the standalone `deque_extras`, `deque_extras2`,
  and `iter_ref` test binaries (foreground, isolated, no-cache, no concurrent
  test runs, to avoid confounding).
- Determine whether `iter_ref.test.tml`'s crash happens during a real panic
  unwind (assertion failure — a genuine refcount bug in `iter_ref()`/
  `value_ref()`) or with no panic at all (a codegen bug in the borrowing
  -iterator machinery, `lib/std/src/collections/behaviors.tml:341-491`).
- Fix the root cause (memory-safety bug in codegen, runtime allocator, or the
  entry-wrapper thunk) — not by re-ordering, retrying, or suppressing the
  symptom.
- Add these 3 files to `scripts/known-failures.txt` ONLY if a genuine
  external blocker remains after the fix attempt (do not use this task to
  silently catalogue-and-ignore).

## Impact

- Affected specs: none directly; touches ADR-004 (NDJSON subprocess test
  protocol) if the entry-wrapper thunk needs a second fix.
- Affected code: `compiler/runtime/core/essential.c` (watchdog / SEH),
  `compiler/src/codegen/llvm/core/generate_entry.cpp`,
  `compiler/src/codegen/mir_codegen.cpp` (entry-wrapper thunk),
  `lib/std/src/collections/deque.tml`, `lib/std/src/collections/behaviors.tml`
  (borrowing iterators) — exact locus TBD by root-cause work.
- Breaking change: NO.
- User benefit: eliminates a real memory-corruption bug in
  `std::collections` and closes a gap in the phase44a false-pass fix if
  hypothesis 1 is confirmed.
