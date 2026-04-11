## 1. Parser
- [x] 1.1 Parse `..expr` after field inits in struct literal — already implemented (parser_expr_complex.cpp:564, DotDot token)
- [x] 1.2 Add StructUpdate field to StructInit AST node — already exists (ast_exprs.hpp:300 `std::optional<ExprPtr> base`)
- [x] 1.3 Wire through AST serialization/visitor — already done

## 2. Type checker
- [x] 2.1 Verify `..expr` type matches the struct being constructed — type-checks correctly
- [x] 2.2 Compute set of unspecified fields that need copying from source — handled in THIR→MIR lowering via env_.lookup_struct()

## 3. Codegen — MIR path (commit 4220c72a)
- [x] 3.1 Add `base` and `from_base` fields to StructInitInst (mir.hpp)
- [x] 3.2 In THIR→MIR lowering: populate all fields — explicit from expressions, base-sourced flagged with from_base (thir_mir_builder_expr.cpp)
- [x] 3.3 In MIR emitter: emit `extractvalue %struct base, <idx>` for from_base fields before insertvalue chain (instructions_misc.cpp)
- [x] 3.4 Update DCE is_used to track StructInitInst.base references (mir_pass.cpp)

## 4. Codegen — Legacy AST path
- [x] 4.1 Field-by-field GEP copy for unspecified fields from base (llvm_struct_expr.cpp)

## 5. Tail (mandatory)
- [x] 5.1 Runtime verified: `Point { x: 999, ..p1 }` outputs correct values (999, 200, 300)
- [x] 5.2 Test file added: compiler/tests/compiler/struct_update.test.tml (needs test::assert fix to pass)
- [ ] 5.3 Update CHANGELOG.md
- [ ] 5.4 Run full test suite green
