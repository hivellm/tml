# phase44a — Standalone Test Dispatcher False-Pass on Panic

> Evidence (2026-07-18, phase26c session): standalone run of
> `lib/core/tests/alloc/shared_get_sound.test.tml` printed
> `panic: assertion failed at :39` and then `{"event":"test_pass"}`; the
> suite-packed run of the same body correctly reported the crash. The
> false-pass masked a real codegen soundness bug and mimicked
> composition-sensitivity.

## 1. Implementation
- [x] 1.1 Root-cause the standalone dispatcher path — **done 2026-07-19**. The dispatcher
  itself was innocent: it correctly branches on `%rc = call i32 @tml_test_<id>()`. The
  defect is in the generated test-entry wrapper, `generate_entry.cpp` (AST path). It
  called `%test_catch_N = call i32 @tml_run_test_with_catch(ptr @test_fn)` and then
  **discarded the result**, ending with an unconditional `ret i32 0`. The runtime catches
  the panic (longjmp back to `tml_run_test_with_catch`), prints `panic: …` to stderr and
  returns `-1` — which was thrown away, so the dispatcher saw rc==0 and emitted
  `test_pass`. Exactly reproduces the 2026-07-18 evidence (panic line, then `test_pass`).
  Deterministic repro used: a `@test` returning `7` was reported as `test_pass`, exit 0.
  Three further defects found on the same path while tracing:
  - `mir_codegen.cpp::emit_test_entry_wrapper` called test bodies **directly** — no
    panic/crash catching at all, plus the same unconditional `ret i32 0`.
  - `tml_run_test_seh` wrapped the call in `__try/__except`; the panic `longjmp` had to
    unwind across that SEH scope, which fastfailed with `STATUS_STACK_BUFFER_OVERRUN`
    (0xC0000409) instead of reporting a failure. (The identical setjmp/longjmp in
    `tml_run_should_panic`, which has no SEH scope, always worked.)
  - `tml_run_with_timeout` never signalled the watchdog "done" event when the body
    longjmp'd out, so the watchdog later fired `TerminateProcess(…, 99)` on an
    already-handled failure.
- [x] 1.2 Fix — **done 2026-07-19**. A panicking body now produces a `test_fail` event
  carrying the real panic message plus a non-zero exit, identically in standalone
  (`--no-suite`) and suite-packed modes. Changes:
  - `compiler/src/codegen/llvm/core/generate_entry.cpp`: record the first non-zero
    `tml_run_test_with_catch` result in `%fail_code`, fail fast, and `ret` it. Added a
    per-test `i32()` thunk so unit-returning bodies no longer yield a garbage i32.
  - `compiler/src/codegen/mir_codegen.cpp`: route MIR test bodies through
    `tml_run_test_with_catch` via the same thunk scheme and propagate the result.
  - `compiler/runtime/core/essential.c/.h`: dropped the vestigial `__try/__except`
    (VEH already handles crashes), added `tml_watchdog_cancel()` on every non-local exit,
    added `tml_test_error_json()` for the event's `error` field.
  - `compiler/src/testing/testing_dispatcher_gen.cpp`: `test_fail` now emits the real
    panic message via `tml_test_error_json()`; test `name`/`file`/suite strings are
    JSON-escaped (Windows paths previously produced unparseable NDJSON).
  Verified: panicking fixture → `test_fail` + `"error":"assertion failed at :5: …"` +
  exit 1, in both modes; passing fixture → `test_pass` + exit 0. Regression: core/alloc
  45/45 (the original evidence suite), core/convert 24/24 (covers `@should_panic`),
  core/str and core/array green. core/iter still shows the pre-existing K001 i64/i32
  range-width compile errors — unrelated to this path.
  No regression fixture committed yet — that is item 2.2.
