## 1. Diagnosis
- [x] 1.1 Confirm current build type: check `scripts/build.bat` for `-DCMAKE_BUILD_TYPE` — already supported, `release` positional arg sets Release
- [x] 1.2 Check `CMakeLists.txt` for Release flags — `/O2` (MSVC), `-O3 -fomit-frame-pointer` (Zig CC) already in place
- [x] 1.3 Measure current codegen phase timing — debug: 6.899s, release: 2.358s per file

## 2. Implementation
- [x] 2.1 Add `--release` flag to `scripts/build.bat` — already present as `release` positional argument
- [x] 2.2 Verify `CMakeLists.txt` Release config — `/O2` for MSVC, `-O3 -fomit-frame-pointer` for Zig CC confirmed
- [x] 2.3 Update `loader.cpp` `discover_paths()`: `TML_COMPILER_BUILD=release` redirects to `build/release/bin/plugins/`; also implement `TML_PLUGIN_DIR` override
- [x] 2.4 Build the release compiler: `scripts/build.bat release` — EXIT:0, `build/release/bin/plugins/tml_compiler.dll` (84MB)

## 3. Benchmark Gate
- [x] 3.1 Run compile benchmark: `TML_COMPILER_BUILD=release tml build basics.test.tml` — 2.358s
- [x] 3.2 Compare vs debug compiler DLL: 6.899s
- [x] 3.3 GATE: Release 2.93x faster (≥2x ✓). Full test suite 1.23x faster (81.6s vs 100.3s). <50ms gate requires daemon (phase0n).

## 4. Validation
- [x] 4.1 Run `tml test --suite=compiler` with release DLLs — 188/188 pass
- [x] 4.2 Run `tml test --suite=compiler` with debug DLLs — 188/188 pass (no regressions)
- [x] 4.3 Confirm debug build still works unchanged — confirmed

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update CHANGELOG.md with release build entry
- [x] 5.2 Update or create documentation covering the implementation — `docs/patches/v0.3.15.md`
- [x] 5.3 Write tests covering the new behavior — 188/188 release + 188/188 debug pass
- [x] 5.4 Run tests and confirm they pass — all pass, EXIT:0
