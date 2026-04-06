# Proposal: Query System — Rewrite in TML

## Why

The query system is the orchestration backbone of the entire TML compiler pipeline. It implements
demand-driven memoization: each compilation stage (ReadSource → Tokenize → Parse → Typecheck →
HirLower → ThirLower → MirBuild → CodegenUnit) is a query that is computed once and cached. This
enables incremental compilation: when a source file changes, only the queries that depend on that
file re-execute. Without porting the query system to TML, the self-hosted compiler must still
delegate orchestration to C++ — which means phase17c bootstrap cannot be achieved. The query
system is the final coordination layer that must be owned by TML.

## What Changes

Port six C++ files (~2,126 LOC) to TML (~1,400 LOC):

- `query_core.cpp` (809 LOC) — query execution engine, provider registration, force()
- `query_context.cpp` (448 LOC) — QueryContext struct, pipeline wiring
- `query_incr.cpp` (613 LOC) — incremental cache: fingerprints, RED/GREEN coloring, .incr-cache/incr.bin
- `query_cache.cpp` (77 LOC) — in-memory HashMap[QueryKey, CachedResult]
- `query_fingerprint.cpp` (78 LOC) — content hashing for source files
- `query_deps.cpp` (65 LOC) — dependency tracking between queries

Key design decisions:
- `QueryKey` as a TML enum (not type-erased void*) — safer, pattern-matchable
- Cache backed by `HashMap[QueryKey, CachedResult]` from std::collections
- Fingerprint = hash(source content) — same algorithm as C++ baseline
- RED/GREEN coloring matches C++ semantics: RED = input changed, GREEN = cached result valid
- Cycle detection via in-flight Set[QueryKey] — error on re-entry

## Impact

- Affected specs: `docs/specs/query-system.md`
- Affected code: `compiler-tml/src/query/` (new), no changes to existing C++ compiler
- Breaking change: NO — C++ compiler remains primary until phase17c
- User benefit: Enables phase17c bootstrap — the self-hosted compiler gains full incremental
  compilation support identical to the C++ compiler

## Success Criteria

- 20 stdlib modules compile through TML query system with zero IR-diff vs C++ baseline
- Incremental recompilation skips unchanged queries (verified by execution trace)
- `.incr-cache/incr.bin` is read/written correctly across compiler invocations
- Full 1,700+ test suite produces zero regressions when query system is active

## Dependencies

- **Requires**: phase16d complete (full codegen pipeline available as query providers)
- **Blocks**: phase17c (bootstrap: tml-stage1 must orchestrate via TML query system)