- [x] 1.2b `@should_panic` crash — **found and fixed 2026-07-19** while validating
  the gate `compiler/tests/cli/dispatcher_panic_fail.sh`. A `@should_panic` body
  that actually panicked crashed with `0xC0000409` instead of passing.
  **Not a regression from 1.2** (proven: an `@should_panic` body that does *not*
  panic ran the entry wrapper end-to-end cleanly, exonerating the codegen change;
  and `tml_run_should_panic` calls none of the runtime functions 1.2 modified).
  It was pre-existing and had **zero coverage** — the repo contained no
  `@should_panic` test at all, and `lib/core/tests/convert/convert.test.tml:371`
  carries a standing note "These need `@should_panic` support".
  Root cause: `tml_run_should_panic` carried its own small setjmp frame; the
  panic `longjmp` back into it fastfailed, while the identical panic unwinding
  into `tml_run_test_with_catch`'s frame worked in the same binary.
  Fix (`compiler/runtime/core/essential.c`): both now share one
  `tml_run_test_catch_impl(fn, quiet)`; `tml_run_should_panic` delegates to it
  with `quiet=1` so an *expected* panic prints no `FATAL [runtime] panic:` line —
  which also stops the coordinator's stderr scan from misreporting it as an
  X003 crash. A crash (-2) still does **not** satisfy `@should_panic`.
  **Correction to my earlier report:** I claimed "core/convert 24/24 covers
  `@should_panic`". That was wrong — the grep matched a comment, not a decorator.
  `@should_panic` had no coverage until this gate; it does now.
- [x] 1.3 Fallout survey — **done 2026-07-19**. Ran `core/alloc` (45/45),
  `core/str` (33/33), and `std/collections` (100/103, then full 103/103 with
  `--no-fail-fast`) standalone-per-file (`--no-suite --no-cache`) against the
  rebuilt compiler. `core/alloc` (the suite where the original false-pass
  evidence — `shared_get_sound.test.tml` — lived) and `core/str` are fully
  clean: zero false-passes, zero swallowed panics. `std/collections` revealed
  3 files that fail ONLY standalone (never in suite-packed mode, where this
  suite was previously verified green): `deque_extras.test.tml`,
  `deque_extras2.test.tml`, `iter_ref.test.tml` — but these are NOT the
  panic-swallow defect this phase fixed. Isolated foreground reruns show
  nondeterministic `HEAP_CORRUPTION (0xC0000374)` / `X002` timeout /
  `X003 STATUS_STACK_BUFFER_OVERRUN (0xC0000409)` across identical reruns of
  the same file — a genuine memory-corruption bug, composition-sensitive
  (standalone-only), not caught by this survey's root-cause budget. Notably,
  `iter_ref.test.tml`'s crash code is identical to the SEH-crossing crash
  documented in item 1.1/1.2, raising the possibility that fix is incomplete
  for stack frames with drop glue/closures/generics — flagged for follow-up.
  Full evidence, per-file symptom table, and methodology in
  `docs/analysis/phase44a-fallout/01-revealed-failures.md`. Follow-up task
  filed: `phase44b_collections-standalone-heap-corruption` (grouped, not
  1-per-test, since all 3 share the composition-sensitive heap-corruption
  signature). None of the 3 were already in `scripts/known-failures.txt`.

