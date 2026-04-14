## 1. Diagnosis
- [ ] 1.1 Measure current binary sizes: compile `benchmarks/profile_tml/math_bench.tml --stage=parser:cpp --release` and a Rust equivalent — record sizes in bytes
- [ ] 1.2 Run `llvm-nm` on the TML binary — count how many symbols are dead (defined but not referenced from `main`)
- [ ] 1.3 Confirm LLVM ThinLTO API availability: check `llvm/include/llvm/LTO/LTO.h` exists in the LLVM headers used by the project

## 2. Implementation
- [ ] 2.1 In the release build codegen path: emit `.bc` bitcode (not object file) for each module with `llvm::WriteBitcodeToFile`
- [ ] 2.2 After all modules are emitted, run the `internalize` pass: mark all symbols not in the export list as `internal` linkage
- [ ] 2.3 Run `llvm::ThinLTOCodeGenerator` or `PassBuilder::buildLTODefaultPipeline` on the set of `.bc` files — produces a single optimized object
- [ ] 2.4 Link the LTO-optimized object instead of per-module objects
- [ ] 2.5 Add `mergefunc` pass to deduplicate identical generic instantiations

## 3. Benchmark Gate
- [ ] 3.1 Compile `math_bench.tml --release` with LTO — measure binary size in bytes
- [ ] 3.2 Compare vs Rust `-C lto=thin` binary size from `docs/analysis/benchmark/08-compilation.md`
- [ ] 3.3 GATE: TML release binary must be ≤1.5x Rust equivalent binary size. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run compiled binary — output must be correct (LTO must not eliminate live functions)
- [ ] 4.2 Run `tml test --suite=core --release` — all tests pass
- [ ] 4.3 Verify debug builds are unchanged (LTO must only apply with `--release`)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `perf(build): enable Thin LTO in release builds for smaller binaries`
- [ ] 5.2 Update `docs/analysis/benchmark/08-compilation.md` with new binary size measurements
- [ ] 5.3 Run tests and confirm they pass
