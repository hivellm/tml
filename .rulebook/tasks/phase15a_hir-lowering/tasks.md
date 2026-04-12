# Tasks: HIR Lowering — Rewrite in TML

**Status**: In Progress (10/24)
**Depends on**: phase14d (type checker complete, TypeEnv available)
**Blocks**: phase15b (THIR needs HIR output)
**Duration**: 8–10 weeks
**Risk**: High — desugaring and monomorphization have subtle semantics
**C++ reference**: ~15,207 LOC → ~9,900 TML

---

## Phase 1: HIR Data Types (4 items)

- [x] 1.1 Create `compiler-tml/src/hir/common.tml` — module root (named common.tml because `mod` is a keyword)
- [x] 1.2 Create `compiler-tml/src/hir/expr.tml` — `HirExpr` enum: 29 expression variants with resolved types, HirBinOp, HirUnaryOp, HirCompoundOp, HirLitValue, hir_expr_type(), hir_expr_id()
- [x] 1.3 Create `compiler-tml/src/hir/stmt.tml` — `HirStmt` enum (Let, Expr) + `compiler-tml/src/hir/pattern.tml` — `HirPattern` enum (9 variants: Wildcard, Binding, Literal, Tuple, Struct, Enum, Or, Range, Array)
- [x] 1.4 Create `compiler-tml/src/hir/module.tml` — HirModule, HirFunction, HirStruct, HirEnum, HirImpl, HirConst, HirParam, HirField, HirVariant, hir_module_new()

## Phase 2: HIR Builder Core (6 items)

- [x] 2.1 Create `compiler-tml/src/hir/builder.tml` — HirBuilder struct with TypeEnv, ID generator, error list, monomorphization cache
- [x] 2.2 Implement `lower_module(b, ast) -> HirModule` — entry point processing all top-level declarations
- [x] 2.3 Implement function lowering: lower_function with param lowering, body lowering, desugared var→let mut
- [x] 2.4 Implement struct/enum lowering: lower_struct with field indexing, lower_enum with discriminant assignment
- [x] 2.5 Implement desugaring: `var x = v` → HirLetStmt with is_mutable=1 and HirBindingPattern, statement lowering for Let/Var/Expr
- [ ] 2.6 Implement closure capture analysis: identify captured variables, determine capture mode (by ref, by value, by move)

## Phase 3: Expression Lowering (5 items)

- [ ] 3.1 Create `compiler-tml/src/hir/lower_expr.tml` — dedicated expression lowering module (currently inlined in builder.tml as lower_expr_dispatch)
- [ ] 3.2 Implement literal/variable/field/index expressions — attach resolved types (literal + variable done; field, index, binary, unary pending)
- [ ] 3.3 Implement call expressions: resolve callee, lower args, attach return type (basic call done; method call, type args pending)
- [ ] 3.4 Implement control flow: lower if/else, when (match), loop — with type-annotated branches
- [ ] 3.5 Implement closures: create closure struct with captured fields, lower body as separate function

## Phase 4: Monomorphization (4 items)

- [ ] 4.1 Create `compiler-tml/src/hir/monomorph.tml` — generic instantiation engine
- [ ] 4.2 Implement: when generic function/type is used with concrete types, generate specialized version
- [ ] 4.3 Implement monomorphization queue: collect all needed instantiations, process iteratively until fixpoint
- [x] 4.4 Implement name mangling: mangle_name() in builder.tml — `List` + `[I32]` → `List_I32`

## Phase 5: HIR Printer + Serialization (3 items)

- [ ] 5.1 Create `compiler-tml/src/hir/printer.tml` — pretty-print HIR for debugging
- [ ] 5.2 Implement HIR serialization (binary) for hybrid pipeline bridge
- [ ] 5.3 Test: round-trip HIR through serialize/deserialize, verify identical

## Phase 6: Differential Testing (2 items)

- [ ] 6.1 Lower 20 stdlib modules to HIR → compare with C++ HIR output (field by field)
- [ ] 6.2 Lower full test suite → verify zero diffs against C++ HIR builder

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
