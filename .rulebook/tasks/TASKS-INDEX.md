# TML Project — Task Index

**Last updated**: 2026-07-15 (numbering re-sequenced into one continuous timeline)
**Strategic direction**: Stabilization first (ERA 0). Everything after phase 28 is
future work, ordered by intended execution; it starts only when the ERA 0 gates are
green. Rationale + evidence: [`docs/analysis/tml-table-analysis/`](../../docs/analysis/tml-table-analysis/README.md).

> **Re-sequencing note (2026-07-15):** the phase24 band-aid line (phase0z, 24i–n)
> was ARCHIVED — superseded by the root fix (phase26); their repros live on in the
> phase25a determinism corpus and their attempt logs feed ADR-009. Frozen tasks were
> renumbered into the continuous timeline below (31→30, 32→31, 33→32, 34→33,
> 23c→34a, 38a/b→29a/b, 38c–f→38a–d). Analysis docs 01–07 cite the PRE-resequence
> ids. Completed: phase25a/25b (archived 2026-07-15, v0.3.53/v0.3.54).

---

## ERA 0 — STABILIZATION (ACTIVE)

### ✅ Phase 25 — Measure honestly (Plan Phase A) — COMPLETE

| ID | Task | Result |
|----|------|--------|
| 25a | Determinism harness + adversarial allocator | **Done, v0.3.53** — runner + corpus + `TML_ALLOC_POISON`/`QUARANTINE` + baseline ×100 + pre-push gate |
| 25b | LLVM verifier as hard error | **Done, v0.3.54** — all 3 emission paths, pre+post-opt, K002 hard, zero fallout across 209 suites |

### Phase 26 — Memory-model soundness (Plan Phase B — THE milestone)

| ID | Task | Status | Depends on |
|----|------|--------|------------|
| 26a | ADR-009: Memory-Model Decision | **✅ Done — B3 ACCEPTED** (unify paths → drop-flags once). Archived | — |
| 26b | [Memory-Model Implementation (B3)](phase26b_memmodel-implementation/) | **In progress** — Step 1 done (v0.3.55 F-013 fix); Step 2 next (MIR gap closure + pipeline flip) | 26a ✅ |
| 26c | [Revert phase24 Band-Aids + Close Bug Class](phase26c_memmodel-bandaid-revert/) | Pending | 26b |
| 26d | [Stdlib Copy-Hazard Sweep (library-level, parallel)](phase26d_stdlib-copy-hazards-sweep/) | Pending — F-017/018/020, model-independent safe wins | none (parallel w/ 26b) |
| 26e | [Collection Borrow Accessors (the zero-cost enabler)](phase26e_collection-borrow-accessors/) | Pending — F-021, new lang+codegen surface | 26b |

**Gate:** `essential.c --emit=ast` ×100 = 100/100 under adversarial allocator, ALL
phase24 workarounds reverted, the 13 F-016 UAF sites + F-022 leaks sound by
construction. See `docs/analysis/tml-table-analysis/08-memory-copy-audit.md`
(F-015..F-022): TML has **no real move semantics** today — the copy-instead-of-move
class is systemic, not the isolated F-013.

### Phase 27 — Codegen stability (Plan Phase C)

| ID | Task | Status | Depends on |
|----|------|--------|------------|
| 27a | [K001 Root-Cause Sweep](phase27a_codegen-k001-root-cause/) | Pending (4 fresh specimens recorded) | 25b ✅ |
| 27b | [X002 Hangs / X003 Crashes](phase27b_codegen-x002-x003-hangs/) | Pending | 25a ✅ |

**Gate:** full compiler suite green ×100 consecutive runs.

### Phase 28 — Prove it with a real app (Plan Phase D)

| ID | Task | Status | Depends on |
|----|------|--------|------------|
| 28a | [UzDB-Core Acceptance App + Soak Test](phase28a_uzdb-core-acceptance/) | Pending | 26c, 27a, 27b |
| 28b | [Async File I/O Hardening](phase28b_async-file-hardening/) | Pending | 28a |

**Gate:** soak binary (millions of ops) ×100 = 100/100, leak-check clean.

---

## FUTURE — post-ERA 0, in execution order

### Phase 29 — Ecosystem unfreeze wave 1 (Plan Phase E1 — FIRST after ERA 0)

Does not depend on the memory model; unblocks external adopters.

| ID | Task | Was |
|----|------|-----|
| 29a | [Package Manager / Registry](phase29a_package-manager/) | phase38a |
| 29b | [Package Manager Alt (CLI + registry integration)](phase29b_package-manager-alt/) | phase38b |

