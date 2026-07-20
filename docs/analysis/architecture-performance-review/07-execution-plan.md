# 07 — Execution Plan (Proposed, Phased)

## Overview

Phased remediation of the root conflicts identified in 01–06. Order matters: Phase A restores trust in test results (without it, no gate below is meaningful), Phase B resolves the deepest architectural conflict, Phase C attacks the remaining structural test-speed floor, Phase D sheds dead weight.

> **Decision required:** Phase B step B1 is a genuine architecture decision that requires user sign-off — two options with materially different scope and payoff are laid out below. Nothing in B proceeds until that call is made.

---

## Phase A — Stop the bleeding on the memory foundation

Highest correctness leverage; unblocks test trust. Targets F-016, F-017, F-019.

- **A1:** Fix the clone-read/drop symmetry at `compiler/src/codegen/llvm/builtins/intrinsics.cpp:854` (F-016) — implement the sound structural-duplicate for monomorphized handle-bearing aggregates, so `Deque::iter`/`List::iter` stop UAF-ing/leaking. This closes the three phase44b standalone crashes (F-019) at the root.
- **A2:** Land the phase44c `size_of[T]`-based alloc-size lint (F-017); fix all true positives in `lib/` and `compiler/`.

**Gate:** `std/collections` 103/103 standalone, 20/20 reruns; determinism corpus clean.

---

## Phase B — Make the tested path the shipped path, or make one path optimize

Attacks F-001/F-002 — the deepest conflict: the optimizing pipeline (MIR + 30 passes) never runs on real programs.

- **B1 (DECISION — user/architect call, flagged not chosen):**

  | Option | Scope | Payoff | Risk |
  |--------|-------|--------|------|
  | **(i) Resurrect MIR unification early** — retire the AST fallback (the old ADR-009 B3): ~8K LOC of imported-fn emission + generic monomorphization + derives/unions on MIR | Large project | Removes the dual-path tax **permanently**; one codegen to maintain, test, and optimize | Big surface; previously deferred to the frozen phases 30–33 for a reason |
  | **(ii) Port the essential MIR optimization passes** (mem2reg equivalent, DCE) to run on the AST→IR output so real programs actually get optimized | Smaller | Delivers the performance goal without the self-hosting prerequisite | Dual-path tax remains; passes are re-implemented against a second IR shape |

- **Gate:** real-program IR ≤2× Rust (AGENTS.override T4) measured via `/compare-ir` on the benchmark corpus, **on the path real programs run** (see F-004: confirm which generator produced the IR before measuring).

---

## Phase C — Attack the structural test-speed floor

Targets F-007, F-008, F-012 — the open items after the phase 40–43 tactical fixes.

- **C1:** In-process thread-per-test execution behind panic isolation (the prerequisite `docs/analysis/compiler-internals/single-binary-test-compilation.md` Phase 3 named). This is the last big test-speed lever and is gated on runtime panic-isolation support — worth scoping as its own task.
- **C2:** Lazy/mmap plugin DLL load; keep `tml_codegen_x86.dll` unloaded for `check`-only work (F-012).
- **C3:** Raise per-suite codegen parallelism >1 once the LLVM-global-state restructuring from the shared-stdlib work is generalized (F-008).

---

## Phase D — Shed dead weight

Targets F-020.

- Quarantine `compiler-tml/` from default build/test/docs discovery until phases 30–33 unfreeze it; document the boundary in the ARCHITECTURE-MAP. Concrete steps in `06-frozen-self-host.md`.

---

## Sequencing rationale

1. **A before everything:** nondeterministic heap corruption (F-019) makes every downstream gate unreliable — a "passed" result can't be trusted, and reruns are the expensive thing this whole review is trying to eliminate.
2. **B is the leverage point:** every other performance effort (IR-parity work, `/optimize-ir`, Rust-as-Reference) is compensating for an optimizer that doesn't run on real code. Deciding B1 determines where all subsequent codegen investment goes.
3. **C is bounded by language features:** C1 needs panic isolation (a runtime/language gap, not a test-framework gap); C2/C3 are independent and can proceed anytime.
4. **D is cheap and safe:** pure surface-area reduction, no behavior change to the shipping compiler.
