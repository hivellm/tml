# 2. Query-based Incremental Compilation (Red-Green Model)

**Status**: proposed
**Date**: 2026-03-15

## Context

Developer workflow requires fast rebuilds. TML implements a demand-driven query system modeled on rustc's TyCtxt. Each pipeline stage (read_source → tokenize → parse → typecheck → borrowcheck → hir_lower → mir_build → codegen_unit) is a memoized query with 128-bit CRC32C fingerprints.

## Decision

Implement Red-Green fingerprint system with binary cache format (TMIC magic). QueryContext::force<R>() template handles: cache lookup, cycle detection via DependencyTracker, provider execution, fingerprint computation, and disk persistence via IncrCacheWriter. Content-addressable object cache fingerprints LLVM IR strings to reuse .obj files.

## Alternatives Considered

- File timestamp-based invalidation (simpler but unreliable)
- Full recompilation every time (correct but slow)
- Make-style dependency tracking (coarser granularity)

## Consequences

Pros: Cached test runs ~6s vs ~5min cold. Identical IR from different sources reuses same .obj. Cons: Binary cache adds complexity; stale cache bugs are hard to diagnose. IncrCacheWriter needs mutex protection for concurrent suite workers. Cache invalidation is a non-obvious surface.
