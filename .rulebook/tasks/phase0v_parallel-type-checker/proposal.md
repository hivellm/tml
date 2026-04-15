# Proposal: phase0v_parallel-type-checker

## Why

The type-checker is the single largest bottleneck in TML cold compilation,
consuming ~60% of total compile time (~3s out of ~7s for a typical file).
It runs entirely single-threaded: module loading, type resolution, trait
checking, and inference all happen sequentially on the main thread.

Profiling shows:
- Plugin/DLL loading: ~2s (solved by daemon)
- **Type checking: ~3s** (single-threaded, dominates cold compile)
- IR generation: ~1s (now parallel via CGUs)
- LLVM compilation: ~1s (already parallel via compile_cgus_parallel)

To match Rust's `cargo check` cold performance (~0.3-1s), the type-checker
needs to run in <500ms. The main opportunities:

1. **Parallel module loading**: preload and type-check independent modules
   on separate threads (dependency graph allows leaf modules in parallel)
2. **Lazy type resolution**: defer trait checking until a type is actually
   used in codegen (omit unused types entirely)
3. **Meta cache preloading**: the .tml.meta binary cache is loaded
   sequentially; could be memory-mapped or prefetched in parallel
4. **Type-checker in release mode**: the C++ type-checker itself runs at
   -O0 (debug build); a release-mode compiler binary would be 3-5x faster

## What Changes

1. Profile the type-checker to identify the exact hot path (module load,
   trait resolution, generic instantiation, import resolution)
2. Implement parallel module preloading using std::async for independent
   modules in the import graph
3. Memory-map .tml.meta files instead of sequential file I/O
4. Consider lazy type resolution for unused imports

## Impact

- Affected specs: compiler/types/checker, compiler/types/module_loading
- Affected code: `compiler/src/types/env_module_load*.cpp`, `compiler/src/types/checker/`
- Breaking change: NO
- User benefit: Cold `tml check` drops from ~3s to <1s, making TML
  competitive with `cargo check` for single-file compilation.
