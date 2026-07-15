# Proposal: phase33a_native-default-backend-switch (renumbered from phase34a, 2026-07-15 ERA 0 pivot)

## Why
The native backend is now complete enough (phases 33a–33e) to serve as the default code generator on Windows x86-64, which is TML's primary development platform. Keeping the legacy LLVM-via-C++ path as the default means users never exercise the native backend in ordinary workflows, so regressions go undetected until late. Switching the default on the `x86_64-pc-windows-msvc` and `x86_64-pc-windows-gnu` triples aligns the default with the backend that will receive active development, while retaining `--backend=llvm` as an explicit opt-in for the legacy path.

## What Changes
- CLI target-triple detection: at startup, query the host triple via an existing `target_triple()` helper; if the triple starts with `x86_64` and the OS component is `windows`, set the default backend to `native`.
- `BuildOptions` and `RunOptions` default field: change `backend` default from `"llvm"` to `""` (empty = auto-detect) and resolve the empty string to `"native"` on x86-64 Windows or `"llvm"` on all other platforms.
- Help text: update `--backend` flag description to document the auto-detect behavior, list `native` and `llvm` as valid values, and note which platforms get which default.
- Documentation: update `docs/native-backend.md` and the CLI reference section of `docs/compiler.md` to describe the new default and the `--backend=llvm` fallback flag.
- Test: add a CLI integration test that invokes `tml build` on x86-64 Windows without `--backend` and asserts the emitted object file is produced by the native backend (check for a native-backend-specific IR comment or object section name).

## Impact
- Affected specs: CLI option parsing (`compiler/src/cmd_build.cpp`, `compiler/src/cmd_run.cpp`)
- Affected code: `compiler/src/cmd_build.cpp`, `compiler/src/cmd_run.cpp`, `docs/native-backend.md`, `docs/compiler.md`
- Breaking change: YES — existing scripts that rely on the LLVM backend being the default on Windows x86-64 must add `--backend=llvm` explicitly
- User benefit: Ordinary `tml build` and `tml run` commands exercise the native backend automatically on the primary development platform, catching regressions early and delivering the faster compile times the native path provides.
