# 04 — Hybrid C++ + TML coverage architecture

**Goal**: design a single pipeline that measures both sides of TML (the ~700-file C++ compiler and the ~858 TML sources + 1,984 TML tests) in one unified report. Rely on the LLVM toolchain that is already vendored at `F:/LLVM` (ADR-007 Zig CC).

---

## Constraints recap

- **C++ compiler**: ~700 files, 100k+ LOC, 5 plugin DLLs, built through Zig CC (Clang 20) + LLD.
- **TML source/tests**: compiled by the TML compiler to LLVM IR → `.obj` → `.exe` via `lld-link`. Each `.tml` becomes a function in IR; each test becomes a separate `.exe`.
- **LLVM 22.1.0** available in `F:/LLVM` with `llvm-profdata`, `llvm-cov`, and the `compiler-rt` profile runtime.
- **Target OS**: Windows primary, Linux/macOS secondary (self-hosted compiler phases).
- **Constraint**: minimise new C/C++ code (AGENTS.override.md §T4). Reuse LLVM binaries for merging and export.

---

## Comparison: C++ instrumentation options

| Dimension | OpenCppCoverage | `zig cc -fprofile-instr-generate` (LLVM source-based) |
|-----------|------------------|-------------------------------------------------------|
| Rebuild required | No (uses PDB) | Yes |
| Overhead | 100–300% | 5–20% |
| Branch coverage | No | Yes |
| MC/DC | No | Yes (since LLVM 18) |
| Cross-platform | Windows-only | Linux + macOS + Windows |
| Output format | Cobertura XML | `.profraw` → LCOV / JSON via `llvm-cov` |
| Integration with LLVM toolchain | Stand-alone (no profdata reuse) | Native |
| Future-proof | Declining | Growing |
| Setup cost | `pip install OpenCppCoverage`, point-and-shoot | Tweak CMake/Zig build flags |

**Decision**: **LLVM source-based via Zig CC**. OpenCppCoverage remains acceptable as a no-rebuild diagnostic tool for quick sanity checks, but the canonical pipeline must be LLVM-based.

Zig CC is already the toolchain (`cmake/toolchains/zig.cmake`), and it forwards `-fprofile-instr-generate -fcoverage-mapping` unchanged to Clang. The only real work is enabling them in a `coverage` CMake profile.

---

## Comparison: TML instrumentation options

| Approach | Pros | Cons |
|----------|------|------|
| **Keep current** (`tml_cover_func` runtime hash table) | Works today | Function-level only; ad-hoc JSON; no ecosystem; 388 LOC of C runtime; ~1,500 LOC of C++ coordinator |
| **Emit `llvm.instrprof.increment` intrinsics in MIR→IR** | Native LLVM; reuses `compiler-rt` profile runtime; free LCOV export; line + branch + MC/DC | Requires codegen changes in `compiler/src/codegen/llvm/` and `compiler/src/mir/thir_mir_builder*.cpp`; compiler must emit `__llvm_coverage_mapping` section per function |
| **IR-level `-passes=pgo-instr-gen`** after TML lowers to IR | Minimal codegen changes | Generates function-level counters only; cannot see TML source regions (the coverage map needs TML source ranges, not IR); defeats the purpose |

**Decision**: emit the intrinsics directly in the MIR→IR builder, with per-region counters driven from TML AST/MIR source locations.

This is the same approach rustc takes (`rustc -Cinstrument-coverage` uses MIR-level region IDs mapped to source spans, then lowers them into `llvm.instrprof.increment`). The TML self-hosted compiler already tracks source locations through `SourceLoc` in AST/MIR; the piece to add is a "counter table" per function plus the intrinsic calls at region entry points.

### Intrinsic contract (LLVM 22)

```llvm
declare void @llvm.instrprof.increment(i8* <hash_ptr>, i64 <func_hash>, i32 <num_counters>, i32 <counter_idx>)
```

For each TML function, the compiler assigns a stable 64-bit hash (CRC64 of mangled name is standard) and a counter table sized to the number of regions. At each region entry, the IR emits a call to the intrinsic with the region's index. LLVM's back-end lowers these into increments of a profile section and emits the coverage map (`__llvm_coverage_mapping`) that encodes `(func_hash, source_ranges[], counter_expressions[])`.

The coverage map format is documented in `llvm/lib/ProfileData/Coverage/CoverageMappingReader.cpp`. It is byte-stable across LLVM versions within the same major release; changes require updating the reader in lock-step with the LLVM bundle.

---

## Pipeline diagram

