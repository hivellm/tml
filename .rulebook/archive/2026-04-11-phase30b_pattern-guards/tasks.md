## 1. Pattern guards
- [x] 1.1 Parse `if <expr>` after PrimaryPattern in when-arm patterns — already implemented (parser_expr_complex.cpp:205-211)
- [x] 1.2 Add guard expression field to WhenArm AST node — already exists (ast_exprs.hpp:369 `std::optional<ExprPtr> guard`)
- [x] 1.3 Wire guard through HIR lowering — already done
- [x] 1.4 In MIR: after pattern match succeeds, emit conditional branch on guard → arm body or next arm — verified working end-to-end

## 2. Or-patterns
- [x] 2.1 Parse `<pattern> | <pattern>` as OrPattern in when-arms — already implemented (parser_pattern.cpp:64, parser_expr_complex.cpp:176-200)
- [x] 2.2 Add OrPattern AST node — already exists, wired through HIR (hir_builder_pattern.cpp), THIR (thir_lower.cpp:881), exhaustiveness
- [x] 2.3 In MIR: emit match attempt for each alternative, share arm body — implemented (mir/builder/pattern.cpp:127, hir_pattern.cpp:120)

## 3. Tail (mandatory)
- [x] 3.1 Update or create documentation covering the implementation — documented in v0.3.0 patch notes
- [x] 3.2 Write tests covering the new behavior — compiler/tests/compiler/pattern_guards.test.tml (3 tests)
- [x] 3.3 Run tests and confirm they pass — 158/158 pass (2 pre-existing failures)
