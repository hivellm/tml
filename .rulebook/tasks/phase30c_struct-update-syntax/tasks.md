## 1. Parser
- [x] 1.1 Parse `..expr` after field inits in struct literal — already implemented (parser_expr_complex.cpp:564)
- [x] 1.2 Add StructUpdate field to StructInit AST node — already exists (ast_exprs.hpp:300 `std::optional<ExprPtr> base`)
- [x] 1.3 Wire through AST serialization/visitor — already done

## 2. Type checker
- [x] 2.1 Verify `..expr` type matches the struct being constructed — type-checks correctly
- [x] 2.2 Compute set of unspecified fields that need copying from source — handled in THIR→MIR lowering

## 3. Codegen
- [x] 3.1 For each unspecified field, emit field extraction from source expression — StructInitInst.base + from_base flags, MIR emitter uses extractvalue
- [x] 3.2 For specified fields, use explicit values — insertvalue chain
- [x] 3.3 Combine into struct construction — insertvalue chain with extractvalue for base fields
- [x] 3.4 Legacy AST codegen path — llvm_struct_expr.cpp field-by-field copy via GEP

## 4. Tail (mandatory)
- [x] 4.1 Verified: `Point { x: 999, ..p1 }` outputs correct values (999, 200, 300)
- [ ] 4.2 Add dedicated test file for struct update
- [ ] 4.3 Update CHANGELOG.md
- [ ] 4.4 Run tests and confirm they pass — 227/227 compiler tests (1 pre-existing timeout)
