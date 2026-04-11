## 1. Struct destructuring in let
- [ ] 1.1 Verify parser accepts `let TypeName { field1, field2 } = expr`
- [ ] 1.2 If not, add struct pattern support in let statement parsing
- [ ] 1.3 In HIR: desugar struct pattern into field-by-field extraction
- [ ] 1.4 In MIR/codegen: emit extractvalue/GEP for each destructured field binding

## 2. Tuple destructuring in let
- [ ] 2.1 Verify parser accepts `let (a, b) = expr`
- [ ] 2.2 If not, add tuple pattern support in let statement parsing
- [ ] 2.3 In HIR/MIR: desugar tuple pattern into positional extraction

## 3. Tail (mandatory)
- [ ] 3.1 Add tests: `let Point { x, y } = get_point()` binds x and y
- [ ] 3.2 Add tests: `let ParsedExpr { expr, pos } = parse()!` with error propagation
- [ ] 3.3 Add tests: `let (a, b) = pair` tuple destructuring
- [ ] 3.4 Update CHANGELOG.md
- [ ] 3.5 Run tests and confirm they pass
