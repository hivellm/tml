# 06 — Execution Plan: Corrective Roadmap

The ordering is deliberate: **nothing downstream matters until the memory model and the
codegen verifier are sound.** Each phase has a concrete, measurable exit gate. Phases map to
findings in files 01–05.

---

## Phase A — Stop the bleeding / measure honestly

**Addresses: F-008, F-005 (regression prevention), F-014 (scope control).**

- **A1. Freeze feature phases 35–38.** Do not start MongoDB/Redis/CUDA/AI/cross-compile work.
  Every line written now against the band-aid APIs is future rework (file 04).
- **A2. Build a determinism harness.** Run the standing repros (`essential.c`, `sig_alone.c`,
  `c_essential_repro.c`) ×100 and record the crash rate as a tracked CI metric. Add
  allocator adversarialization (guard pages, allocation shuffling, ASan-style poisoning) so
  heap-layout-dependent bugs surface deterministically instead of 2-in-30.
  - **Gate:** crash rate is a number in CI, not a vibe.
- **A3. Turn on the LLVM verifier in CI as a hard error** so K001 cannot regress silently.
  - **Gate:** every merged commit produces verifier-clean IR for all passing suites.

## Phase B — Fix the memory model at the root (THE milestone)

**Addresses: F-001, F-002, F-003, F-004, F-013.**

Pick **ONE** model and commit; the hybrid band-aid path is proven non-convergent (phase24l
Attempt log, file 02 F-003):

- **Option B1 (Rust-faithful): move/init-state + drop-flag elaboration in MIR.**
  Track per-local initialization/moved-out state through the CFG; a value read-by-copy out of
  a container becomes either *borrow-then-clone* or an explicit move — never a silent
  alias-and-double-drop. Emit conditional drops guarded by drop flags. This makes `.get()`
  semantics well-defined and lets the existing borrow checker (NLL + Polonius,
  `compiler/src/borrow/`) see through container abstractions via lowering instead of being
  blind to raw pointers (F-004).

- **Option B2 (ARC/Swift-style): compiler-inserted refcount operations.**
  Make the fundamental owning types refcounted end-to-end and have codegen auto-insert retain
  on every copy and release on every drop. Removes the need for hand-placed `.duplicate()`
  entirely, at the cost of runtime refcount traffic (optimizable later with
  retain/release-elision passes).

Decision inputs: B1 preserves TML's zero-cost story and existing borrow-checker investment
but is the harder compiler change; B2 is simpler to make sound and matches how `Shared[T]`
already wants to behave, but changes the language's performance model. **An ADR must be
written before implementation starts** (candidate: ADR-009).

- **Gate (non-negotiable):**
  1. Revert ALL phase24c–24n band-aids (into_raw hacks, `declarator_name_value_leak`,
     ~70 manual `.duplicate()` calls, `get_clone` call-site migrations).
  2. `essential.c --emit=ast` ×100 = **100/100** under the Phase-A adversarial allocator.
  3. `std/collections` suites (btreemap/btreeset/arraylist) go K001-free.

## Phase C — Codegen stability

**Addresses: F-005, F-006, F-007.**

- **C1.** Root-cause the remaining K001 sources in `std/collections`, `hir_types`,
  `infer_differential`, `c_preprocessor` — these are mangling/monomorphization mismatches
  (F-007), not one-off typos; fix the mechanism, not the instance.
- **C2.** Fix the X002 hangs (`let_patterns`, `slice_split_pred`, `builtins_imports`) and the
  X003 crash (`closure_codegen`) — core-feature failures with zero acceptable tolerance.
- **Gate:** full compiler suite green ×100 consecutive runs; zero non-deterministic failures.

## Phase D — Prove it with a real app (regain the UzDB use case)

**Addresses: F-009, F-012.**

- **D1. Self-host for real:** `essential.c`, then the full C stdlib subset, compiles
  deterministically. Only after this gate may `compiler-tml` become the default backend
  (phases 31–34 stay frozen until then).
- **D2. Build a minimal UzDB core in TML as the acceptance test:** in-memory
  `BTreeMap` store + append-only commit log + `File::sync` durability + msgpack framing,
  run under a soak test (millions of insert/read/delete cycles) with the adversarial
  allocator. **This is the real gate** — it exercises exactly F-001/F-002 at application
  scale, on the exact workload that killed the original project.
- **D3. Harden async file I/O** beyond MVP (WAL fsync-ordering path, partial-write errors,
  cancellation).

## Phase E — Ecosystem

**Addresses: F-010, F-014.**

- **E1.** Real package registry + publish flow (finish `phase38a`).
- **E2.** Only after A–D: resume the advanced feature phases (DB drivers, AI, HTTP perf),
  each with a mandatory soak test in its definition-of-done so the memory-model regression
  class cannot ship back in.

---

## Summary dependency chain

```
A (measure + freeze)
└─► B (memory model — ADR, then implementation, then band-aid revert)
    └─► C (codegen K001/X002/X003)
        └─► D (self-host proof + UzDB-core soak test)
            └─► E (registry, then feature phases 35–38 unfrozen)
```

## Immediate next actions

1. Create rulebook task for **Phase A** (determinism harness + LLVM verifier in CI +
   feature-phase freeze note in PLANS.md).
2. Write **ADR-009: Memory-model soundness — MIR drop-flag elaboration vs compiler-inserted
   ARC** (Phase B decision). This is a user/architect decision with implementation input.
3. Verify **F-013** (does MIR drop-elaborate the `inner` local in
   `Shared::increment_count`/`decrement_count`?) — cheap check, potentially explains the
   phase24l Attempt-2 imbalance.
