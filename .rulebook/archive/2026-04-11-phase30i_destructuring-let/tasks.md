## 1. Struct destructuring in let
- [x] 1.1 Parser accepts `let TypeName { field1: a, field2: b } = expr` — uses parse_pattern() for StructPattern
- [x] 1.2 Type-checker validates struct pattern bindings
- [x] 1.3 Codegen: added StructPattern handler in gen_let_stmt (llvm_ir_gen_stmt_let.cpp) — alloca + GEP + load for each field binding, Bool i8→i1 trunc included
- [x] 1.4 Runtime verified: `let Pair { first: a, second: b } = p` outputs correct values (42, 77)

## 2. Tuple destructuring in let
- [x] 2.1 Already implemented in codegen — gen_let_stmt has TuplePattern handler (llvm_ir_gen_stmt_let.cpp:147-239)
- [x] 2.2 Parser supports tuple patterns in let
- [x] 2.3 Codegen emits GEP + load for each positional element

## 3. Tail (mandatory)
- [x] 3.1 Test file: compiler/tests/compiler/destructuring_let.test.tml (2 tests using bare assert_eq: pair + triple destructuring)
- [x] 3.2 Update or create documentation covering the implementation — documented in v0.3.0 patch notes
- [x] 3.3 Write tests covering the new behavior — destructuring_let.test.tml (pair + triple)
- [x] 3.4 Run tests and confirm they pass — 222/222 pass (2 pre-existing failures)
