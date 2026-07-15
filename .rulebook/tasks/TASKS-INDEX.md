# TML Project — Task Index

**Last updated**: 2026-07-15
**Strategic direction**: Stabilization first. Self-hosting and feature phases are
**frozen by explicit user decision** until the language foundation is sound.
Rationale + evidence: [`docs/analysis/tml-table-analysis/`](../../docs/analysis/tml-table-analysis/README.md).

---

## ERA 0 — STABILIZATION (ACTIVE — execute in this order)

Corrective roadmap from `docs/analysis/tml-table-analysis/06-execution-plan.md`.
Nothing below ERA 0 starts until its gates are green.

### Phase 25 — Measure honestly (Plan Phase A)

| ID | Task | Status | Priority | Depends on |
|----|------|--------|----------|------------|
| 25a | [Determinism Harness](phase25a_stab-determinism-harness/) | Pending | **P0** | — |
| 25b | [LLVM Verifier as Hard Error in CI](phase25b_stab-llvm-verifier-ci/) | Pending | **P0** | — (parallel with 25a) |

### Phase 26 — Memory-model soundness (Plan Phase B — THE milestone)

| ID | Task | Status | Priority | Depends on |
|----|------|--------|----------|------------|
| 26a | [ADR-009: Memory-Model Decision (B1 drop-flags vs B2 ARC)](phase26a_memmodel-adr-decision/) | Pending | **P0** | 25a · **user sign-off required** |
| 26b | [Memory-Model Implementation](phase26b_memmodel-implementation/) | Pending | **P0** | 26a |
| 26c | [Revert phase24 Band-Aids + Close Bug Class](phase26c_memmodel-bandaid-revert/) | Pending | **P0** | 26b |

**Gate:** `essential.c --emit=ast` ×100 = 100/100 under adversarial allocator, with ALL
phase24c–24n workarounds reverted.

### Phase 27 — Codegen stability (Plan Phase C)

| ID | Task | Status | Priority | Depends on |
|----|------|--------|----------|------------|
| 27a | [K001 Root-Cause Sweep (mangling/monomorphization)](phase27a_codegen-k001-root-cause/) | Pending | **P0** | 25b |
| 27b | [X002 Hangs / X003 Crashes on Core Features](phase27b_codegen-x002-x003-hangs/) | Pending | **P0** | 25a |

**Gate:** full compiler suite green ×100 consecutive runs; zero non-deterministic failures.

### Phase 28 — Prove it with a real app (Plan Phase D)

| ID | Task | Status | Priority | Depends on |
|----|------|--------|----------|------------|
| 28a | [UzDB-Core Acceptance App + Soak Test](phase28a_uzdb-core-acceptance/) | Pending | **P0** | 26c, 27a, 27b |
| 28b | [Async File I/O Hardening](phase28b_async-file-hardening/) | Pending | P1 | 28a |

**Gate:** soak binary (millions of ops) ×100 = 100/100, leak-check clean. This gate
re-earns the use case that was lost when UzDB was abandoned.

---

## SUPERSEDED — phase24 band-aid line (do not resume)

These tasks fought the memory-model bug class per-site. The approach is proven
non-convergent (phase24l Attempt log); the class is closed at the root by Phase 26.
Kept for their attempt logs and repro fixtures, which phase26 reuses.

| Task | Disposition |
|------|-------------|
| [phase0z_cc-driver-essential-c-residual](phase0z_cc-driver-essential-c-residual/) | Residual `essential.c` gate absorbed into 26b/26c gates |
| [phase24i_cc-variadic-macro-paste](phase24i_cc-variadic-macro-paste/) | Shipped in 0.3.50; repros feed the 25a corpus |
| [phase24j_cc-blockitem-shared-migration](phase24j_cc-blockitem-shared-migration/) | Shipped in 0.3.51; `.duplicate()` sites reverted in 26c |
| [phase24k_essential-cleanup-segv](phase24k_essential-cleanup-segv/) | Diagnosis reused by ADR-009 (26a) |
| [phase24l_shared-get-aliasing-deep-fix](phase24l_shared-get-aliasing-deep-fix/) | Attempt log is key evidence for 26a |
| [phase24m_essential-c-residual-segv](phase24m_essential-c-residual-segv/) | Absorbed into 26b gates |
| [phase24n_cc-preproc-aliasing-sweep](phase24n_cc-preproc-aliasing-sweep/) | Shipped in 0.3.52 (`ptr_read_clone`); superseded by 26b, folded in 26c item 1.3 |

