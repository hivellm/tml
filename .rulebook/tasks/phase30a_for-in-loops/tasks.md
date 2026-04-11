## 1. Range types in core
- [x] 1.1 Range/RangeInclusive desugared by HIR (hir_builder_expr.cpp:1247) — `a to b` → `Range { start: a, end: b }`
- [x] 1.2 `to` and `through` already parsed as infix operators producing Range struct

## 2. Parser — ForExpr
- [x] 2.1 ForExpr AST node exists (ast_exprs.hpp:466)
- [x] 2.2 ForExpr parsing implemented (parser_expr_complex.cpp:351-378)
- [x] 2.3 Wired through AST visitor/serialization, HIR, THIR

## 3. Codegen — Legacy AST path
- [x] 3.1 Full for-loop codegen in loop.cpp:276-453 (range loops + iterator protocol)

## 4. Codegen — MIR path (this session)
- [x] 4.1 Detect Range/RangeInclusive iter type in build_for (thir_mir_builder_control.cpp)
- [x] 4.2 build_for_range: extract start/end from ThirStructExpr fields
- [x] 4.3 Emit counter alloca + header (load+cmp) + body (bind i) + latch (increment) + exit
- [x] 4.4 Simple for-in with println in body works: `for i in 0 to 3 { println(i.to_string()) }` → 0,1,2
- [ ] 4.5 **BUG**: mutable outer variables (`var sum`) read wrong value after loop — SSA phi for outer vars not generated in for-body/exit blocks. `sum = sum + i` inside for-in produces wrong result.

## 5. Tail (mandatory)
- [ ] 5.1 Fix outer mutable variable tracking across for-in loop body
- [ ] 5.2 Add test file for for-in loops
- [ ] 5.3 Update CHANGELOG.md
- [ ] 5.4 Run tests and confirm they pass
