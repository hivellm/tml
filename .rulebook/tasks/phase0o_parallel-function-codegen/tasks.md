## 1. Diagnosis
- [ ] 1.1 Profile codegen phase for a file with 20+ functions — measure time spent per function and total emission time
- [ ] 1.2 Check if `llvm::LLVMContext` is shared across functions today — confirm thread-safety requirements for parallel use
- [ ] 1.3 Read LLVM threading docs: `llvm::ThreadSafeModule`, `llvm::LLVMContext` thread ownership, `llvm::Linker::linkModules` API

## 2. Implementation
- [ ] 2.1 Partition functions into N groups (N = CPU cores, capped at 8) — ensure closure/inline dependencies stay in the same group
- [ ] 2.2 Create one `llvm::LLVMContext` + `llvm::Module` per thread group
- [ ] 2.3 Emit each function group on its own thread using `std::async` — each thread calls the existing MIR→LLVM emission logic with its own context
- [ ] 2.4 After all threads complete, merge per-thread modules into the primary module via `llvm::Linker::linkModules`
- [ ] 2.5 Run the optimization + object emission pass on the merged module (single-threaded, as before)

## 3. Benchmark Gate
- [ ] 3.1 Compile a file with 50+ functions with and without parallel codegen — measure wall time
- [ ] 3.2 Compare vs Rust `rustc` compile time for equivalent number of functions
- [ ] 3.3 GATE: Parallel codegen must be at least 3x faster than single-threaded for a 50-function file. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=compiler` — output must be bit-for-bit identical to single-threaded codegen
- [ ] 4.2 Run `tml test --suite=core` — no regressions
- [ ] 4.3 Run with 1 thread (disable parallelism) and 8 threads — confirm same output

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `perf(codegen): parallel function-level IR emission using per-thread LLVM contexts`
- [ ] 5.2 Update `docs/analysis/benchmark/08-compilation.md` with parallel codegen timings
- [ ] 5.3 Run tests and confirm they pass
