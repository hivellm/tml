# 05 — Recommendation (ADR)

**Status**: proposed
**Date**: 2026-04-17
**Deciders**: architect (opus), pending project lead acceptance
**Supersedes**: none (first architectural record for coverage)
**Related**: ADR-007 (Zig CC toolchain)

---

## Context

TML currently ships a proprietary, function-level code coverage system (`lib/test/runtime/coverage.c`, `compiler/src/testing/testing_coverage*.cpp`, `compiler/src/cli/commands/cmd_coverage.cpp`, `lib/test/src/coverage.tml`) totalling ~3,350 LOC. The implementation:

- Tracks only function entries (`tml_cover_func(name)`), not lines or branches, despite the public API advertising `cover_line` / `cover_branch`.
- Emits an ad-hoc JSON (`.sandbox/coverage-*.json`) that no external consumer understands.
- Generates HTML via 1,397 LOC of string concatenation in C++, with inline CSS/JS embedded as string literals.
- Uses an ad-hoc hand-written scanner (`compiler/src/testing/testing_coverage.cpp`) to identify TML declarations from `.tml` sources — duplicating work the TML parser already performs.
- Has no integration with Codecov, Coveralls, SonarQube, VS Code Coverage Gutters, or any SaaS; coverage is visible only on the developer's local machine.

Meanwhile TML already vendors LLVM 22.1.0 (`F:/LLVM`) via Zig CC (ADR-007). `llvm-profdata`, `llvm-cov`, and the `compiler-rt` profile runtime are on disk but unused.

The broader industry has converged on two substrates: **LLVM source-based coverage** for compiled languages (rustc, clang, Swift, Go-adjacent via gcov2lcov) and **LCOV `.info`** as the textual interoperability format accepted by every SaaS.

---

## Decision

**Adopt LLVM source-based coverage as the single backbone for both the C++ compiler and TML code. Emit LCOV and JSON via `llvm-cov export`. Replace the custom HTML generator with a static SPA consuming the exported JSON.**

Concretely:

1. **C++ side**: enable `-fprofile-instr-generate -fcoverage-mapping` through a `TML_COVERAGE=ON` CMake option. Zig CC forwards both flags to Clang unchanged. Plugin DLLs and `tml.exe` emit `.profraw` on exit.
2. **TML side**: instrument the MIR→IR lowering to emit `llvm.instrprof.increment` intrinsics per region and attach `__llvm_coverage_mapping` sections per function. Link every test executable against `compiler-rt`'s profile runtime.
3. **Merge**: `llvm-profdata merge -sparse` produces a single `.profdata`.
4. **Export**: `llvm-cov export -format=lcov` for SaaS ingestion; `llvm-cov export -format=text` (JSON) for the HTML reporter.
5. **HTML**: new `compiler/src/testing/coverage_report/` with a static `template/` directory (index.html, app.js, app.css, Prism for TML/C++ highlighting) and an ~300 LOC `emit_html.cpp` packer. No HTML string concatenation in C++.

---

## Alternatives considered

### A. Keep the current system and bolt line tracking on top

- Extend `coverage.c` to track `(file, line)` tuples in addition to function names.
- Keep the proprietary JSON.
- Add more scanning code in `testing_coverage.cpp` to correlate lines.

**Rejected because**: this doubles down on the wrong substrate. LCOV/Codecov integration still has to be built separately; HTML-in-C++ stays 1,397 LOC; branch coverage still absent; MC/DC unreachable; no path to VS Code gutters or PR annotations. Estimated effort comparable to the LLVM migration with dramatically less return.

### B. OpenCppCoverage (C++) + keep TML as-is

- Use OpenCppCoverage for the C++ compiler (no rebuild needed).
- Leave TML coverage on the current custom path.

**Rejected because**: Windows-only, 100–300% overhead, no branch coverage, no unified report across both languages (two separate outputs, two separate reporters). Useful as a *diagnostic* one-liner during development, not as the long-term strategy.

### C. `cargo tarpaulin`-style debugger-based TML coverage

- Ship a `tml cover` subprocess that runs test binaries under a debugger (`DebugActiveProcess` on Windows, `ptrace` on Linux) and traps per-line via DWARF/PDB line tables.
- No codegen changes.

