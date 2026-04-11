## 1. Parser — optional closure param types
- [ ] 1.1 Allow closure params without `: Type` annotation (parser already may support this — verify)
- [ ] 1.2 Store param type as Maybe[Type] in ClosureExpr AST node

## 2. Type checker — bidirectional inference
- [ ] 2.1 When type-checking a closure argument, propagate the expected `func(T) -> U` signature
- [ ] 2.2 Fill in missing param types from the expected signature
- [ ] 2.3 Infer return type from expected signature if not explicitly annotated
- [ ] 2.4 Error if closure is used in a context with no expected type and params lack annotations

## 3. Implicit return
- [ ] 3.1 If closure body is a single expression (no semicolons, no statements), treat as return value
- [ ] 3.2 Type-check the expression against the expected return type

## 4. Tail (mandatory)
- [ ] 4.1 Add tests: `list.filter(do(x) { x > 0 })` infers x: I64, return: Bool
- [ ] 4.2 Add tests: `list.map(do(x) { x.to_string() })` infers types correctly
- [ ] 4.3 Add tests: error when closure used without context and types omitted
- [ ] 4.4 Update CHANGELOG.md
- [ ] 4.5 Run tests and confirm they pass
