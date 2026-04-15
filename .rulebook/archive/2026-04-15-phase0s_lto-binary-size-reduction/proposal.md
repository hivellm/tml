# Proposal: phase0s_lto-binary-size-reduction

## Why
TML-compiled binaries are 2.4x larger than equivalent Rust binaries (e.g., hello-world: 154 KB TML vs 64 KB Rust; math benchmark: 212 KB TML vs 89 KB Rust). The excess size comes from: (1) dead functions not removed because cross-module DCE requires LTO, (2) every function body duplicated for each generic instantiation without deduplication, (3) no whole-program dead-code elimination. Large binaries hurt cold startup time (OS page faults on load), instruction cache utilization, and distribution size. Rust achieves compact size via LTO (`-C lto=thin`) which runs whole-program inlining + DCE across all compilation units. See `docs/analysis/benchmark/08-compilation.md`.

## What Changes
1. Enable LLVM Thin LTO in the release build pipeline: after emitting `.bc` bitcode for each TML module, pass all bitcode files to `llvm::ThinLTO` which performs cross-module inlining + dead-code elimination.
2. Dead-function elimination: add an LLVM `internalize` pass before LTO that marks all non-exported functions as `internal` — allowing the global DCE pass to remove unreachable bodies.
3. Generic deduplication: with LTO, identical generic instantiations from different modules are merged (LLVM's `mergefunc` pass handles this).
4. LTO is only applied in release builds (`--release` flag from phase0i).

## Impact
- Affected specs: compiler/lto, build/binary-size
- Affected code: `compiler/src/codegen/llvm_context.cpp` (LTO pass pipeline), `scripts/build.bat` (linker flags)
- Breaking change: NO (LTO is release-only; debug binaries unchanged)
- User benefit: Target: TML binary size ≤1.2x Rust for equivalent programs. Faster startup, smaller distribution artifacts.
