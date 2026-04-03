# Proposal: DB Benchmark Infrastructure

## Why
Benchmark suite to measure DB performance, compare with other languages, and track regressions. TechEmpower-compatible for industry comparison.

## What Changes
- BenchmarkRunner with warmup, measurement, statistical analysis (p50/p95/p99)
- Standard operations suite (insert, select, bulk, join, transaction)
- Report generation (console, CSV, JSON, markdown)
- Cross-language benchmarks (Rust/SQLx, Go, Node.js)

## Impact
- Affected code: lib/std/src/db/bench/ (new), benchmarks/db/ (new)
- Breaking change: NO
- User benefit: Performance visibility, regression detection
