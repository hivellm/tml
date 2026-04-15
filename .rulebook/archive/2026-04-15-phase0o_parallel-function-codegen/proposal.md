# Proposal: phase0o_parallel-function-codegen

## Why
TML's LLVM IR emission is single-threaded: functions are codegen'd one at a time on the main thread. Rust's `rustc` uses LLVM's `ThreadSafeModule` + `lto::run_pass_manager` to parallelize codegen across available CPU cores. On an 8-core machine this gives `rustc` a free 4-6x throughput multiplier for large files with many functions. TML does not parallelize at all. For the self-hosting compiler (which will have thousands of functions) this single-threaded bottleneck will dominate compile time. Parallelizing function-level codegen is the largest remaining compile-time gain after DLL caching and release builds. See `docs/analysis/benchmark/08-compilation.md`.

## What Changes
1. The MIR→LLVM emission phase will be split: function IR construction moves to a thread pool (`std::async` / `std::thread` pool), with each function emitting into its own `llvm::Function` under a shared `llvm::Module` protected by a mutex (or using per-thread modules merged at the end via `llvm::Linker::linkModules`).
2. The safer approach (fewer lock points): use per-thread `llvm::LLVMContext` + `llvm::Module`, emit each function independently, then link all modules into one with `llvm::Linker::linkModules` before the optimization/emission pass.
3. Thread count: respect `std::thread::hardware_concurrency()`, capped at 8.
4. Functions with cross-function dependencies (closures capturing locals, inline functions) are kept on the main thread.

## Impact
- Affected specs: compiler/codegen-parallelism, build/compilation-latency
- Affected code: `compiler/src/codegen/mir_llvm_builder.cpp`, possibly `compiler/src/codegen/llvm_context.cpp`
- Breaking change: NO
- User benefit: 4-6x compile-time speedup for files with 50+ functions (compiler internals, large application modules).