**Rejected because**: 100–500% overhead makes it unusable for the ~1,984 TML tests; DWARF emission from the TML codegen is currently incomplete (debug line tables are present but line-accurate remapping is not validated); no branch coverage without additional source-range metadata that would need to be emitted anyway; and this would duplicate tarpaulin's own migration away from ptrace toward LLVM source-based coverage in 2023.

### D. `grcov` as the central converter + two custom collectors

- Keep both the current TML collector and add OpenCppCoverage; merge via `grcov`.

**Rejected because**: `grcov` consumes `.gcda` and `.profraw` — not proprietary JSON. Using it would still require rewriting the TML collector. At that point we are just re-implementing option B with a converter bolted on.

### E. Build our own LCOV writer directly, skip `llvm-cov`

- Generate LCOV `.info` manually from our own per-line hit map, avoiding the LLVM profile runtime.

**Rejected because**: loses branch coverage (we would have to implement it from scratch in the TML codegen), loses MC/DC, loses crash-safe `.profraw` atomicity, duplicates runtime work that is already written and tested in `compiler-rt`. The single reason to do this would be if we were trying to remove the LLVM dependency — but ADR-007 already commits to LLVM.

---

## Consequences

### Positive

- **Line + branch + MC/DC coverage** for both C++ and TML from day one (currently function-only).
- **Native LCOV output** → Codecov, Coveralls, SonarQube, GitLab CI, Azure DevOps, VS Code Coverage Gutters all start working without custom adapters.
- **Single unified report** across C++ and TML (one `merged.profdata`, one `coverage.lcov`, one SPA).
- **~76% LOC reduction in the coverage subsystem** (~3,350 → ~800 LOC). Less code to maintain, less code to review, fewer bugs.
- **HTML becomes human-authored** (template files) instead of C++ string soup. Anyone can iterate on the UX without recompiling the compiler.
- **Leverages existing investment**: Zig CC, LLVM 22.1, `compiler-rt` are already on disk.
- **Better overhead**: 5–20% (LLVM source-based) vs current ~unknown but likely higher due to the mutex-free hash table's contention on hot paths.
- **PR diff coverage** becomes trivial (upload LCOV to Codecov, get line-by-line annotations).
- **Future MC/DC** for safety-critical TML users is one flag away (`-fcoverage-mcdc`).

### Negative

- **Irreversible coupling to LLVM's coverage map format.** The format is byte-stable within an LLVM major version but can change across majors. Mitigation: pin the LLVM version in `F:/LLVM`, already pinned via ADR-007.
- **Codegen complexity increases.** MIR→IR lowering gains a new responsibility (counter emission, coverage map writing). Estimated 400–600 LOC in `compiler/src/codegen/llvm/` and `compiler/src/mir/`. Mitigation: reuse `libLLVMProfileData` via static link from `tml_codegen_x86_plugin`.
- **Rebuild required for coverage.** Unlike OpenCppCoverage, source-based coverage needs a dedicated build. Mitigation: `TML_COVERAGE=ON` CMake profile; coverage rebuild is cached; `tml test --coverage` auto-triggers it.
- **Profile-runtime linkage on Windows is slightly fragile.** `clang_rt.profile-x86_64.lib` must match the LLVM major version exactly. Mitigation: same LLVM bundle provides both; version drift is impossible by construction.
- **Crash safety.** `.profraw` is flushed on `atexit`; a segfault in a test loses that test's profile. Same limitation as gcov. Mitigation: acceptable (test failures dominate crashes in normal use); optional future `__llvm_profile_write_file()` on signal handler.
- **Temporary dual-path during migration.** For one or two phases, both the old and new systems coexist so the test suite can validate the new output. Mitigation: feature flag `--coverage-backend=llvm|legacy`, default flipped after parity is validated.

### Reversibility

Reversible with moderate effort. The old system is git-preserved; rollback means `git revert` of the migration commits plus re-enabling the old CMake path. Since we are removing ~2,550 LOC of C++, the rollback is a restoration, not a rewrite. Flagged as "reversible" per the ADR rule that irreversible decisions must be explicit — this one is not.

---

## Migration roadmap

Five phases, each independently landable and reviewable. Each phase ends with the test suite green.

