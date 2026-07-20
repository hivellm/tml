# 01 — The Dual-Codegen Split

## Overview

TML has two parallel code generation paths: an optimizing MIR-based pipeline and a legacy AST-based fallback. The architecture forces all programs with imports, generics, unions, or derives onto the fallback path, where the optimization passes never run.

---

### F-001 — Two full code generators; the optimizing one is bypassed for all real programs

**Impact: Very High**  
**Confidence: High**

**Measured LOC:**
- `compiler/src/codegen/llvm/` (AST-legacy) = 107 files / 75,551 LOC
- `compiler/src/codegen/mir/` = 7 files / 4,635 LOC
- `compiler/src/mir/` (builder + 30+ passes) = 78 files / 32,429 LOC
- `compiler/src/hir/` + `thir/` = 20 files / 12,614 LOC

**Routing gate:**

The decision that forces the AST path is in `compiler/src/query/query_core.cpp:800-992`. This logic forces the AST generator whenever the module has any of:

- TML imports needing codegen (lines 800–809)
- Local generic types or functions (lines 952–958)
- Reference to a built-in generic wrapper — `Maybe`, `Outcome`, `Arc`, `Mutex`, `Shared`, `Box`, `Vec`, `BTreeMap`, `HashSet`, etc. (lines 823–847)
- A union type (lines 903–907)
- An AST-only derive — `Reflect`, `Default`, `FromJson`, `ToJson` (lines 874–879)

MIR is used **only** when all of the following hold:
- `!has_tml_imports_needing_codegen && !has_local_generics` (line 992)

**In practice**, this is trivial, import-free, non-generic programs only. Corroborated by:
- `compiler/src/cli/builder/build.cpp:327,339,548` — explicit comments: "MIR codegen doesn't support imported module functions… generic type instantiation"
- ADR-009's finding that both `tml build` and the test framework route stdlib/generic programs to AST `LLVMIRGen`

**Consequence:**

Every real program (stdlib, all tests, user code with imports or generics) runs through the AST generator. The MIR pipeline is inert on production code.

---

### F-002 — The 30+ MIR optimization passes never run on real programs

**Impact: Very High**  
**Confidence: High**

`docs/ARCHITECTURE-MAP.md:83-88` lists mem2reg as "CRITICAL" and documents 30+ passes in `compiler/include/mir/passes/`.

Because F-001 routes real code around MIR, that optimization layer is dead on the hot path. Real programs get naive AST→IR plus whatever LLVM `-O` recovers later.

**The architectural contradiction:**

The project's stated goal (AGENTS.override T4) is `TML should not exceed ~2× Rust instruction count`. The machinery built to achieve this is the MIR optimizer — mem2reg elimination of temporary allocations, dead-code elimination, block merging, etc. But that machinery *doesn't run on real programs*.

This is the deepest structural conflict in the codebase. The work to make TML competitive exists, but it is located on a dead branch.

The MIR-vs-Rust IR-parity work and the `/optimize-ir` skill are effectively compensating for an optimizer that doesn't run.

---

### F-003 — Historic tests-vs-production codegen divergence

**Impact: High (mostly closed)**  
**Confidence: High**

ADR-009 (`docs/adr/ADR-009-memory-model-soundness.md:56-63`):

> "12,000+ tests green" and "UzDB corrupts in production" were simultaneously true because the two ran different codegen paths.

The dual-path design means every codegen fix is a **dual-path tax**: a bug fixed on one path may still exist on the other. Correctness on one path is not evidence for the other.

**Current status (as of 2026-07-20):**

The 2026-07-15 pivot (B1-on-AST) resolved the *divergence* by fixing the path everyone runs, but it did so by **conceding the MIR path** and deferring unification to the frozen phases 30–33. The split is now architecturally permanent until self-hosting resurrection.

---

### F-004 — `--emit-ir` fidelity depends on which path a file happens to hit

**Impact: Medium**  
**Confidence: Medium**

ADR-009:165 notes `--emit-ir` "currently shows IR from a path users run but tests don't."

**Implication for IR-quality review:**

The Rust-as-Reference methodology (AGENTS.override T4: write equivalent `.rs` and `.tml`, compare IR side-by-side) can only measure the path that actually runs. Any IR-quality review must first confirm which generator produced the IR, or it measures the wrong thing.

This is a tooling/analysis issue, not a correctness bug, but it means `/compare-ir` results are valid only if both the TML IR and the Rust IR were generated on equivalent compiler optimization levels.
