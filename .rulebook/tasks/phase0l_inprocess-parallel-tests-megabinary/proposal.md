# Proposal: phase0l_inprocess-parallel-tests-megabinary

## Why
The last structural test-speed lever. The isolation machinery already runs
in-process per-test; only process-global state keeps it single-threaded. The
old premise (100 MB DLL loads per test process) was measured false — the real
cost is ~176 codegen+link cycles, which only a mega-binary removes (analysis
L-101, L-105).

## What Changes
Panic/catch state becomes thread-local (Windows+POSIX); tests run on a thread
pool from a fn-pointer table with a suite-level deadline; one aggregated binary
per run replaces ~176 links; stale cost claims corrected.

## Impact
- Affected specs: testing docs (ADR-004 execution model update)
- Affected code: compiler/runtime/core/essential.c, compiler/src/testing/testing_dispatcher_gen.cpp, compiler/src/codegen/llvm/core/generate_entry.cpp, cmd_test plumbing
- Breaking change: NO (subprocess fallback retained; crash policy unchanged)
- User benefit: full-suite wall-clock drops from link-bound to execution-bound; failures no longer hide behind fail-fast
