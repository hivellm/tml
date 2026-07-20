# Proposal: phase0g_o0-ssa-cleanup

## Why
Every default build and all tests run IR with zero LLVM passes — measured ~7×
instruction bloat and call-per-element loops — because SSA cleanup is welded to
the same flag as checked arithmetic (analysis L-060, L-061, L-068).

## What Changes
A curated mem2reg/SROA/inline/DCE pipeline runs even at O0 while checked-math
semantics stay; an escape hatch preserves raw-IR debugging; `--emit-ir` stops
lying about which IR ships.

## Impact
- Affected specs: build/optimization docs
- Affected code: compiler/src/backend/llvm_backend.cpp, cli option plumbing
- Breaking change: NO (semantics unchanged; codegen quality changes)
- User benefit: default builds and the entire test suite run near-optimized code; felt performance and test wall-clock improve
