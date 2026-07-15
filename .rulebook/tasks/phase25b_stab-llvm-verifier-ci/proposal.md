# Proposal: phase25b_stab-llvm-verifier-ci

## Why

K001 ("Failed to parse LLVM IR", `compiler/src/backend/llvm_backend.cpp:219,441`)
means the backend emitted IR that LLVM itself rejects — and it is a STANDING
failure across std/collections (btreemap/btreeset/arraylist), hir_types,
infer_differential, and c_preprocessor (.rulebook/PLANS.md:56-60). Worse, new
K001s appear as side effects of unrelated changes (phase24l Attempt 3 broke 4
unrelated lib/core files). Invalid IR must be structurally unable to land
silently: every module must pass the LLVM verifier or the build fails.

## What Changes

`llvm::verifyModule` runs on every emitted module (MIR and legacy paths)
before JIT/link; failure is a hard, structured K-class diagnostic naming the
offending function. Debug builds and CI have it always-on; `--verify-ir`
forces it elsewhere. The currently-failing suites go into an explicit
known-failures list so pre-existing K001s stay visible while any NEW verifier
failure fails the build.

## Impact

- Affected specs: none.
- Affected code: `compiler/src/backend/llvm_backend.cpp`, codegen entry
  points, CI config, known-failures manifest.
- Breaking change: NO for valid programs; builds that silently emitted
  broken IR now fail loudly (intended).
- User benefit: K001 can never regress silently again; phase27a fixes are
  permanently protected.

## Source

- docs/analysis/tml-table-analysis/06-execution-plan.md — Phase A3.
- Analysis finding F-005.
