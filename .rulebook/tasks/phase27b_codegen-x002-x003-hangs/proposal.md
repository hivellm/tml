# Proposal: phase27b_codegen-x002-x003-hangs

## Why

The compiler non-terminates (X002) on `let_patterns`, `slice_split_pred`,
and `builtins_imports`, and crashes (X003) on `closure_codegen` — all valid
programs exercising CORE language features (PLANS.md standing failures).
These have been "pre-existing" in every patch note since at least 0.3.45.
A compiler that hangs on let-patterns and crashes on closures cannot be
trusted for a large codebase; any real application will eventually write
the triggering shape and have no workaround.

## What Changes

Root-cause and fix each standing X002/X003 (plus the core/any T056 rider):
profile the hang to the non-terminating phase (type-inference loop? MIR
fixpoint? parser), fix the mechanism, and add a compile-time watchdog that
reports WHICH phase and item exceeded its time budget — turning any future
X002 from a mystery timeout into an actionable bug report.

## Impact

- Affected specs: none.
- Affected code: the phases identified by profiling (likely
  `compiler/src/types/` inference and `compiler/src/mir/` passes),
  plus a watchdog in the driver.
- Breaking change: NO.
- User benefit: full compiler suite green ×100 consecutive runs; core
  language features carry zero timeout/crash risk.

## Source

- docs/analysis/tml-table-analysis/03-codegen-stability.md (F-006).
- .rulebook/PLANS.md standing-failures list.
