## 1. Diagnosis
- [ ] 1.1 Profile DLL load time: run `tml build hello.tml --stage=parser:cpp` with Windows PerfView or `GetTickCount64()` instrumentation — confirm 145ms DLL load attribution
- [ ] 1.2 Locate `plugin_loader.cpp` (or equivalent) — read the current `LoadLibrary`/`GetProcAddress` call sequence
- [ ] 1.3 Measure Rust baseline: `rustc` cold compile of a trivial `hello.rs` — record wall time

## 2. Implementation
- [ ] 2.1 Implement process-level DLL handle cache: `static std::unordered_map<std::string, HMODULE> g_dll_cache` keyed by canonical DLL path, checked before every `LoadLibraryW` call
- [ ] 2.2 Add mtime validation: re-load only when DLL file mtime changes (invalidates stale cache after compiler rebuild)
- [ ] 2.3 Add background preload: in `main()`, spawn a `std::thread` that loads both DLLs immediately while the CLI parses arguments — join before first compilation
- [ ] 2.4 Use `LOAD_LIBRARY_SEARCH_DEFAULT_DIRS` flag to avoid redundant path searches on each `LoadLibraryW`

## 3. Benchmark Gate
- [ ] 3.1 Run 10 consecutive `tml build hello.tml --stage=parser:cpp` compilations — measure wall time for each
- [ ] 3.2 Compare vs Rust `rustc hello.rs` wall time
- [ ] 3.3 GATE: Second and subsequent compilations (warm cache) must be <20ms. Cold first compile must be <60ms. Ratio vs Rust must be <3x. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=compiler` — no regressions
- [ ] 4.2 Verify cache invalidation works: touch `tml_compiler.dll`, confirm next compile reloads it
- [ ] 4.3 Verify cache hit path: two consecutive compiles, second must not call `LoadLibraryW` for the same path

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `perf(loader): cache DLL handles + background preload, −130ms cold compile`
- [ ] 5.2 Update `docs/analysis/benchmark/08-compilation.md` with new compile-time measurements
- [ ] 5.3 Run tests and confirm they pass
