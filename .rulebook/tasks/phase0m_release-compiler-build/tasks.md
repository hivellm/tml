## 1. Diagnosis
- [ ] 1.1 Confirm current build type: check `scripts/build.bat` for `-DCMAKE_BUILD_TYPE` — verify it is `Debug` or missing
- [ ] 1.2 Check `CMakeLists.txt` for Release flags — confirm `/O2` or `-O2` is not applied in current default build
- [ ] 1.3 Measure current codegen phase timing with `tml build` internal profiling or `--verbose` output

## 2. Implementation
- [ ] 2.1 Add `--release` flag to `scripts/build.bat`: when present, pass `-DCMAKE_BUILD_TYPE=Release` to CMake
- [ ] 2.2 Verify `CMakeLists.txt` Release config: ensure it sets `/O2 /GL /LTCG` for MSVC or `-O2 -flto` for Zig CC
- [ ] 2.3 Update `plugin_loader.cpp`: when environment variable `TML_COMPILER_BUILD=release` is set (or `build/release/` exists), load DLLs from `build/release/` instead of `build/debug/`
- [ ] 2.4 Build the release compiler: run `scripts/build.bat --release` and confirm `build/release/tml_compiler.dll` is produced

## 3. Benchmark Gate
- [ ] 3.1 Run compile benchmark with release compiler DLLs: `TML_COMPILER_BUILD=release tml build math_bench.tml --stage=parser:cpp` — measure wall time
- [ ] 3.2 Compare vs debug compiler DLL wall time from `docs/analysis/benchmark/08-compilation.md`
- [ ] 3.3 GATE: Release compiler must compile the same file at least 2x faster than debug compiler. Total compile time must be <50ms. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=compiler` with release DLLs — all tests pass (same output as debug)
- [ ] 4.2 Run `tml test --suite=core` with release DLLs — no regressions
- [ ] 4.3 Confirm debug build still works unchanged (`scripts/build.bat` without `--release`)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `build: add --release flag to build.bat for optimized compiler DLLs`
- [ ] 5.2 Update `docs/analysis/benchmark/08-compilation.md` with release compiler timing
- [ ] 5.3 Run tests and confirm they pass
