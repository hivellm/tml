# Proposal: Bootstrap Verification — Self-Hosting Achievement

## Why

Every previous compiler port phase (phase12–17b) has ported individual subsystems to TML while
the C++ compiler remained the primary tool. Phase17c is the convergence point: all subsystems are
now in TML, and we must verify that they compose correctly into a compiler that can reproduce its
own output. Bootstrap verification is the only reliable proof of self-hosting correctness. A
compiler that produces wrong output for some inputs may still pass a hand-written test suite —
but if it cannot reproduce itself, the errors will cascade and be detected. The three-stage
bootstrap (C++ → Stage 1 → Stage 2, verify Stage 1 output == Stage 2 output) is the industry
standard correctness check used by GCC, Rust, and Go.

## What Changes

No new C++ code. No new TML subsystems. This phase assembles and verifies:

- `compiler-tml/src/main.tml` — top-level entry point, wires CLI → query system → pipeline
- Stage 0: C++ `tml.exe` compiles `compiler-tml/src/main.tml` → `tml-stage1.exe`
- Stage 1: `tml-stage1.exe` compiles `compiler-tml/src/main.tml` → `tml-stage2.exe`
- Stage 2: verify Stage 1 and Stage 2 produce identical IR for all test inputs

Architecture of the bootstrapped compiler:
- `main.tml` calls `cli::dispatch(args)` from phase17b
- `cli::build` calls `query::force(CodegenUnit)` from phase17a
- `force(CodegenUnit)` triggers the full pipeline: ReadSource → Tokenize → Parse →
  Typecheck → HirLower → ThirLower → MirBuild → CodegenUnit (emits LLVM IR text)
- C++ shim invoked via `@extern("c")` for LLVM IR → .obj (LLVM backend) and .obj → .exe (LLD)
- ERA 2 will replace the C++ shim with a pure TML backend

Key decisions:
- Stage 0 (C++ `tml.exe`) is NEVER deleted — it remains as the bootstrap seed
- IR-diff (not binary diff) is the definitive correctness check — binary layout may differ due
  to link order or timestamp non-determinism, but LLVM IR must be semantically identical
- The full test suite (1,700+ tests) must pass under tml-stage2, not just selected suites
- If Stage 1 → Stage 2 produces any IR-diff, the bug is in the TML compiler source and must be
  fixed before tagging v1.0.0-self-hosted — no exceptions

## Impact

- Affected specs: `docs/ROADMAP.md` (ERA 1 complete), `docs/analyses/independence-plan/04-milestone-matrix.md` (M-10 ACHIEVED)
- Affected code: `compiler-tml/src/main.tml` (new entry point), no changes to existing C++ compiler
- Breaking change: NO — C++ compiler still available as fallback after self-hosting
- User benefit: TML becomes self-hosting — the language can bootstrap itself without external
  compilers (other than LLVM/LLD, which ERA 2 will eliminate)

## Success Criteria

- `tml-stage1.exe --version` outputs a version string (compiler is runnable)
- `tml-stage2.exe build samples/hello.tml` produces a working binary (full pipeline functional)
- IR-diff between Stage 1 and Stage 2 output is zero for ALL test files
- Full test suite (1,700+ tests) passes under tml-stage2 with zero failures
- Coverage matches C++ compiler baseline within ±1%

## This Milestone

Completion of phase17c = **ERA 1 COMPLETE**. The TML compiler no longer requires C++ to compile
TML source. ERA 2 (custom LLVM-free backend), ERA 3 (custom linker), and ERA 4 (C/C++ frontend)
build on this foundation. The C++ compiler is retained as Stage 0 but is no longer the primary
development tool.

## Dependencies

- **Requires**: phase17a complete (query system — pipeline orchestration in TML)
- **Requires**: phase17b complete (CLI/tooling — tml-stage1 must be a usable compiler)
- **Blocks**: ERA 2 (custom backend replaces C++ LLVM shim)
- **Blocks**: ERA 3 (custom linker replaces C++ LLD shim)
- **Blocks**: ERA 4 (C/C++ frontend — TML compiles C after it can compile itself)
