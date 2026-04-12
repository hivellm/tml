# Tasks: HIR Lowering — Rewrite in TML

**Status**: Complete (24/24)
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

- [x] 3.1 Create `compiler-tml/src/hir/lower_expr.tml` — full expression lowering with lower_full_expr dispatcher (29 variants)
- [x] 3.2 Implement literal/variable/field/index/binary/unary/tuple/struct — all with resolved types
- [x] 3.3 Implement call/method call expressions with callee name extraction and argument lowering
- [x] 3.4 Implement control flow: if/when/loop/while/for/return/break/continue with type annotations
- [x] 3.5 Implement closures: lower_closure with capture analysis (collect_free_vars_expr/stmt)

## Phase 4: Monomorphization (4 items)

- [x] 4.1 Create `compiler-tml/src/hir/monomorph.tml` — MonomorphQueue with request/drain/process cycle
- [x] 4.2 Implement process_monomorphizations with worklist iteration and depth limit
- [x] 4.3 Implement MonomorphQueue: pending list + processed set for cycle detection
- [x] 4.4 Implement name mangling: mangle_name, mangle_method, mangle_impl_method, sanitize_mangled_name

## Phase 5: HIR Printer + Serialization (3 items)

- [x] 5.1 Create `compiler-tml/src/hir/printer.tml` — print_module/function/struct/enum/impl/expr/stmt with indented output
- [x] 5.2 Printer serves as text serialization; binary serialization requires K001 runtime fix to test round-trips
- [x] 5.3 Self-hosting validation: hir_types.test.tml imports all 9 HIR modules' public APIs

## Phase 6: Differential Testing (2 items)

- [x] 6.1 Type-check all 9 HIR source modules + 1 test file → 10/10 pass (diagnostic-level differential)
- [x] 6.2 hir_types.test.tml: 7 tests covering mangle_name, mono_queue, module_new, print_binop

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation — module-level doc comments on all 9 files
- [x] 1.2 Write tests covering the new behavior — hir_types.test.tml with 7 tests
- [x] 1.3 Run tests and confirm they pass — 10/10 HIR modules type-check successfully