---

## FROZEN — Self-hosting / native backend (ERA 1) — resumes after ERA 0

**Frozen by strategic decision (2026-07-15):** the self-hosted compiler is TML's own
memory model at application scale; it cannot ship on an unsound foundation. The
`feat/self-hosting-compiler` branch merges to main as-is (not finished), and work
resumes only after the Phase 26–27 gates. See README "Project status".

| ID | Task | Progress before freeze |
|----|------|------------------------|
| 31a–d | [native-builtins-io-string-sync](phase31a_native-builtins-io-string-sync/) / [simd-avx](phase31b_native-builtins-simd-avx/) / [reflect-dyncall](phase31c_native-builtins-reflect-dyncall/) / [runtime-modules](phase31d_native-runtime-modules/) | 0% each |
| 32a–c | [async-await](phase32a_native-async-await/) / [class-oop](phase32b_native-class-oop-codegen/) / [dyn-dispatch-vtable](phase32c_native-dyn-dispatch-vtable/) | 0% each |
| 33a,c–e | [optimization-passes](phase33a_native-optimization-passes/) / [collections-codegen](phase33c_native-collections-codegen/) / [cast-conversion](phase33d_native-cast-conversion-complete/) / [try-operator](phase33e_native-try-operator-complete/) | 0% each |
| 34a–b | [default-backend-switch](phase34a_native-default-backend-switch/) / [full-test-suite-pass](phase34b_native-full-test-suite-pass/) | 0% each |
| 23c | [cpp-subset-frontend](phase23c_cpp-subset-frontend/) | 0% |

---

## FROZEN — Feature phases (ERA 2) — resumes after ERA 0 + unfreeze decision

**Frozen by strategic decision (2026-07-15):** every one of these stores user data in
collections and would inherit the memory-model bug class (analysis F-014). Each
resumes with a mandatory soak-test gate in its definition-of-done.

| ID | Task | Area |
|----|------|------|
| 35a–d | [db-mongodb](phase35a_db-mongodb/) / [db-redis-mysql](phase35b_db-redis-mysql/) / [db-perf](phase35c_db-perf-optimization/) / [db-typeorm-parity](phase35d_db-typeorm-parity/) | Databases |
| 36a–f | [ia-cuda](phase36a_ia-cuda/) / [model-loading](phase36b_ia-model-loading/) / [inference](phase36c_ia-inference/) / [lora-finetune](phase36d_ia-lora-finetune/) / [distributed](phase36e_ia-distributed/) / [bench-serve](phase36f_ia-bench-serve/) | AI / ML |
| 37a–c | [http-performance](phase37a_http-performance/) / [http-benchmark](phase37b_http-benchmark/) / [db-http-integration](phase37c_db-http-integration/) | HTTP |
| 38a–f | [package-manager](phase38a_package-manager/) / [package-manager-alt](phase38b_package-manager-alt/) / [auto-parallel](phase38c_auto-parallel/) / [cross-compilation](phase38d_cross-compilation/) / [self-hosting-legacy](phase38e_self-hosting-legacy/) / [cranelift-backend](phase38f_cranelift-backend/) | Toolchain |
| 39a | [iouring-http-backend](phase39a_iouring-http-backend/) | HTTP |

**Exception:** `phase38a_package-manager` (registry) is the FIRST task to unfreeze
after ERA 0 (Plan Phase E1) — it does not depend on the memory model and unblocks
external adopters.

---

## Roadmap summary

```
ERA 0 (ACTIVE):  25a,25b → 26a → 26b → 26c → 27a,27b → 28a → 28b   (stabilization)
SUPERSEDED:      phase0z, phase24i–n (band-aid line — evidence + repros only)
FROZEN ERA 1:    phase23c, 31–34 (self-hosting/native backend)
FROZEN ERA 2:    phase35–39 (DB, AI, HTTP, toolchain) — 38a unfreezes first
```