```
┌──────────────────────┐          ┌──────────────────────┐
│ compiler/*.cpp       │          │ lib/**.tml,          │
│ (700 files, 100k LOC)│          │ compiler-tml/**.tml  │
└──────────┬───────────┘          └──────────┬───────────┘
           │                                 │
           │ zig cc -fprofile-instr-generate │ tml build --coverage
           │        -fcoverage-mapping       │ (MIR→IR emits
           ▼                                 │  llvm.instrprof.increment
┌──────────────────────┐                     │  + __llvm_coverage_mapping)
│ tml.exe              │                     ▼
│ tml_compiler.dll     │          ┌──────────────────────┐
│ tml_codegen_x86.dll  │          │ *.test.exe           │
│ ...                  │          │ (one per test file)  │
└──────────┬───────────┘          └──────────┬───────────┘
           │ run test suite                  │ run all tests
           │ (LLVM_PROFILE_FILE=tml-%p.profraw)         (same env var)
           ▼                                 ▼
        ┌─────────────────────────────────────────┐
        │ .sandbox/profiles/*.profraw             │
        │  (one file per process, atomic writes)  │
        └──────────────────┬──────────────────────┘
                           │ llvm-profdata merge -sparse
                           ▼
                ┌────────────────────────┐
                │ merged.profdata        │
                │ (indexed binary)       │
                └───────────┬────────────┘
                            │
        ┌───────────────────┼────────────────────┐
        │                   │                    │
        ▼                   ▼                    ▼
  llvm-cov export    llvm-cov export       llvm-cov show
  -format=lcov       -format=text          --format=html
    |                    |                     | (fallback only)
  coverage.lcov    coverage.json          coverage-llvm-html/
  (SaaS ingest)    (SPA input)            (debug / parity)
                         │
                         ▼
              ┌────────────────────────┐
              │ emit_html.cpp          │
              │ (≈300 LOC)             │
              │ JSON → SPA packer      │
              └───────────┬────────────┘
                          ▼
              ┌────────────────────────┐
              │ coverage-html/         │
              │   index.html           │
              │   app.js, app.css      │
              │   coverage.json        │
              │   prism.min.js/.css    │
              └────────────────────────┘
```

---

## Key pipeline details

### 1. C++ instrumentation build profile

Add a CMake option `TML_COVERAGE=ON` that toggles:

```cmake
if (TML_COVERAGE)
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
endif()
```

Zig CC passes both flags through to Clang unchanged. The resulting `tml.exe` + plugin DLLs write `.profraw` files at process exit (or on `__llvm_profile_write_file()` calls).

### 2. TML instrumentation

Three codegen changes, all inside `compiler/src/codegen/llvm/` and `compiler/src/mir/`:

1. **Counter assignment**: when lowering a `MirFunction`, walk the MIR CFG and assign a counter index to every region (entry, each branch target, each loop header). The assignment is deterministic and stored in `MirFunction::coverage_counters`.
2. **Intrinsic emission**: at the start of each counter's region, emit a call to `@llvm.instrprof.increment(i8* @<func_hash_var>, i64 <func_hash>, i32 <num_counters>, i32 <counter_idx>)`.
3. **Coverage map section**: after the function is emitted, attach the encoded coverage map to the `__llvm_coverage_mapping` global. The encoding is documented in `CoverageMapping.h`; TML just needs to reuse the `CoverageMappingWriter` API from LLVM's own `ProfileData` library (available in the same LLVM bundle).

Reuse the LLVM writer means no byte-format maintenance on the TML side — we get MC/DC and branch coverage for free when LLVM advances.

### 3. Runtime

**Delete** `lib/test/runtime/coverage.c` (388 LOC). Replace with linking against `compiler-rt/lib/profile` (also shipped in the LLVM bundle). Every TML test executable now links the profile runtime just like a C++ program compiled with `-fprofile-instr-generate`.

The CLI still sets `LLVM_PROFILE_FILE=.sandbox/profiles/%m-%p.profraw` before spawning each test. `%m` expands to a per-binary hash, `%p` to the PID — guaranteed unique.

### 4. Merge

```bash
llvm-profdata merge -sparse .sandbox/profiles/*.profraw -o .sandbox/merged.profdata
```

`llvm-profdata` handles all edge cases (partial writes, concurrent processes, version skew). No code on TML's side.

### 5. Export

```bash
llvm-cov export -format=lcov     -instr-profile=.sandbox/merged.profdata <binaries> > coverage.lcov
llvm-cov export -format=text     -instr-profile=.sandbox/merged.profdata <binaries> > coverage.json
```

`<binaries>` is the list of everything that might have contributed to the profile: `tml.exe`, all plugin DLLs, and each `.test.exe`. `llvm-cov` walks their `__llvm_coverage_mapping` sections and produces the unified output.

### 6. Report generation

`emit_html.cpp` (new, ~300 LOC) reads `coverage.json`, compacts it into the shape described in `03-html-report-state-of-art.md`, and copies the static template. **No string-concatenation HTML in C++.**

---

## Unified report: how both worlds merge

The binary and the intermediate format are the same for both C++ and TML — both emit `__llvm_coverage_mapping` sections, both produce `.profraw` files, both are merged into the same `.profdata`. From `llvm-cov`'s perspective, TML is just another language target.

