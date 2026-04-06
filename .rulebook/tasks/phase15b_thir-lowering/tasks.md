# Tasks: THIR Lowering — Rewrite in TML

**Status**: Planned (0/16)
**Depends on**: phase15a (HIR output available)
**Blocks**: phase15c (MIR builder needs THIR)
**Duration**: 3–4 weeks
**Risk**: Medium — thin pass, most complexity already handled by type checker (phase14d)
**C++ reference**: ~3,042 LOC → ~2,000 TML

---

## Phase 1: THIR Data Types (3 items)

- [ ] 1.1 Create `compiler-tml/src/thir/mod.tml` — module root
- [ ] 1.2 Create `compiler-tml/src/thir/expr.tml` — `ThirExpr` enum: HIR expressions + coercion nodes + resolved method calls
- [ ] 1.3 Create `compiler-tml/src/thir/module.tml` — `ThirModule` struct

## Phase 2: THIR Lowering Pass (6 items)

- [ ] 2.1 Create `compiler-tml/src/thir/lower.tml` — `ThirLower` struct and `lower(hir: HirModule) -> ThirModule` entry point
- [ ] 2.2 Implement coercion insertion: insert implicit type conversions (integer widening, ref coercion, deref coercion)
- [ ] 2.3 Implement method resolution: resolve `obj.method()` to concrete impl method via trait solver
- [ ] 2.4 Implement operator desugaring: `a + b` → `a.add(b)`, `a[i]` → `a.index(i)`, `a == b` → `a.eq(b)`
- [ ] 2.5 Implement associated type normalization: replace `<T as Iterator>::Item` with concrete type
- [ ] 2.6 Implement `when` arm processing: lower each arm pattern + guard + body

## Phase 3: Exhaustiveness Checker (4 items)

- [ ] 3.1 Create `compiler-tml/src/thir/exhaustiveness.tml` — pattern exhaustiveness analysis
- [ ] 3.2 Implement: for each `when` expression, compute whether all possible values are covered
- [ ] 3.3 Implement: enum exhaustiveness — all variants present, or wildcard covers remainder
- [ ] 3.4 Implement: emit warning for unreachable patterns, error for non-exhaustive match

## Phase 4: Differential Testing (3 items)

- [ ] 4.1 Lower 20 stdlib modules through HIR→THIR → compare with C++ THIR output
- [ ] 4.2 Lower full test suite → verify zero diffs against C++ THIR lowerer
- [ ] 4.3 Specifically test: operator desugaring, coercion insertion, exhaustiveness on 10 edge-case files
