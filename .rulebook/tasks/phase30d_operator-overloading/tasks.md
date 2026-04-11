## 1. Core operator behaviors
- [ ] 1.1 Create `lib/core/src/ops/` module with Add, Sub, Mul, Div, Rem behaviors (each with `type Output` and method)
- [ ] 1.2 Add Neg, Not behaviors for unary operators
- [ ] 1.3 Add Index[Idx] behavior with `type Output` and `index(ref this, idx: Idx) -> ref This::Output`
- [ ] 1.4 Add PartialEq, PartialOrd operator behaviors for `==`, `<`, `>`, `<=`, `>=`

## 2. Type checker — operator resolution
- [ ] 2.1 On BinaryExpr with non-primitive operands, lookup corresponding operator behavior impl
- [ ] 2.2 Resolve return type from `This::Output` associated type
- [ ] 2.3 On UnaryExpr with non-primitive operand, lookup Neg/Not behavior
- [ ] 2.4 On IndexExpr with non-primitive base, lookup Index behavior
- [ ] 2.5 Keep primitive type operators as built-in (no behavior dispatch)

## 3. Codegen
- [ ] 3.1 Emit behavior method call for non-primitive binary ops
- [ ] 3.2 Emit behavior method call for non-primitive unary ops
- [ ] 3.3 Emit Index::index call for non-primitive indexing

## 4. Tail (mandatory)
- [ ] 4.1 Add tests: BigInt + operator, custom Vec2 + operator
- [ ] 4.2 Add tests: Index behavior for custom collection
- [ ] 4.3 Add tests: primitives unchanged (no performance regression)
- [ ] 4.4 Update CHANGELOG.md
- [ ] 4.5 Run tests and confirm they pass
