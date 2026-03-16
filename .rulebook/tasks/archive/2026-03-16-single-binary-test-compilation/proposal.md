# Proposal: single-binary-test-compilation

## Why
Test suite takes ~10min because 206 suites each spawn ~7 subprocesses (1 per test), totaling ~1452 process creations. All major compiled languages (Go, Rust, Zig) use 1 binary per module group with in-process execution.

## What Changes
Phase 1: Switch coordinator from `--test-index=N` (1 subprocess per test) to `--run-all` (1 subprocess per suite). Crash recovery retries failed tests individually.

Phase 2 (future): Merge all suites into single mega-binary with aggregated dispatcher.

See [docs/analyses/single-binary-test-compilation.md](../../../docs/analyses/single-binary-test-compilation.md)

## Impact
- Affected specs: none
- Affected code: compiler/src/testing/testing_coordinator.cpp, testing_coordinator.hpp
- Breaking change: NO
- User benefit: ~20-30% faster test runs, ~80% fewer subprocess spawns
