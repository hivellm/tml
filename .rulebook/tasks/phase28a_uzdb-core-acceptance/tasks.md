# phase28a — UzDB-Core Acceptance App + Soak Test (Stabilization ERA 0, Phase D2)

> Analysis: `docs/analysis/tml-table-analysis/06-execution-plan.md` (Phase D).
> THE real gate for "TML is usable": rebuild the minimal core of the application
> that was abandoned (UzDB, `E:\UzmiGames\UzDB`) and prove it survives sustained
> load. Exercises F-001/F-002 at application scale. Requires phases 26 + 27 done.

## 1. Implementation
- [ ] 1.1 In-memory store: `BTreeMap[I64, Row]` where `Row` is a struct with nested `Str`/`Buffer`/`List` fields (the exact shape that triggered the aliasing class); insert / get / update / delete API
- [ ] 1.2 Append-only commit log: msgpack-framed records written through `File` with `File::sync` durability points; replay-on-open recovery
- [ ] 1.3 Snapshot reads: point-in-time copy of a key range (stresses copy semantics of nested owned handles — the MVCC-shaped workload)
- [ ] 1.4 Soak test: millions of mixed insert/read/update/delete/snapshot cycles under the phase25a adversarial allocator; memory usage flat (no leak from over-cloning), zero crashes
- [ ] 1.5 Determinism gate: soak binary ×100 runs = 100/100; leak check via `mcp__tml__debug check_leaks=true` clean
- [ ] 1.6 Performance sanity: ops/sec recorded and compared against the equivalent Rust toy (does not need parity — needs to be in the same order of magnitude and stable)
- [ ] 1.7 Write up the result as `docs/analysis/tml-table-analysis/08-acceptance-results.md` — this closes or reopens the "TML is usable for real apps" question with data

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
