# phase25b — LLVM Verifier as Hard Error (Stabilization ERA 0, Phase A3)

> Analysis: `docs/analysis/tml-table-analysis/06-execution-plan.md` (Phase A) +
> `03-codegen-stability.md` (F-005). Goal: K001 (invalid LLVM IR) can never
> regress silently again.

## 1. Implementation
- [ ] 1.1 Run `llvm::verifyModule` on every emitted module in the backend (both legacy AST path and MIR path) before handing IR to the JIT/linker; on failure, emit a structured diagnostic (K-class) including the verifier message and the offending function name
- [ ] 1.2 Make verifier failure a hard compile error in debug builds and CI (no silent fallback, no partial emission)
- [ ] 1.3 Add a `--verify-ir` flag (or make it default-on in debug) so users/tests can force verification in release builds too
- [ ] 1.4 Inventory the currently-failing K001 suites (`std/collections` btreemap/btreeset/arraylist, `hir_types`, `infer_differential`, `c_preprocessor`, `c_frontend Maybe[Heap[CBlockItem]]`) into a tracked known-failures list consumed by CI — pre-existing failures stay visible but NEW verifier failures fail the build
- [ ] 1.5 Wire into the quality gate: pre-push runs the verifier-enabled suite; document in AGENTS.override.md/CI docs

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
