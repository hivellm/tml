# Proposal: phase40b_release-compiler-daily

## Why
The compiler everyone (and every agent) uses daily is an **unoptimized `-O0` debug build**: `scripts/build.bat:18` defaults `BUILD_TYPE=debug`, and `compiler/CMakeLists.txt` only sets explicit `/O2`/`-O3` for the runtime targets (`tml_runtime`, `tml_json_runtime`, `tml_search_runtime` — lines 348-360, 988-998). The entire frontend/codegen (`tml_types`, `tml_codegen`, `tml_mir`, …) runs without optimization, so every `check`/`build`/`test` pays an estimated 2-3× constant-factor penalty (finding F-001, `docs/analysis/tooling-performance/02-build-performance.md`; prior benchmark `docs/analysis/benchmark/08-compilation.md`).

## What Changes
- Produce a working **release** build via `scripts/build.bat release`, fixing any release-config compile/link errors that surface.
- Verify the Release CMake config actually applies `-O2/-O3` to ALL compiler targets (not just runtimes); fix CMakeLists if any target is left unoptimized.
- A/B measure debug vs release: `tml check` on a trivial file + a representative test-suite compile; record the delta.
- Document the daily-driver workflow: which binary agents/MCP/daemon should use, and how debug vs release coexist (debug stays for C++ debugging with PDBs).

## Impact
- Affected specs: none (build tooling only)
- Affected code: `scripts/build.bat`, `compiler/CMakeLists.txt`, docs (`docs/analysis/tooling-performance/`)
- Breaking change: NO (debug build remains available; release is additive)
- User benefit: estimated 2-3× faster `check`/`build`/`test` wall-clock across the board, compounding with phase40a's warm-daemon win