The resulting `coverage.lcov` / `coverage.json` contains entries for:
- `compiler/src/**/*.cpp` (C++)
- `compiler/include/**/*.hpp` (C++)
- `lib/core/src/**/*.tml` (TML)
- `lib/std/src/**/*.tml` (TML)
- `compiler-tml/src/**/*.tml` (TML)

Paths are relative to the project root (via `llvm-cov --compilation-dir=<root>`). The SPA groups files by top-level directory for the tree sidebar.

**Caveat**: `llvm-cov` needs debug info (`-g`) to resolve source ranges in the coverage map. Both Zig CC and the TML codegen already emit DWARF/PDB for debug builds, so this is free in the coverage CMake profile.

---

## CLI changes

`tml test --coverage` becomes a thin orchestrator:

```
1. Ensure --coverage rebuild flags on (CMake + TML codegen).
2. Clear .sandbox/profiles/.
3. Run test suites; each subprocess sets LLVM_PROFILE_FILE.
4. llvm-profdata merge -sparse .sandbox/profiles/*.profraw -o merged.profdata
5. llvm-cov export (lcov + json)
6. emit_html: pack SPA.
7. Print summary from LCOV.
```

All seven steps live in `cmd_coverage.cpp`, replacing the current 412 LOC with roughly ~250 LOC (simpler: no regex scanner, no hash table walking, no JSON merging, no HTML emission).

---

## Cross-platform concerns

- **Windows**: `llvm-profdata.exe` and `llvm-cov.exe` ship in `F:/LLVM/bin`. `compiler-rt` profile runtime is in `F:/LLVM/lib/clang/<ver>/lib/windows/clang_rt.profile-x86_64.lib`. LLD links it when `-fprofile-instr-generate` is on the link line.
- **Linux**: same tools under `<llvm-root>/bin`, runtime under `lib/clang/<ver>/lib/linux/libclang_rt.profile-x86_64.a`.
- **macOS**: same under `lib/darwin/`. Requires `xcrun --show-sdk-path` for the SDK, not a TML concern.

The TML build must link `clang_rt.profile` explicitly for test executables — today the TML linker invocation in `compiler/src/codegen/llvm/linker/` needs a `--coverage` branch that adds the runtime lib.

---

## What to keep from the current implementation

Very little.

| Current file | Fate |
|--------------|------|
| `lib/test/runtime/coverage.c` (388 LOC) | **Delete.** Replaced by `compiler-rt/lib/profile`. |
| `lib/test/src/coverage.tml` (126 LOC) | **Rewrite as thin FFI wrapper.** Keep the public TML API surface (`coverage::report()`, `coverage::is_func_covered()`) but back it with `__llvm_profile_*` calls. |
| `compiler/src/testing/testing_coverage.cpp` (1,153 LOC) | **Delete ~900 LOC.** Keep only: module-discovery + the summary printer. Everything else (scanner, aggregation, JSON merge) goes. Net ~250 LOC. |
| `compiler/src/testing/testing_coverage_html.cpp` (1,397 LOC) | **Delete in full.** Replace with `coverage_report/emit_html.cpp` (~300 LOC) + static `template/` directory. |
| `compiler/src/cli/commands/cmd_coverage.cpp` (412 LOC) | **Rewrite ~150 LOC.** Becomes a subprocess orchestrator. |

**Before**: 3,350 LOC (of which ~2,550 LOC is C++, ~388 LOC C, ~126 LOC TML, ~286 LOC remaining).
**After**: ~800 LOC total (~150 orchestrator + ~250 discovery/summary + ~300 html packer + ~100 TML wrapper + ~0 C runtime).

---

## Open questions (for the migration task)

1. **Coverage map writer reuse**: can TML link against LLVM's `libLLVMProfileData` directly from the plugin DLL, or does it need to re-implement `CoverageMappingWriter`? Spike required. Preferred answer: link the LLVM static lib — it is already linked in `tml_codegen_x86_plugin`.
2. **Test isolation**: does the profile runtime flush `.profraw` on `_exit()` / crash? `compiler-rt` uses `atexit`, so segfaults lose data. This is the same limitation gcov has — acceptable for now.
3. **Parallel test runs**: multiple `.test.exe` writing simultaneously to the same directory. `LLVM_PROFILE_FILE=%p` (PID) guarantees filename uniqueness. Verified in LLVM docs; test under the rulebook test harness.
4. **Path normalisation**: `llvm-cov --compilation-dir=<project_root>` plus `-path-equivalence=old=new` to handle Windows backslashes → forward slashes. Standardise on forward-slash paths in the emitted JSON.
5. **Source availability**: the SPA needs the TML source text for the file view. Either inline into `coverage.json` (small enough for TML size) or fetch on demand via `fetch(path)` if the HTML is served over HTTP. Inline is the zero-config default.

Continues in [`05-recommendation.md`](./05-recommendation.md).