- [x] 1.3b `iter_ref` 0xC0000409 attribution — **resolved 2026-07-19**. Item 1.3
  flagged that this crash code matches the SEH-crossing crash from 1.1/1.2 and
  might mean the fix was incomplete for frames with drop glue/closures/generics.
  **Disproven by bisect:** restoring the `__try/__except` in `tml_run_test_seh`
  behind an `#ifdef` and rebuilding reproduces the crash **identically**, so the
  1.2 change is not the cause. Corroborating: the panic path works throughout
  this build (rich frame with live `Shared` + generic `List` + borrowing iterator
  + closure → clean `test_fail`; gate 24/24 both modes; `@should_panic` passing) —
  a broken shared unwinder would fail all of these. Rewriting iter_ref's checks as
  return codes (no panic, no longjmp anywhere) fails deterministically 6/6 with
  `h.strong_count()` not returning to 2 after the by-value `iter()` copy drops:
  a real refcount leak in the phase26e borrowing iterator, reachable with no panic
  at all. The minimal crash repro has no iterator and no collection — just
  `Shared::new` + `duplicate()` + a failing assert; the trigger is panicking while
  a local `Shared` binding is live (drop glue during unwind).
  Belongs to `phase44b`; recorded there under item 1.2 so it is not re-litigated.
  Methodological note: the 100ms watchdog fires first and reports `TIMEOUT`,
  masking the real failure — under `@slow_test` the same binary reaches the actual
  0xC0000409 after ~430ms. Part of 1.3's "timeout vs crash nondeterminism" is this
  masking, not purely heap-layout variance.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [x] 2.1 Update or create documentation covering the implementation (ADR-004 notes if the NDJSON contract text changes) — **done 2026-07-19**. `docs/specs/10-TESTING.md` §11.2
  (the NDJSON protocol contract): the example events were stale (`message`,
  `duration_ms`, no `index`/`file`) and now match what
  `testing_dispatcher_gen.cpp` actually emits. Added the failure contract —
  a panicking body MUST emit `test_fail` and exit non-zero, identically in
  standalone and suite-packed modes; `test_fail` carries `error` with the real
  panic message via `tml_test_error_json()`; `@should_panic` runs the same catch
  path with `quiet=1`; all string fields are JSON-escaped (the stream was
  previously invalid JSON for any Windows path). Release docs: `VERSION` → 0.3.83,
  `CHANGELOG.md` row, `docs/patches/v0.3.83.md` full notes (incl. the fallout
  survey and the 1.3b bisect).
- [x] 2.2 Write tests covering the new behavior (regression fixture: panicking `@test` must FAIL standalone — events + exit code) — **done 2026-07-19**.
  `compiler/tests/cli/dispatcher_panic_fail.sh` (committed gate). Three generated
  test files — a panicking `@test` (must FAIL), a `@should_panic` (must PASS,
  shares the setjmp/longjmp path), and a healthy sibling (must PASS, proves the
  failing test does not corrupt sibling results) — run in BOTH suite-packed and
  standalone (`--no-suite`) modes. Asserts on the terminal summary AND the JSON
  report derived from the raw NDJSON: non-zero process exit, `passed:false` for
  the panicking test, its `error` field carrying the actual panic message, a
  positively non-zero `exit_code`, and `passed:true` for the other two.
- [x] 2.3 Run tests and confirm they pass — **done 2026-07-19**. Gate **24/24** (12 assertions × 2 modes),
  confirmed by an independent run from the main session, not only by the
  implementing agent. First run was 9/24: 8 failures were assertions matching
  *function* names while the report uses *file stems*, and the rest were
  contamination from stray scratch fixture dirs under `compiler/tests/`
  (`dispfix_iso*`, `p44probe`) that prefix-matched `--suite=compiler/dispfix` —
  assertions corrected, scratch dirs removed. The `exit_code` check was also
  strengthened: it used `grep -qv '"exit_code":0'`, which would pass if the field
  were missing entirely, and now asserts a non-zero value positively.
  Regressions re-run on the final build: core/str 33/33, core/convert 24/24,
  compiler/borrow 12/12.
  **Correction on core/alloc.** Both agents reported 45/45. Two independent
  verification runs from the main session gave **43/45** (`allocator_ref_methods`
  + `shared_sync_edge`) and **44/45** (`shared_getmut`) — a *different* test each
  run, `HEAP_CORRUPTION (0xC0000374)` / exit `-1073740940`. That is the floating
  instability documented since v0.3.77, **not** a phase44a regression, but 45/45
  was a lucky run reported as stable; the honest baseline is 43–44/45. The
  signature (panicking or dropping while a local `Shared` binding is live → drop
  glue during unwind) is the same one 1.3b isolated for phase44b, so the
  core/alloc floating flaky and the collections standalone-only crashes are
  plausibly ONE bug — phase44b should test that hypothesis rather than assume two.
