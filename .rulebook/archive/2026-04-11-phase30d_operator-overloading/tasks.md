## 1. Core operator behaviors
- [x] 1.1 Create `lib/core/src/ops/` module with Add, Sub, Mul, Div, Rem behaviors — already exists (arith.tml)
- [x] 1.2 Add Neg, Not behaviors for unary operators — already exists (arith.tml:114, bit.tml:71)
- [x] 1.3 Add Index[Idx] behavior — already exists (index.tml:45)
- [x] 1.4 Add PartialEq, PartialOrd — already exists (traits/cmp.tml:205, 269)

## 2. Type checker — operator resolution
- [x] 2.1 On BinaryExpr with non-primitive operands, lookup corresponding operator behavior impl — THIR lowering (thir_lower.cpp:303-315)
- [x] 2.2 Resolve return type from `This::Output` associated type — type flows from left operand type
- [x] 2.3 On UnaryExpr with non-primitive operand, lookup Neg/Not behavior — already wired
- [x] 2.4 On IndexExpr with non-primitive base, lookup Index behavior — already wired
- [x] 2.5 Keep primitive type operators as built-in (no behavior dispatch) — is_primitive_numeric guard at thir_lower.cpp:303

## 3. Codegen
- [x] 3.1 Emit behavior method call for non-primitive binary ops — MIR path (thir_mir_builder.cpp:811-829) + AST path (binary_ops.cpp:48-89, fixed K001 GEP bug)
- [x] 3.2 Emit behavior method call for non-primitive unary ops — already wired
- [x] 3.3 Emit Index::index call for non-primitive indexing — already wired

## 4. Tail (mandatory)
- [x] 4.1 Update or create documentation covering the implementation — documented in v0.3.0 patch notes
- [x] 4.2 Write tests covering the new behavior — operator_overload.test.tml (method call, operator+, primitives)
- [x] 4.3 Run tests and confirm they pass — 222/222 pass (2 pre-existing failures)
