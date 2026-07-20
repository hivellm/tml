# phase0h — Alias attributes + minimal TBAA (release parity for aggregate code)

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` findings
> L-062/L-063/L-064 (04-shipping-codegen.md). The two structural blockers to
> LLVM -O2 recovering the naive IR on aggregate/collection code. Additive and
> NOT wasted by any later codegen unification.

## Motivation

Across the 75K-LOC shipping codegen there are zero pointer-parameter attributes
on user/monomorphized functions (grep: 10 hits total, all fixed runtime decls)
and zero `!tbaa`/`!alias.scope` metadata. Without `noalias`/`readonly`/TBAA,
-O2 must assume every pointer aliases everything and cannot eliminate loads
across the pervasive runtime calls — this is why aggregate-heavy code stays
above 2× Rust even at release while scalar code recovers.

## 1. Implementation
- [ ] 1.1 Introduce ONE shared signature/attribute emit helper (first step of
  the typed-emit layer, L-066) and thread parameter attributes through
  `compiler/src/codegen/llvm/decl/func.cpp` / `decl/impl.cpp` and call sites:
  `noalias nocapture readonly dereferenceable(N) align A` for immutable `ref`
  params, `dereferenceable/align` for mutable refs; audit `this` params.
- [ ] 1.2 Minimal TBAA tree (root → primitives → struct-path nodes) emitted
  once per module and attached to loads/stores via the shared helper.
- [ ] 1.3 Enum construction via `insertvalue` chains (like struct literals) +
  `sret` for aggregate returns larger than a register pair; delete the dead
  `bitcast ptr→ptr` per payload store (L-064).
- [ ] 1.4 Gate: `/compare-ir` corpus at -O2 reaches ≤2× Rust on the
  aggregate/collection probes (verify generator provenance per F-004 first);
  full suite green — wrong alias metadata = miscompile, treat any suspicious
  failure as such.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation (codegen attribute/TBAA policy note for future
  emitters)
- [ ] 2.2 Write tests covering the new behavior (IR-assertion fixtures:
  attributes on ref params, tbaa on field access, sret on big returns)
- [ ] 2.3 Run tests and confirm they pass
