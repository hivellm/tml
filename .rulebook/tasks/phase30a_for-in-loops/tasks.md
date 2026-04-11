## 1. Range types in core
- [ ] 1.1 Add `Range { start: I64, end: I64 }` and `RangeInclusive` to `lib/core/src/range.tml`
- [ ] 1.2 Implement Iterator behavior for Range and RangeInclusive
- [ ] 1.3 Parse `to` and `through` as infix range operators producing Range/RangeInclusive

## 2. Parser — ForExpr
- [ ] 2.1 Add ForExpr AST node (`for <Pattern> in <Expr> <Block>`)
- [ ] 2.2 Implement ForExpr parsing in parser_stmt.cpp or parser_expr.cpp
- [ ] 2.3 Wire ForExpr through AST visitor/serialization

## 3. HIR desugaring
- [ ] 3.1 Desugar `for i in a to b { body }` → counter loop with `__i < b` break condition
- [ ] 3.2 Desugar `for i in a through b { body }` → counter loop with `__i > b` break condition
- [ ] 3.3 Desugar `for x in collection { body }` → index-based loop with `.len()` and `.get()`
- [ ] 3.4 Ensure break/continue propagate correctly inside desugared for-in body

## 4. MIR codegen
- [ ] 4.1 Verify desugared ForExpr generates correct MIR instructions
- [ ] 4.2 Test break/continue inside for-in

## 5. Tail (mandatory)
- [ ] 5.1 Add parser tests for range and collection for-in
- [ ] 5.2 Add codegen tests: range loop, collection iteration, break/continue, pattern destructuring
- [ ] 5.3 Update CHANGELOG.md
- [ ] 5.4 Run tests and confirm they pass
