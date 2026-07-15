# Proposal: phase28a_uzdb-core-acceptance

## Why

UzDB (E:\UzmiGames\UzDB) — the application that motivated the entire
stabilization era — was abandoned and rewritten in Rust because TML could not
sustain exactly this workload: long-lived BTreeMaps of owned rows, snapshot
copies, refcounted shared state, tight alloc/free loops. Passing the compiler
test suite does not prove the language is usable; only an application-scale
program running under sustained load does. This task rebuilds UzDB's minimal
core as the acceptance gate that re-earns the lost use case.

## What Changes

A small TML application (in `.sandbox`-adjacent `examples/` or `apps/` —
decide at implementation): in-memory `BTreeMap[I64, Row]` store (Row with
nested Str/Buffer/List fields), append-only msgpack-framed commit log with
`File::sync` durability and replay-on-open, snapshot reads over key ranges,
and a soak test driving millions of mixed operations under the phase25a
adversarial allocator. Results (determinism ×100, leak check, ops/sec vs a
Rust toy equivalent) recorded in
`docs/analysis/tml-table-analysis/08-acceptance-results.md`.

## Impact

- Affected specs: none (application + test).
- Affected code: new example/acceptance app + soak harness; no compiler
  changes expected (failures found here reopen phase26/27).
- Breaking change: NO.
- User benefit: a data-backed answer to "is TML usable for real apps";
  the exact workload that killed UzDB becomes a permanently-guarded
  regression gate.

## Source

- docs/analysis/tml-table-analysis/01-context-uzdb-failure.md +
  06-execution-plan.md (Phase D2).
- E:\UzmiGames\UzDB\docs\specs\00-gaps-analysis.md (original requirements).
