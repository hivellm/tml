# Proposal: phase0a_strict-moves-default-on

## Why
Move checking exists but is disabled by default behind `TML_STRICT_MOVES`. Its
blast radius was measured at zero (183 files, ADR-009 v0.3.65) and it was never
flipped. Users today get internal codegen errors instead of B001 diagnostics,
and the MIR path double-drops `let b = a` at runtime (probe-proven, analysis
L-020/L-021).

## What Changes
Strict moves become the default (env var becomes opt-out for one release); the
MIR-path double-drop is closed by wiring `mark_moved` or gating droppable
locals to the AST path; internal-error cases become real diagnostics.

## Impact
- Affected specs: docs/specs/06-MEMORY.md
- Affected code: compiler/src/borrow/checker_core.cpp, compiler/include/borrow/checker.hpp, compiler/src/mir/thir_mir_builder*.cpp or compiler/src/query/query_core.cpp
- Breaking change: YES (programs with latent move violations now get diagnostics — previously they miscompiled or corrupted)
- User benefit: real ownership errors at check time; no more silent double-drops
