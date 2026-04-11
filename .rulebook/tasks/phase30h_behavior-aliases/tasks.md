## 1. Parser
- [ ] 1.1 After `behavior Ident`, peek `=` → parse as BehaviorAlias (TypeBound after `=`)
- [ ] 1.2 Add BehaviorAlias AST node
- [ ] 1.3 Wire through AST visitor/serialization

## 2. Type checker
- [ ] 2.1 Register behavior alias in type environment
- [ ] 2.2 When alias name used as a bound, expand to constituent bounds
- [ ] 2.3 Validate all constituent behaviors exist

## 3. Tail (mandatory)
- [ ] 3.1 Add test: `behavior Numeric = Add + Sub + Mul + Div` defines alias
- [ ] 3.2 Add test: `func sum[T: Numeric]()` expands bounds correctly
- [ ] 3.3 Update CHANGELOG.md
- [ ] 3.4 Run tests and confirm they pass