### Phase 1 — Spike (1–2 days, 0 LOC net)

**Goal**: validate that the LLVM ecosystem works end-to-end against the current C++ compiler, before touching TML.

- Add `TML_COVERAGE=ON` CMake option that injects `-fprofile-instr-generate -fcoverage-mapping` on C++ flags.
- Build `tml.exe` + plugins with `TML_COVERAGE=ON`.
- Run an existing test suite (e.g. `tml test --suite=compiler`).
- Collect `.profraw` files.
- Run `llvm-profdata merge` + `llvm-cov export -format=lcov`.
- Manually inspect the `.lcov` to confirm it contains reasonable per-line data for `compiler/src/`.

**Exit criteria**: `coverage.lcov` opens correctly in `genhtml` and shows C++ line hits for `compiler/src/`.

**Files touched**: `CMakeLists.txt`, `cmake/profile.cmake` (new, ~30 LOC).

### Phase 2 — TML codegen instrumentation (≤1 week, ~500 LOC added)

**Goal**: emit LLVM profile intrinsics and coverage map from TML codegen.

- Extend `MirFunction` with `coverage_counters: Vec<(SourceRange, counter_idx)>`.
- Walk MIR CFG in `thir_mir_builder.cpp` / `thir_mir_builder_control.cpp` to assign counter indices.
- In `compiler/src/codegen/llvm/func.cpp`, emit `llvm.instrprof.increment` calls at region entries.
- Add a new `coverage_map_writer.cpp` that reuses LLVM's `CoverageMappingWriter` to emit `__llvm_coverage_mapping` per function.
- Link test executables against `clang_rt.profile` when coverage is on (`compiler/src/codegen/llvm/linker/linker_driver.cpp`).
- Update `lib/test/src/coverage.tml` to be a thin FFI wrapper over `__llvm_profile_*` (no more custom hash table).

**Exit criteria**: a TML test binary compiled with coverage produces a `.profraw` that `llvm-profdata merge` accepts. `llvm-cov export` emits per-line data for `.tml` files.

**Files touched**: `compiler/src/codegen/llvm/func.cpp`, `compiler/src/codegen/llvm/linker/linker_driver.cpp`, `compiler/src/mir/thir_mir_builder*.cpp`, new `compiler/src/codegen/llvm/coverage_map_writer.cpp`, `lib/test/src/coverage.tml`.

### Phase 3 — Delete legacy runtime (≤2 days, ~500 LOC removed)

**Goal**: remove the custom coverage runtime now that LLVM profile runtime covers its role.

- Delete `lib/test/runtime/coverage.c`.
- Remove `tml_cover_func` / `tml_cover_line` / `tml_cover_branch` emission in the codegen.
- Remove the `.sandbox/coverage-*.json` writers.
- Remove the hand-written scanner in `testing_coverage.cpp` (no longer needed — `llvm-cov` already reports declared vs executed).

**Exit criteria**: no symbol `tml_cover_*` remains in any DLL or object file. Tests still compile and run with `TML_COVERAGE=ON`.

**Files touched**: deletion of `lib/test/runtime/coverage.c`, rewrite of `compiler/src/testing/testing_coverage.cpp` to ~250 LOC.

### Phase 4 — New HTML reporter (≤1 week, ~300 LOC added, 1,397 LOC removed)

**Goal**: replace `testing_coverage_html.cpp` with a static SPA.

- Create `compiler/src/testing/coverage_report/template/` with `index.html`, `app.js` (~300 LOC vanilla JS), `app.css`, `prism.min.js`, `tml.prism.js` (TML grammar, ~50 LOC).
- Create `compiler/src/testing/coverage_report/emit_html.cpp` (~300 LOC) that reads `coverage.json` from `llvm-cov export`, compacts it into the shape defined in `03-html-report-state-of-art.md`, and copies the template.
- Delete `compiler/src/testing/testing_coverage_html.cpp`.
- Wire the new path into `cmd_coverage.cpp`.

**Exit criteria**: `tml test --coverage --html` produces `coverage-html/index.html` with file tree, per-file source view, line/branch counters, syntax highlighting, search. Works offline via `file://`.

