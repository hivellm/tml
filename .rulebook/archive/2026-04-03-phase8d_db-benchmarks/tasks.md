# Tasks: Database Library — Benchmark Infrastructure

**Status**: Complete. 100% (14/14).
**Depends on**: phase8_db-foundation

## Phase 1: Benchmark Runner

- [x] 1.1 `db/bench/runner.tml` — BenchConfig { warmup_iterations, measure_iterations, config_name }
- [x] 1.2 BenchmarkResult::new(name, iterations, total_ns, min_ns, max_ns) with computed mean/ops
- [x] 1.3 BenchmarkResult with mean_ns, ops_per_sec, min_ns, max_ns, total_ns, iterations, summary()
- [x] 1.4 Statistical calculation functions: compute_mean, compute_ops, measure_ns(Instant)

## Phase 2: Benchmark Suite

- [x] 2.1 `db/bench/suite.tml` — BenchOp enum (InsertSingle, InsertBulk, SelectByPK, SelectAll, SelectFiltered, UpdateByPK, DeleteByPK, Transaction) with name()
- [x] 2.2 TechEmpower schema: techempower_world_sql(), techempower_fortune_sql()
- [x] 2.3 Seed data generation: seed_world_sql(id, random_number), seed_fortune_sql(id, message)

## Phase 3: Report Generation

- [x] 3.1 `db/bench/report.tml` — format_console(ref BenchmarkResult) -> Str
- [x] 3.2 format_csv(ref BenchmarkResult) + csv_header() -> Str
- [x] 3.3 format_json(ref BenchmarkResult) -> Str
- [x] 3.4 format_markdown(ref BenchmarkResult) + markdown_header() -> Str
- [x] 3.5 `db/bench/mod.tml` — exports runner, stats, suite, report, reference

## Phase 4: Cross-Language Benchmarks

- [x] 4.1 `db/bench/stats.tml` — mean(), ops_per_sec(), ns_to_us(), ns_to_ms(), format_duration()
- [x] 4.2 `db/bench/reference.tml` — Rust (SQLx) equivalent in doc comments
- [x] 4.3 Go (database/sql) equivalent in doc comments
- [x] 4.4 Node.js (better-sqlite3) equivalent in doc comments
- [x] 4.5 comparison_notes() function + TML (std::db) equivalent in doc comments

## Notes

- BenchResult/BenchStats are pub type aliases for BenchmarkResult/BenchmarkStats (backward compat)
- Struct field naming: BenchmarkResult uses br_name/br_stats; BenchmarkStats uses st_* prefix
  (avoids compiler codegen bug where method names matching field names causes all GEPs to resolve to index 0)
- Codegen bug workaround: GlobalASTCache caches struct layouts; renaming to BenchmarkResult/BenchmarkStats
  avoids collision with earlier stale cache entries for BenchResult/BenchStats
- Tests: lib/std/tests/db/db_bench.test.tml — 13 tests, 21/21 std/db suite passing
