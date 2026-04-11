## 1. Pattern guards
- [x] 1.1 Parse `if <expr>` after PrimaryPattern in when-arm patterns — already implemented (parser_expr_complex.cpp:205-211)
- [x] 1.2 Add guard expression field to WhenArm AST node — already exists (ast_exprs.hpp:369 `std::optional<ExprPtr> guard`)
- [x] 1.3 Wire guard through HIR lowering — already done
- [x] 1.4 In MIR: after pattern match succeeds, emit conditional branch on guard → arm body or next arm — verified working end-to-end

## 2. Or-patterns
- [ ] 2.1 Parse `<pattern> | <pattern>` as OrPattern in when-arms — needs verification
- [ ] 2.2 Add OrPattern AST node — needs verification
- [ ] 2.3 In MIR: emit match attempt for each alternative, share arm body — needs verification

## 3. Tail (mandatory)
- [x] 3.1 Type-check verified: `Just(x) if x > 0 => ...` passes type-checker
- [x] 3.2 Test file: compiler/tests/compiler/pattern_guards.test.tml (3 tests using bare assert_eq: basic guard, fallthrough, wildcard)
- [ ] 3.3 Update CHANGELOG.md
- [ ] 3.4 Run full test suite green