### Phases 30–33 — Self-hosting / native backend resume (needs 26+27 gates)

The self-hosted compiler is TML's own memory model at application scale — it
resumes only on the fixed foundation.

| ID | Task | Was |
|----|------|-----|
| 30a–d | [builtins io/string/sync](phase30a_native-builtins-io-string-sync/) · [simd-avx](phase30b_native-builtins-simd-avx/) · [reflect-dyncall](phase30c_native-builtins-reflect-dyncall/) · [runtime-modules](phase30d_native-runtime-modules/) | 31a–d |
| 31a–c | [async-await](phase31a_native-async-await/) · [class-oop](phase31b_native-class-oop-codegen/) · [dyn-dispatch-vtable](phase31c_native-dyn-dispatch-vtable/) | 32a–c |
| 32a–d | [optimization-passes](phase32a_native-optimization-passes/) · [collections-codegen](phase32b_native-collections-codegen/) · [cast-conversion](phase32c_native-cast-conversion-complete/) · [try-operator](phase32d_native-try-operator-complete/) | 33a,c,d,e |
| 33a–b | [default-backend-switch](phase33a_native-default-backend-switch/) · [full-test-suite-pass](phase33b_native-full-test-suite-pass/) | 34a–b |

### Phase 34 — C/C++ frontend (after the native backend line)

| ID | Task | Was |
|----|------|-----|
| 34a | [C++ Subset Frontend](phase34a_cpp-subset-frontend/) | phase23c |

### Phases 35–37 — Feature waves (unchanged numbering; each gains a mandatory soak-test gate on unfreeze)

| ID | Task | Area |
|----|------|------|
| 35a–d | [mongodb](phase35a_db-mongodb/) · [redis-mysql](phase35b_db-redis-mysql/) · [db-perf](phase35c_db-perf-optimization/) · [typeorm-parity](phase35d_db-typeorm-parity/) | Databases |
| 36a–f | [cuda](phase36a_ia-cuda/) · [model-loading](phase36b_ia-model-loading/) · [inference](phase36c_ia-inference/) · [lora-finetune](phase36d_ia-lora-finetune/) · [distributed](phase36e_ia-distributed/) · [bench-serve](phase36f_ia-bench-serve/) | AI / ML |
| 37a–c | [http-performance](phase37a_http-performance/) · [http-benchmark](phase37b_http-benchmark/) · [db-http-integration](phase37c_db-http-integration/) | HTTP |

### Phases 38–39 — Toolchain & advanced backends

| ID | Task | Was |
|----|------|-----|
| 38a | [auto-parallel](phase38a_auto-parallel/) | 38c |
| 38b | [cross-compilation](phase38b_cross-compilation/) | 38d |
| 38c | [self-hosting-legacy](phase38c_self-hosting-legacy/) | 38e |
| 38d | [cranelift-backend](phase38d_cranelift-backend/) | 38f |
| 39a | [iouring-http-backend](phase39a_iouring-http-backend/) | — |

---

## ARCHIVED (2026-07-15) — superseded phase24 band-aid line

Per-site fixes proven non-convergent (phase24l attempt log); root fix = phase26.
Full dirs with attempt logs in `.rulebook/tasks/archive/2026-07-15-*`.

| Task | Disposition |
|------|-------------|
| phase0z_cc-driver-essential-c-residual | essential.c gate → 26b/26c |
| phase24i_cc-variadic-macro-paste | shipped v0.3.50; repros → 25a corpus |
| phase24j_cc-blockitem-shared-migration | shipped v0.3.51; band-aids reverted in 26c |
| phase24k_essential-cleanup-segv | diagnosis → ADR-009 |
| phase24l_shared-get-aliasing-deep-fix | attempt log = ADR-009 key evidence |
| phase24m_essential-c-residual-segv | absorbed into 26b gates |
| phase24n_cc-preproc-aliasing-sweep | shipped v0.3.52 (ptr_read_clone); superseded by 26b |

---

## Roadmap summary

```
ERA 0 (ACTIVE):  [25a ✅ 25b ✅ 26a ✅] → 26b(in progress) + 26d(parallel) → 26c → 26e
                 → 27a,27b → 28a → 28b
FUTURE:          29 (registry) → 30–33 (native/self-hosting) → 34 (C++ frontend)
                 → 35–37 (DB/AI/HTTP features) → 38–39 (toolchain/backends)
ARCHIVED:        phase24 band-aid line (7 tasks) + 25a/25b/26a (done, evidence preserved)
```
