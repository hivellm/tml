# Tasks: HIR Lowering — Rewrite in TML

**Status**: Planned (0/24)
**Depends on**: phase14d (type checker complete, TypeEnv available)
**Blocks**: phase15b (THIR needs HIR output)
**Duration**: 8–10 weeks
**Risk**: High — desugaring and monomorphization have subtle semantics
**C++ reference**: ~15,207 LOC → ~9,900 TML

---

## Phase 1: HIR Data Types (4 items)

- [ ] 1.1 Create `compiler-tml/src/hir/mod.tml` — module root
- [ ] 1.2 Create `compiler-tml/src/hir/expr.tml` — `HirExpr` enum: all expression variants with resolved types (mirrors hir_expr.hpp, ~40 variants)
- [ ] 1.3 Create `compiler-tml/src/hir/stmt.tml` — `HirStmt` enum, `compiler-tml/src/hir/pattern.tml` — `HirPattern` enum
- [ ] 1.4 Create `compiler-tml/src/hir/module.tml` — `HirModule` struct: functions, types, impls, all with resolved type annotations

## Phase 2: HIR Builder Core (6 items)

- [ ] 2.1 Create `compiler-tml/src/hir/builder.tml` — `HirBuilder` struct: TypeEnv ref, current module, monomorphization queue
- [ ] 2.2 Implement `lower_module(ast: Module, env: ref TypeEnv) -> HirModule` — entry point
- [ ] 2.3 Implement function lowering: resolve param types, return type, lower body → `HirFunc`
- [ ] 2.4 Implement struct/enum lowering: resolve field types, compute layout, assign field indices
- [ ] 2.5 Implement desugaring: `var x = v` → `let mut x = v`, `for x in iter` → loop over iterator protocol
- [ ] 2.6 Implement closure capture analysis: identify captured variables, determine capture mode (by ref, by value, by move)

## Phase 3: Expression Lowering (5 items)

- [ ] 3.1 Create `compiler-tml/src/hir/lower_expr.tml` — expression lowering
- [ ] 3.2 Implement literal/variable/field/index expressions — attach resolved types
- [ ] 3.3 Implement call expressions: resolve callee, lower args, attach return type
- [ ] 3.4 Implement control flow: lower if/else, when (match), loop — with type-annotated branches
- [ ] 3.5 Implement closures: create closure struct with captured fields, lower body as separate function

## Phase 4: Monomorphization (4 items)

- [ ] 4.1 Create `compiler-tml/src/hir/monomorph.tml` — generic instantiation engine
- [ ] 4.2 Implement: when generic function/type is used with concrete types, generate specialized version
- [ ] 4.3 Implement monomorphization queue: collect all needed instantiations, process iteratively until fixpoint
- [ ] 4.4 Implement name mangling: `List[I32]` → `List$I32$`, `HashMap[Str, I64]` → `HashMap$Str$I64$`

## Phase 5: HIR Printer + Serialization (3 items)

- [ ] 5.1 Create `compiler-tml/src/hir/printer.tml` — pretty-print HIR for debugging
- [ ] 5.2 Implement HIR serialization (binary) for hybrid pipeline bridge
- [ ] 5.3 Test: round-trip HIR through serialize/deserialize, verify identical

## Phase 6: Differential Testing (2 items)

- [ ] 6.1 Lower 20 stdlib modules to HIR → compare with C++ HIR output (field by field)
- [ ] 6.2 Lower full test suite → verify zero diffs against C++ HIR builder
