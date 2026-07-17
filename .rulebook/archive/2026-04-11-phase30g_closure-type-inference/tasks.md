## 1. Parser — optional closure param types
- [x] 1.1 Allow closure params without `: Type` annotation — already implemented (parser_expr_complex.cpp:492-498, `:` is optional)
- [x] 1.2 Store param type as optional in ClosureExpr AST node — `std::optional<TypePtr>` in ast_exprs.hpp:563

## 2. Type checker — bidirectional inference
- [x] 2.1 When type-checking a closure argument, propagate the expected `func(T) -> U` signature — expr_call.cpp:749-758 unifies closure types with expected func types
- [x] 2.2 Fill in missing param types from the expected signature — types_checker.cpp:278 creates fresh type vars, expr_call.cpp:754 unifies with expected
- [x] 2.3 Infer return type from expected signature if not explicitly annotated — types_checker.cpp:301 uses body_type, expr_call.cpp:757 unifies with expected return
- [x] 2.4 Inferred types stored in AST for HIR builder — inferred_param_types/inferred_return_type fields, used by hir_builder_expr.cpp:1191 and closure.cpp:149

## 3. Implicit return
- [x] 3.1 If closure body is a single expression, treat as return value — types_checker.cpp:300-301, body_type used as return_type when no annotation
- [x] 3.2 Type-check the expression against the expected return type — unification at expr_call.cpp:757

## 4. Tail (mandatory)
- [x] 4.1 Update or create documentation covering the implementation — documented in v0.3.0 patch notes
- [x] 4.2 Write tests covering the new behavior — closure_type_inference.test.tml (annotated, inferred param, inferred return, fully inferred)
- [x] 4.3 Run tests and confirm they pass — all 4 tests pass
