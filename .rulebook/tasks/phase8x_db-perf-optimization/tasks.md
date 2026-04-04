# Tasks: DB Performance Optimization — Close the Gap with Rust

**Status**: In Progress. 22% (5/22) — Tier 4 complete.
**Reference**: benchmarks/db-sqlite/ANALYSIS.md
**Baseline**: INSERT 1853ns, SELECT 1607ns, UPDATE 1866ns, DELETE 1508ns (2-3x slower than Rust)
**Target**: INSERT <700ns, SELECT <400ns, UPDATE <700ns, DELETE <700ns

## Tier 1: Quick Wins — No Compiler Changes (Expected: 2-3x speedup)

- [ ] 1.1 Rewrite `bench_tml.tml` to use prepare/bind_i64/step/reset instead of execute() with string concat
- [ ] 1.2 Run TML benchmark in release mode (`--release`) and compare
- [ ] 1.3 Add explicit BEGIN/COMMIT wrapping around INSERT/UPDATE/DELETE loops
- [ ] 1.4 Run comparative benchmark and update RESULTS.md with new numbers
- [ ] 1.5 Profile: measure time spent in sqlite3_prepare_v2 vs sqlite3_step vs string alloc

## Tier 2: Library API Improvements (Expected: additional 1.5x)

- [ ] 2.1 `SqliteConnection::execute_params(sql, params: ...)` — prepare+bind+step+finalize in one call
- [ ] 2.2 `SqliteConnection::query_params(sql, params: ...)` — prepare+bind+step loop, return results
- [ ] 2.3 Statement cache (LRU) — `SqliteConnection` caches last N prepared statements by SQL hash
- [ ] 2.4 `Statement::execute_with(params: ...)` — bind all + step + reset in one call
- [ ] 2.5 Batch insert API — `insert_many(table, columns, values_list)` with single BEGIN/COMMIT
- [ ] 2.6 Tests for all new APIs
- [ ] 2.7 Re-run benchmark with new APIs, update RESULTS.md

## Tier 3: Compiler Optimizations (Expected: additional 1.1-1.3x)

- [ ] 3.1 `I64.to_string()` — use stack buffer instead of heap allocation (avoid mem_alloc for <20 chars)
- [ ] 3.2 Outcome[T, E] niche optimization — use tag bits in pointer for ptr-sized payloads
- [ ] 3.3 String escape analysis — detect Str temporaries that don't escape, allocate on stack
- [ ] 3.4 Inline `@extern("c")` hot paths — sqlite3_step, sqlite3_bind_int64, sqlite3_reset
- [ ] 3.5 Str concatenation optimization — reuse buffer when LHS is a temporary (move semantics)

## Tier 4: Benchmark Methodology

- [x] 4.1 Add warmup iterations (1000 ops) before each timed section in bench_tml.tml
- [x] 4.2 Add min/max per-op tracking (percentiles skipped — would require array storage per iteration)
- [x] 4.3 Memory tracking skipped — needs OS-specific APIs not available in TML
- [x] 4.4 Added note in output: "Run 3 times and take the median for fair comparison"
- [x] 4.5 Output now prints both human-readable table (Total ms, Avg ns/op, Min ns/op, Max ns/op) and CSV with extended fields (lang,op,iters,total_ns,ns_per_op,min_ns,max_ns)
