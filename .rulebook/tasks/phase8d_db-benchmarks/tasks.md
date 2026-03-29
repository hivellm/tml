# Tasks: Database Library — Benchmark Infrastructure

**Status**: Planning. 0% (0/14).
**Depends on**: phase8_db-foundation

## Phase 1: Benchmark Runner

- [ ] 1.1 `db/bench/runner.tml` — BenchmarkRunner { warmup, iterations, cooldown }
- [ ] 1.2 run(name, setup, bench, teardown) -> BenchResult
- [ ] 1.3 BenchResult with mean, median, p50, p95, p99, ops_per_sec
- [ ] 1.4 Statistical calculation functions

## Phase 2: Benchmark Suite

- [ ] 2.1 Standard operations (insert_single, select_pk, bulk_insert, etc.)
- [ ] 2.2 TechEmpower schema (World, Fortune tables)
- [ ] 2.3 Seed data generation

## Phase 3: Report Generation

- [ ] 3.1 Console table formatter
- [ ] 3.2 CSV export
- [ ] 3.3 JSON export
- [ ] 3.4 Markdown table export
- [ ] 3.5 `db/bench/mod.tml` — exports

## Phase 4: Cross-Language Benchmarks

- [ ] 4.1 SQLite benchmarks
- [ ] 4.2 Rust (SQLx) equivalent
- [ ] 4.3 Go equivalent
- [ ] 4.4 Node.js equivalent
- [ ] 4.5 Comparison report template