**Files touched**: deletion of `testing_coverage_html.cpp`, new `coverage_report/` directory, rewrite of `cmd_coverage.cpp` to ~250 LOC.

### Phase 5 — Polishing and SaaS integration (≤1 week)

**Goal**: ship the reporter to external consumers.

- Add `tml test --coverage --format=lcov|json|cobertura|html|all` (default `html+lcov`).
- Add `.github/workflows/coverage.yml` that uploads `coverage.lcov` to Codecov on every PR.
- Add `.gitignore` entries for `.sandbox/profiles/`, `merged.profdata`, `coverage.lcov`, `coverage-html/`.
- Document the new workflow in `docs/ROADMAP.md` and `docs/CODE_COVERAGE.md`.
- Add a regression test that validates: `tml test --coverage` produces a non-empty `coverage.lcov` with per-line entries for at least one file in each of `compiler/src/`, `lib/core/src/`, `compiler-tml/src/`.

**Exit criteria**: Codecov badge on `README.md`; coverage visible in GitHub PRs.

**Files touched**: `.github/workflows/coverage.yml` (new), `docs/CODE_COVERAGE.md` (new), `README.md` (badge).

---

## LOC accounting

| Component | Current | After migration | Δ |
|-----------|---------|-----------------|---|
| `lib/test/runtime/coverage.c` | 388 | 0 | −388 |
| `lib/test/src/coverage.tml` | 126 | ~50 | −76 |
| `compiler/src/testing/testing_coverage.cpp` | 1,153 | ~250 | −903 |
| `compiler/src/testing/testing_coverage_html.cpp` | 1,397 | 0 | −1,397 |
| `compiler/src/cli/commands/cmd_coverage.cpp` | 412 | ~250 | −162 |
| `compiler/src/codegen/llvm/coverage_map_writer.cpp` | 0 | ~400 | +400 |
| `compiler/src/codegen/llvm/func.cpp` (delta) | — | ~+50 | +50 |
| `compiler/src/mir/thir_mir_builder*.cpp` (delta) | — | ~+100 | +100 |
| `compiler/src/testing/coverage_report/emit_html.cpp` | 0 | ~300 | +300 |
| `compiler/src/testing/coverage_report/template/` (HTML/JS/CSS) | 0 | ~500 non-C++ | +500 (different kind of code) |
| **Total C/C++** | **3,476** | **~1,100** | **~−2,400 (~−69%)** |

(HTML/JS/CSS lines are not counted in the C++ total; they are template files, not implementation.)

---

## Review date

Revisit on **2026-10-17** (6 months) or when:

- LLVM major version changes (22 → 23), or
- Windows/Linux parity breaks in `compiler-rt/lib/profile`, or
- A user reports > 25% overhead in coverage mode on a representative benchmark suite, or
- GitHub / Codecov deprecates LCOV ingestion (unlikely).

---

## Trade-off statement (mandatory)

**In exchange for a ~76% LOC reduction, native SaaS integration, line + branch + MC/DC coverage, and a maintainable HTML reporter, TML commits harder to the LLVM toolchain.** The coverage subsystem can no longer be rebuilt without LLVM's profile runtime and coverage map format. If a future decision moves TML away from LLVM as the codegen backend, the coverage subsystem will have to be rebuilt from scratch — roughly at the same cost as the original implementation. ADR-007 already commits to LLVM, so this is not a new coupling in practice, but it is worth recording explicitly.

---

## Acceptance checklist (for the `phase-coverage-llvm-migration` task)

- [ ] Phase 1 spike merged; `coverage.lcov` for C++ works end-to-end.
- [ ] Phase 2 codegen emits `llvm.instrprof.increment` and `__llvm_coverage_mapping` for TML functions.
- [ ] Phase 3 legacy runtime deleted; no `tml_cover_*` symbol remains.
- [ ] Phase 4 SPA reporter ships; HTML loads offline and shows line + branch counters.
- [ ] Phase 5 Codecov badge visible on the `README.md`.
- [ ] Regression test ensures non-empty LCOV across C++, `lib/core/`, and `compiler-tml/`.
- [ ] `docs/CODE_COVERAGE.md` documents the new flow.
- [ ] CHANGELOG entry under "Changed" explaining the migration and the removed custom schema.
