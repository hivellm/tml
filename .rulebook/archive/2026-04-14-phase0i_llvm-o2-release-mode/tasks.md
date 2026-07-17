## 1. Diagnosis
- [x] 1.1 Emit IR for `benchmarks/profile_tml/math_bench.tml --stage=parser:cpp` — verify O0 attributes (`optnone`, no `alwaysinline`)
- [x] 1.2 Check `llvm_context.cpp` for current optimization level setting — document exact call site
- [x] 1.3 Read LLVM PassBuilder docs: `llvm/include/llvm/Passes/PassBuilder.h` — identify the correct O2 pipeline call

## 2. Implementation
- [x] 2.1 Add `BuildMode` enum (Debug / Release) to compiler build options if not already present
- [x] 2.2 In `llvm_context.cpp` (or codegen entry point): when `Release`, call `PassBuilder::buildPerModuleDefaultPipeline(O2)` and run it on the module before emitting object code
- [x] 2.3 Add `--release` flag to `cmd_build.cpp`, `cmd_run.cpp`, `cmd_test.cpp` — propagate to `BuildMode::Release`
- [x] 2.4 Parse `[profile.release] optimize = 2` in `tml.toml` (map to `BuildMode::Release`)

## 3. Benchmark Gate
- [x] 3.1 Compile and run `benchmarks/profile_tml/math_bench.tml --stage=parser:cpp --release` — capture all arithmetic results
- [x] 3.2 Compare vs Rust `-O` baseline from `docs/analysis/benchmark/02-math-arithmetic.md`
- [x] 3.3 GATE: Float add must be <0.5 ns/op, Int div must be <1 ns/op with `--release`. Ratio vs Rust must be <1.5x. Do NOT proceed to next task if gate fails.
- [x] 3.4 Run struct benchmark with `--release` — struct access must improve from 10-18x to <3x vs Rust

## 4. Validation
- [x] 4.1 Run `tml test --suite=core --release` — no regressions vs debug output
- [x] 4.2 Run `tml test --suite=compiler` (debug, unchanged) — no regressions
- [x] 4.3 Verify `--release` does not affect debug build path (O0 still default)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update CHANGELOG.md and create docs/patches/v0.3.11.md — documentation covering the implementation
- [x] 5.2 Write test: `compiler/tests/release_mode_basic.test.tml` — compiles with `--release`, output is correct
- [x] 5.3 Run tests and confirm they pass
