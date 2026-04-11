## 1. Struct destructuring in let
- [x] 1.1 Verify parser accepts `let TypeName { field1, field2 } = expr` — passes type-checker
- [x] 1.2 Parser already supports struct patterns in let (parser_stmt.cpp:77 uses parse_pattern())
- [x] 1.3 In HIR: desugar struct pattern into field-by-field extraction — already done
- [x] 1.4 In MIR/codegen: emit extractvalue/GEP for each destructured field binding — tested: `let Pair { first: a, second: b } = p` outputs correct values (42, 77)

## 2. Tuple destructuring in let
- [ ] 2.1 Verify parser accepts `let (a, b) = expr` — needs testing
- [ ] 2.2 If not, add tuple pattern support in let statement parsing
- [ ] 2.3 In HIR/MIR: desugar tuple pattern into positional extraction

## 3. Tail (mandatory)
- [x] 3.1 Runtime verified: `let Pair { first: a, second: b } = p` binds a=42, b=77
- [ ] 3.2 Add dedicated test file for destructuring
- [ ] 3.3 Update CHANGELOG.md
- [ ] 3.4 Run tests and confirm they pass
