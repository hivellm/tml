## 1. Diagnosis
- [ ] 1.1 Emit IR for `benchmarks/profile_tml/math_bench.tml --stage=parser:cpp` — verify O0 attributes (`optnone`, no `alwaysinline`)
- [ ] 1.2 Check `llvm_context.cpp` for current optimization level setting — document exact call site
- [ ] 1.3 Read LLVM PassBuilder docs: `llvm/include/llvm/Passes/PassBuilder.h` — identify the correct O2 pipeline call

## 2. Implementation
- [ ] 2.1 Add `BuildMode` enum (Debug / Release) to compiler build options if not already present
- [ ] 2.2 In `llvm_context.cpp` (or codegen entry point): when `Release`, call `PassBuilder::buildPerModuleDefaultPipeline(O2)` and run it on the module before emitting object code
- [ ] 2.3 Add `--release` flag to `cmd_build.cpp`, `cmd_run.cpp`, `cmd_test.cpp` — propagate to `BuildMode::Release`
- [ ] 2.4 Parse `[profile.release] optimize = 2` in `tml.toml` (map to `BuildMode::Release`)

## 3. Benchmark Gate
- [ ] 3.1 Compile and run `benchmarks/profile_tml/math_bench.tml --stage=parser:cpp --release` — capture all arithmetic results
- [ ] 3.2 Compare vs Rust `-O` baseline from `docs/analysis/benchmark/02-math-arithmetic.md`
- [ ] 3.3 GATE: Float add must be <0.5 ns/op, Int div must be <1 ns/op with `--release`. Ratio vs Rust must be <1.5x. Do NOT proceed to next task if gate fails.
- [ ] 3.4 Run struct benchmark with `--release` — struct access must improve from 10-18x to <3x vs Rust

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core --release` — no regressions vs debug output
- [ ] 4.2 Run `tml test --suite=compiler` (debug, unchanged) — no regressions
- [ ] 4.3 Verify `--release` does not affect debug build path (O0 still default)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `feat(compiler): add --release flag backed by LLVM O2`
- [ ] 5.2 Write test: `compiler/tests/release_mode_basic.test.tml` — compiles with `--release`, output is correct
- [ ] 5.3 Run tests and confirm they pass
