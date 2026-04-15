## 1. Diagnosis
- [x] 1.1 Profiled `tml check hello.tml` cold: ~2.9s total. Meta preload "generating from source" takes ~2.5s (85% of total) parsing 418 library modules
- [x] 1.2 Root cause: meta cache dir (`build/debug/cache/meta/`) has no .tml.meta binary files. The module loading code READS from cache (`env_module_loading.cpp:277`) but never WRITES. Every `tml check` re-lexes and re-parses all 418 stdlib files.
- [x] 1.3 With warm cache (second run): still ~2.9s — confirming cache is never persisted

## 2. Implementation
- [ ] 2.1 Write meta cache: after `preload_all_meta_caches()` generates from source, serialize each module's metadata to `build/debug/cache/meta/<module_path>.tml.meta` using the existing `ModuleBinaryWriter`
- [ ] 2.2 On subsequent runs, load from `.tml.meta` binary (already implemented in reader path) — estimated speedup: 10-50x for meta loading (binary deserialize vs lex+parse)
- [ ] 2.3 Invalidation: compare source file hash (already computed via `compute_module_source_hash`) against cached hash header
- [ ] 2.4 Parallel module preloading (P2): use `std::async` for leaf modules that have no dependencies on each other
- [ ] 2.5 Release-mode compiler binary (P3): separate optimization, not part of this task

## 3. Benchmark Gate
- [ ] 3.1 GATE: `tml check hello.tml` cold must complete in under 1 second
- [ ] 3.2 GATE: `tml check` warm (with meta cache) must complete in under 500ms

## 4. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 4.1 Update or create documentation covering the implementation
- [ ] 4.2 Write tests covering the new behavior
- [ ] 4.3 Run tests and confirm they pass
