# Proposal: phase33b_native-full-test-suite-pass (renumbered from phase34b, 2026-07-15 ERA 0 pivot)

## Why
The native backend is now the default on Windows x86-64 (phase34a). For that switch to be safe, every test in the compiler test suite must pass on the native path — not just the targeted codegen tests added in phases 33a–33e. Currently a subset of the 266 compiler tests fail on the native backend due to unresolved codegen gaps (incorrect ABI for multi-return values, missing linker symbol exports, a few remaining instruction lowering holes). Shipping a broken default backend undermines user trust and defeats the purpose of the switch. This task drives the suite to 266/266 and adds a performance baseline benchmark.

## What Changes
- Run the full 266-test compiler suite under `--backend=native` and triage all failures into three buckets: codegen, ABI, or linker.
- Fix codegen failures: patch `emit_inst.tml`, `emit_expr.tml`, or `emit_intrinsic.tml` for each remaining unhandled instruction or expression form identified in the triage.
- Fix ABI failures: correct calling-convention mismatches in the native backend's function-call emission for multi-return structs, variadic calls, and `@extern("c")` declarations.
- Fix linker failures: ensure all runtime symbols referenced by native-backend output (`__tml_panic`, `__tml_alloc`, collection runtime helpers) are exported from `tml_compiler.dll` or linked from the correct static archive.
- Add a benchmark entry in `docs/benchmarks.md` recording native-backend compile time and output binary size for a canonical 500-line TML program, compared to the LLVM backend.

## Impact
- Affected specs: `compiler-tml/src/codegen/emit_inst.tml`, `emit_expr.tml`, `emit_intrinsic.tml`, `emit_call.tml`
- Affected code: any codegen, ABI, or linker file implicated by the triage; `docs/benchmarks.md`
- Breaking change: NO
- User benefit: The native backend is production-ready on Windows x86-64; users get a measurably faster compile loop without any test regressions.
