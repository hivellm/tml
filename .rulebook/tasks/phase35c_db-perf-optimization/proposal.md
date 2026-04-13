# Proposal: DB Performance Optimization — Close the Gap with Rust

## Why
TML SQLite is 2.7x slower than Rust on INSERT, 8x on SELECT. Root cause is string-based SQL
construction instead of prepared statement reuse. Benchmark analysis shows the gap is entirely
addressable without compiler changes — TML's FFI layer is already fast.

## What Changes
- Tier 1 (Quick wins): Rewrite benchmark with prepare/bind/step/reset, run in release mode,
  add explicit BEGIN/COMMIT. Expected: 2-3x speedup.
- Tier 2 (Library): Add execute_with_params() API, LRU statement cache, batch insert API.
  Expected: additional 1.5x.
- Tier 3 (Compiler): Outcome niche optimization, I64.to_string() stack buffer, Str escape analysis.
  Expected: additional 1.1-1.3x.

## Impact
- Affected code: lib/std/src/db/, lib/std/src/sqlite/, benchmarks/db-sqlite/
- Breaking change: NO (all new APIs are additive)
- User benefit: 2-3x faster database operations, competitive with Rust/Node.js
