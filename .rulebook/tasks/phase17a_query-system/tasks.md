# Tasks: Query System — Rewrite in TML

**Status**: Planned (0/18)
**Depends on**: phase16d (all codegen ported — full pipeline available)
**Blocks**: phase17c (bootstrap needs query system to orchestrate compilation)
**Duration**: 3–4 weeks
**Risk**: Medium — well-defined memoization pattern, stable API
**C++ reference**: 2,126 LOC → ~1,400 TML

---

## Phase 1: Query Types & Cache (5 items)

- [ ] 1.1 Create `compiler-tml/src/query/mod.tml` — module root
- [ ] 1.2 Create `compiler-tml/src/query/key.tml` — `QueryKey` enum: ReadSource, Tokenize, ParseModule, Typecheck, HirLower, ThirLower, MirBuild, CodegenUnit
- [ ] 1.3 Create `compiler-tml/src/query/cache.tml` — `QueryCache`: HashMap[QueryKey, CachedResult] with fingerprint validation
- [ ] 1.4 Implement `QueryCache.get(key) -> Maybe[CachedResult]` — return cached if fingerprint matches
- [ ] 1.5 Implement `QueryCache.store(key, result, fingerprint)` — cache with content hash

## Phase 2: Query Context & Execution (5 items)

- [ ] 2.1 Create `compiler-tml/src/query/context.tml` — `QueryContext` struct: cache, providers, options
- [ ] 2.2 Implement `force[T](key: QueryKey) -> T` — execute query or return cached result
- [ ] 2.3 Implement demand-driven pipeline: force(CodegenUnit) → force(MirBuild) → force(HirLower) → ... → force(ReadSource)
- [ ] 2.4 Implement dependency tracking: record which queries each query depends on
- [ ] 2.5 Implement cycle detection: error if query A → B → A

## Phase 3: Incremental Compilation (4 items)

- [ ] 3.1 Create `compiler-tml/src/query/incremental.tml` — persistent cache to `.incr-cache/incr.bin`
- [ ] 3.2 Implement fingerprinting: hash(source content) → fingerprint, compare with stored
- [ ] 3.3 Implement RED/GREEN coloring: changed inputs = RED, unchanged = GREEN, skip GREEN queries
- [ ] 3.4 Implement cache serialization: write/read `.incr-cache/incr.bin` in binary format

## Phase 4: Differential Testing (4 items)

- [ ] 4.1 Compile 20 stdlib modules through TML query system → verify identical output to C++
- [ ] 4.2 Test incremental: modify one file, recompile → verify only affected queries re-execute
- [ ] 4.3 Test cache invalidation: modify dependency → verify dependent queries re-execute
- [ ] 4.4 Full test suite with TML query system → zero IR-diff regressions

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
