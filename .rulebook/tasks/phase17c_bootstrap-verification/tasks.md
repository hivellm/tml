# Tasks: Bootstrap Verification — Self-Hosting Achievement

**Status**: Planned (0/16)
**Depends on**: phase17a (query system), phase17b (CLI/tooling)
**Blocks**: ERA 2 (custom backend), ERA 3 (custom linker), ERA 4 (C/C++ frontend)
**Duration**: 3–4 weeks
**Risk**: Medium — verification is well-defined, but first bootstrap attempt may reveal bugs
**This is the FINAL task of ERA 1. Completing this = TML COMPILES ITSELF.**

---

## Phase 1: Stage 1 — First TML-Compiled Compiler (4 items)

- [ ] 1.1 Prepare TML compiler source tree: all phase12-17 TML code in `compiler-tml/src/`
- [ ] 1.2 Build Stage 1: `tml.exe build compiler-tml/src/main.tml -o tml-stage1.exe` (C++ compiler builds TML compiler)
- [ ] 1.3 Verify Stage 1 runs: `tml-stage1.exe --version` outputs version string
- [ ] 1.4 Smoke test: `tml-stage1.exe build samples/hello.tml` produces working binary

## Phase 2: Stage 2 — Self-Compiled Compiler (4 items)

- [ ] 2.1 Build Stage 2: `tml-stage1.exe build compiler-tml/src/main.tml -o tml-stage2.exe` (TML compiler compiles itself)
- [ ] 2.2 Verify Stage 2 runs: `tml-stage2.exe --version` outputs same version string
- [ ] 2.3 Smoke test: `tml-stage2.exe build samples/hello.tml` produces working binary
- [ ] 2.4 Compare: `tml-stage1.exe` binary hash vs `tml-stage2.exe` binary hash (may differ due to non-determinism)

## Phase 3: Output Verification (4 items)

- [ ] 3.1 IR-diff: compile ALL test files with tml-stage1 → collect IR; compile ALL with tml-stage2 → collect IR; diff must be zero
- [ ] 3.2 Run full test suite (1,700+ tests) with tml-stage2 → all must pass
- [ ] 3.3 Run coverage with tml-stage2 → coverage must match C++ compiler baseline (±1%)
- [ ] 3.4 If any IR-diff or test failure: debug, fix in TML compiler source, rebuild from Stage 0, re-verify

## Phase 4: Finalization (4 items)

- [ ] 4.1 Tag git commit: `v1.0.0-self-hosted` — first self-hosting milestone
- [ ] 4.2 Update ROADMAP.md: ERA 1 complete, record actual timeline vs estimate
- [ ] 4.3 Update docs/analyses/independence-plan/04-milestone-matrix.md: mark M-10 ACHIEVED
- [ ] 4.4 Archive phase12-17 tasks as complete
