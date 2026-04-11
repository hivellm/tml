## 1. Range types in core
- [x] 1.1 Range/RangeInclusive desugared by HIR (hir_builder_expr.cpp:1247) — `a to b` → `Range { start: a, end: b }`
- [x] 1.2 `to` and `through` already parsed as infix operators producing Range struct

## 2. Parser — ForExpr
- [x] 2.1 ForExpr AST node exists (ast_exprs.hpp:466)
- [x] 2.2 ForExpr parsing implemented (parser_expr_complex.cpp:351-378)
- [x] 2.3 Wired through AST visitor/serialization, HIR, THIR

## 3. Codegen — Legacy AST path
- [x] 3.1 Full for-loop codegen in loop.cpp:276-453 (range loops + iterator protocol)

## 4. Codegen — MIR path
- [x] 4.1 Detect Range/RangeInclusive iter type in build_for (thir_mir_builder_control.cpp)
- [x] 4.2 build_for_range: extract start/end directly from ThirStructExpr fields (avoids Range struct type mangling)
- [x] 4.3 Emit counter alloca + header (load+cmp) + body (pattern bind) + increment + backedge + exit
- [x] 4.4 SSA phi nodes for pre-loop variables — same pattern as build_while: create phis in header, complete back-edges after body, restore header vars at exit
- [x] 4.5 Verified: `for i in 0 to 5 { sum += i }` → sum=10, `for i in 1 through 5 { sum += i }` → sum=15

## 5. Tail (mandatory)
- [x] 5.1 Test file added: compiler/tests/compiler/for_in_range.test.tml (exclusive sum, inclusive sum, empty range)
- [ ] 5.2 Update CHANGELOG.md
- [ ] 5.3 Run full test suite green
