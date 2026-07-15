# Proposal: phase27a_codegen-k001-root-cause

## Why

K001 invalid-IR failures are standing in the flagship container suites
(std/collections btreemap/btreeset/arraylist) plus hir_types,
infer_differential, c_preprocessor, and c_frontend (PLANS.md:56-60). The
release history shows the instance-by-instance approach (TemplateLiteral
0.3.38, List[Str]::push 0.3.46, struct forward-ref 0.3.20-25) does not
prevent new instances, because the MECHANISM — impl-level type-param
re-inference heuristics, mangling drift, struct-by-value ABI fixups applied
inconsistently — keeps minting mismatched symbols. Generic collections of
user structs are the bread and butter of application code; they must be
K001-free.

## What Changes

Root-cause sweep of every standing K001, grouped by mechanism rather than by
test: (1) container-suite failures, (2) hir_types/infer_differential,
(3) c_preprocessor + c_frontend `Maybe[Heap[CBlockItem]]`, (4) the
generic-`Shared`-user class exposed by phase24l Attempt 3 (future_fuse,
cache_*). Replace the inference heuristics in `method_impl.cpp` (and the
0.3.39 ABI fallback) with authoritative signature data
(`impl_self_type_args`) wherever a heuristic remains. Runs under the
phase25b verifier so each fix is locked in.

## Impact

- Affected specs: none.
- Affected code: `compiler/src/codegen/llvm/expr/method_impl.cpp`,
  `method_static_dispatch.cpp`, mangling/monomorphization paths,
  `compiler/src/codegen/mir/`.
- Breaking change: NO.
- User benefit: zero K001 across compiler + core + std suites; generic
  container code stops being a codegen minefield.

## Source

- docs/analysis/tml-table-analysis/03-codegen-stability.md (F-005, F-007).
- .rulebook/PLANS.md standing-failures list.
