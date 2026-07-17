# Proposal: phase41a_test-aggregation-default

## Why
The default `tml test` mode compiles **one native EXE per test file**: `suite_mode = false` (`compiler/src/cli/commands/cmd_test.hpp:170`) forces `max_per_suite = 1` (`cmd_test.cpp:293`), producing ~1339 separate LLVM-codegen + LLD-link cycles per full run (cache proves it: 1339 EXEs, 837 MB in `build/debug/cache/tests/`). The aggregated models already exist and work behind opt-in flags — `--suite-mode` (10 files/EXE) and `--unified` (single binary, `compile_unified_binary` in `testing_compile_parallel.cpp:131`) — and were already approved by `docs/analysis/compiler-internals/single-binary-test-compilation.md` ("~10× reduction"), but neither ever became the default. Finding F-005 (+ F-011 subprocess overhead) in `docs/analysis/tooling-performance/04-test-framework-performance.md`.

## What Changes
- Aggregated compilation becomes the **default** for non-coverage `tml test` runs (per-file mode stays for coverage, which requires `max_per_suite=1`, and stays reachable via an explicit flag).
- Before flipping: verify the aggregated path preserves all observable semantics — per-test NDJSON reporting (ADR-004), filtering (`path=`/name filters), timeout attribution, and crash isolation (a crashing test in an aggregated EXE must not silently drop the sibling files' results; on suite-EXE compile failure, fall back to isolating the offending file).
- A/B measured before/after on representative suites and on a full non-coverage run.

## Impact
- Affected specs: none (tooling; ADR-004 protocol unchanged on the wire)
- Affected code: `compiler/src/cli/commands/cmd_test.hpp`, `cmd_test.cpp`, `compiler/src/testing/testing_compile_parallel.cpp`, `testing_coordinator.cpp` (defaults + fallback path only)
- Breaking change: NO (same CLI surface; per-file mode still available; coverage unchanged)
- User benefit: full non-coverage test runs drop from ~1339 codegen+link cycles to ~30 (or 1), directly attacking the dominant test wall-clock multiplier
