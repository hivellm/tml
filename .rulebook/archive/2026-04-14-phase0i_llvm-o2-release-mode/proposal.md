# Proposal: phase0i_llvm-o2-release-mode

## Why
Every TML compilation runs with LLVM at O0 (no optimization). Rust benchmarks use `-O` (equivalent to O2). This single difference is the root cause of most multi-cycle gaps measured in the benchmark analysis: struct access 10-18x, function pointers 8x, closures 3-5x, List ops 3-5x. Enabling O2 for TML's release build path would allow LLVM to inline, constant-fold, eliminate dead loads, vectorize, and produce CMOV/select instructions automatically — closing many gaps without a single line of TML source change. Current compilation time: 145ms warmup + ~50ms codegen. With O2 the codegen phase will increase (expect 2-3x), but binary execution will improve dramatically. This task adds `--release` build support backed by O2.

## What Changes
1. `compiler/src/codegen/llvm_context.cpp` (or equivalent): when `build_mode == Release`, set `module.setOptimizationLevel(O2)` and pass the standard O2 pass pipeline via `llvm::PassBuilder`.
2. `tml.exe` CLI: add `--release` flag to `build`, `run`, and `test` commands that sets `BuildMode::Release`.
3. `tml.toml`: add `[profile.release] optimize = 2` support (mirrors Cargo's `[profile.release]`).
4. Benchmark harness: all benchmarks in `benchmarks/profile_tml/` must be run with `--release` for fair Rust comparison going forward.

## Impact
- Affected specs: compiler/build-modes, codegen/optimization
- Affected code: `compiler/src/codegen/llvm_context.cpp`, `compiler/src/cmd/cmd_build.cpp`, `compiler/src/cmd/cmd_run.cpp`
- Breaking change: NO (debug remains default; `--release` is opt-in)
- User benefit: First benchmark gate where TML matches Rust's optimization level — reveals the true performance gap that cannot be closed by codegen fixes alone.
