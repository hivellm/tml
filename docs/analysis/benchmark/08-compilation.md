# 08 — Compilation & Binary Size

## Compilation Time

| File | Rust (s) | TML (s) | Ratio |
|------|---------|---------|-------|
| hashmap_bench | 0.39 | 9.83 | 25x |
| list_bench | 0.32 | 10.06 | 31x |
| collections_bench | — | 6.81 | — |
| math_bench | 0.26 | 7.07 | 27x |
| control_flow_bench | 0.27 | 7.24 | 27x |

**Average**: TML is **27x slower** to compile than Rust.

### Time Breakdown (estimated)

| Phase | Time (s) | % of Total |
|-------|---------|------------|
| Plugin/DLL loading | 2.0-3.0 | 30% |
| Parsing (C++ parser) | 0.5-1.0 | 10% |
| Type checking | 0.5-1.0 | 10% |
| MIR generation | 0.3-0.5 | 5% |
| LLVM IR generation | 1.0-2.0 | 20% |
| LLVM optimize + codegen | 1.5-2.5 | 25% |
| Linking (LLD in-process) | 0.3-0.5 | 5% |

### Root Causes

1. **Plugin loading (30%)**: `tml_compiler.dll` (82MB) + `tml_codegen_x86.dll` (63MB) = 145MB of DLLs loaded from disk on every compile. This is a constant 2-3s overhead regardless of program size.

2. **Debug compiler build**: The TML compiler itself is compiled without optimization (`-O0`). All C++ code in the compiler runs unoptimized.

3. **Single-threaded pipeline**: No parallel compilation of independent functions or modules.

4. **No precompiled headers**: Library modules (`core`, `std`) are re-type-checked from cached binary, but the cache load still takes time.

5. **Incremental compilation**: First compile is full; subsequent compiles use `incr.bin` cache. The benchmarks above are first-compile numbers.

### Incremental Comparison

| Scenario | TML (s) | Notes |
|----------|---------|-------|
| First compile | 7-10 | Full pipeline |
| Incremental (1 line change) | 3-5 | Cache hit, re-codegen |
| Incremental (cached) | 2-3 | Plugin load + cache check |

Rust with `cargo` incremental is ~0.5-1.5s for small changes. TML's incremental is 3-5s — still 3-5x slower.

### Daemon Mode (v0.3.16+)

`tml daemon start` keeps the compiler DLLs and LLVM context warm in a
background process. Subsequent compiles are served from an in-memory result
cache keyed on `(argv_hash, file_mtime)`.

| Scenario | Windows (debug) | Notes |
|----------|----------------|-------|
| `cargo check` no-change | ~98ms | Rust reference on same machine |
| TML daemon, first request (cache miss) | ~7.7s | Full compile |
| TML daemon, second request, no changes (cache hit) | **22ms** | Result cache; 4.5× faster than `cargo check` |
| TML daemon, compilation time (daemon-side only) | **<1ms** | Hash lookup + mtime check |

> **Platform note (Windows):** Process creation is ~21ms on Windows 10
> regardless of build mode (`tml --version` = 22ms baseline). The remaining
> 1ms above baseline is the named-pipe IPC round-trip plus JSON framing.
> On Linux/macOS the total would be ~3-5ms (fork ~1ms + pipe ~1ms).

**DLL staleness detection:** if `tml_compiler.dll` is rebuilt while the
daemon is running, the daemon exits cleanly on the next request with message
`"daemon: compiler updated, restarting"`. The next invocation auto-starts
a fresh daemon.

## Binary Size

| Program | Rust (KB) | TML (KB) | Ratio |
|---------|----------|---------|-------|
| hashmap_bench | 148 | 356 | 2.4x |
| list_bench | 139 | 352 | 2.5x |
| collections_bench | — | 347 | — |
| math_bench | 138 | 322 | 2.3x |
| control_flow_bench | 138 | 327 | 2.4x |

**Average**: TML binaries are **2.4x larger** than Rust.

### Size Breakdown (TML ~350 KB binary)

| Component | Size (KB) | % |
|-----------|----------|---|
| User code | ~30-50 | 12% |
| Runtime support | ~100-150 | 40% |
| Import tables | ~20-30 | 8% |
| Debug info / metadata | ~50-100 | 25% |
| Alignment padding | ~20-30 | 8% |
| Section headers | ~10 | 3% |

### Rust ~140 KB binary

Rust's smaller size comes from:
- No separate runtime (stdlib is statically linked with dead-code elimination)
- LTO (Link-Time Optimization) strips unused functions
- Minimal metadata in release builds

### Runtime Dependencies

TML programs depend on runtime DLLs:

| DLL | Size | Required For |
|-----|------|-------------|
| libcrypto-3-x64.dll | 4.5 MB | Crypto (if used) |
| brotlienc.dll | 3.2 MB | Compression (if used) |
| sqlite3.dll | 1.0 MB | Database (if used) |
| libssl-3-x64.dll | 804 KB | TLS (if used) |
| zstd.dll | 637 KB | Compression (if used) |
| brotlicommon.dll | 135 KB | Brotli shared |
| zlib1.dll | 88 KB | zlib |
| brotlidec.dll | 50 KB | Brotli decompression |
| **Total** | **~10.4 MB** | |

These are copied to the binary directory but only loaded if the program uses the corresponding features.

### Compiler Toolchain Size

| Component | Size |
|-----------|------|
| tml.exe | 756 KB |
| tml_compiler.dll | 77.7 MB |
| tml_codegen_x86.dll | 60.1 MB |
| tml_daemon.exe | 721 KB |
| tml_mcp.exe | 3.1 MB |
| **Total** | **~142 MB** |

Rust's `rustc` + LLVM: ~300-500 MB (but shared across all projects).

## Improvement Opportunities

| Priority | Change | Expected Impact |
|----------|--------|-----------------|
| P0 | Release build of TML compiler | 2-3x compile speed |
| P0 | Lazy-load plugins (only load on first use) | -2s per compile |
| P1 | Memory-map DLLs instead of full load | -1s per compile |
| P1 | Parallel function codegen | 2-3x for multi-function programs |
| P1 | LTO for TML binaries | 30-40% binary size reduction |
| P2 | Precompile core/std to bitcode | -1-2s for type-checking |
| P2 | Strip debug info in release | 20-30% binary size reduction |
