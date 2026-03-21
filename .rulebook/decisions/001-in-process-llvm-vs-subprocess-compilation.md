# 1. In-process LLVM vs Subprocess Compilation

**Status**: proposed
**Date**: 2026-03-15

## Context

TML compiler needs to transform LLVM IR into object files. Two approaches: embed LLVM as a library (in-process) or invoke clang/llc as subprocess. The test linker specifically needs subprocess mode due to LLVM global state accumulation after compiling many suites.

## Decision

Embed LLVM in-process for the compiler (fast single-file compilation), but force subprocess LLD for test linking (lldMain deadlocks after accumulated LLVM global state). SEH wrapping provides crash isolation per suite.

## Alternatives Considered

- Full subprocess compilation (slower but no state issues)
- In-process for everything (deadlocks in test suite)
- LLVM as a separate daemon process (complex IPC)

## Consequences

Pros: Fast compilation, no subprocess overhead for normal builds. Cons: LLVM thread-safety issues require single-threaded per-file compilation, 30s codegen timeout watchdog, SEH wrapping on Windows. Test system complexity increased significantly. Future risk: any LLVM upgrade may introduce new global state issues.
