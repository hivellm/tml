# 01 — Executive Summary

## TML vs Rust: Performance Ratio Overview

| Category | Ratio (TML/Rust) | Rating |
|----------|-----------------|--------|
| Integer arithmetic | 1.0-1.5x | Excellent |
| Float arithmetic | 1.0x | Excellent |
| Bitwise operations | 1.0x | Excellent |
| Control flow (simple) | 1.0-2.0x | Good |
| Control flow (when/match) | 3.0-4.0x | Moderate |
| Short-circuit booleans | 4.0x | Moderate |
| List (push/pop) | 2.0-2.5x | Moderate |
| List (access/iterate) | 3.0-5.0x | Poor |
| HashMap (insert/lookup) | 1.3-1.6x | Good |
| HashMap (remove) | 0.8x | TML wins |
| Struct creation/access | 4.0-15.0x | Poor |
| Array bulk ops | 0.01x | TML wins |
| Function calls (inline) | 1.0x | Excellent |
| Function pointer dispatch | 8.0x | Poor |
| Recursion (fib n=20) | 2.0x (real) | Good |
| Base64 encoding | 2.4-3.1x | Moderate |
| Compilation time (cold) | 25-31x | Critical |
| Compilation time (daemon cached) | 0.22x (4.5x faster) | Excellent |
| Binary size | 2.3-2.5x | Moderate |

## Key Strengths

1. **Raw arithmetic matches Rust** — integer add/mul/div/mod and float ops are identical
2. **HashMap is competitive** — only 1.3-1.6x slower, remove is actually faster
3. **Array bulk operations are faster** — memcpy/memset intrinsics outperform Rust
4. **Inline function calls are optimal** — 1 ns/op, same as Rust

## Key Weaknesses

1. **Compiler optimizations missing** — no inlining, no dead-code elimination, no constant folding at TML level (LLVM does some but TML debug build limits it)
2. **Struct operations are slow** — 4-15x gap from alloca/load/store instead of register promotion
3. **Cold compilation is 25-31x slower** — plugin loading + single-threaded pipeline. However, `tml daemon` (v0.3.16+) caches results in-memory: **22ms with cache hit, 4.5x faster than `cargo check` (~98ms)**
4. **Memory leaks detected** — encoding benchmark leaked 200K allocations (2.8MB)
5. **K001 codegen bugs** — string, JSON, and crypto benchmarks can't even run

## Daemon Mode Impact (v0.3.16+)

The cold benchmarks above reflect first-time compilation. With `tml daemon start`:

| Scenario | Time | vs `cargo check` |
|----------|------|-------------------|
| No-change rebuild (daemon cache hit) | **22ms** | **4.5x faster** (98ms) |
| Daemon-side compilation time | **<1ms** | Hash lookup + mtime check |
| DLL staleness detection | Automatic | Daemon exits on compiler rebuild |

This transforms TML into one of the fastest edit-compile-test cycles available.
See [08-compilation.md](08-compilation.md) for full daemon benchmarks.

## Previously Broken Benchmarks (Fixed in v0.3.x)

| Benchmark | Error | Status |
|-----------|-------|--------|
| string_bench | K001: undefined `@tml_N4core3str3lenE_S` | **Fixed** in v0.3.3 |
| json_bench | K001: `i32` vs `i1` type mismatch | **Fixed** in v0.3.4 |
| crypto_bench | N002: linking failure, missing .obj | **Fixed** in v0.3.5 |
