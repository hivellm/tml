# Tasks: TML Parser — Rewrite in TML

**Status**: Planned (0/25)
**Depends on**: phase13a (AST types), phase13b (lexer produces token stream)
**Blocks**: phase13d (frontend integration)
**Duration**: 5–6 weeks
**Risk**: Medium — Pratt parser requires careful precedence handling

---

## Phase 1: Parser Infrastructure (4 items)

- [ ] 1.1 Create `compiler-tml/src/parser/mod.tml` — module root, `Parser` type definition
- [ ] 1.2 Implement `Parser` struct: tokens (List[Token]), pos (I64), current (Token), errors (List[ParseError])
- [ ] 1.3 Implement helpers: `advance()`, `peek()`, `expect(kind)`, `match_token(kind)`, `at_end()`
- [ ] 1.4 Implement error recovery: `synchronize()` — skip tokens until statement boundary

## Phase 2: Type Expression Parser (3 items)

- [ ] 2.1 Create `compiler-tml/src/parser/parse_type.tml` — type expression parsing
- [ ] 2.2 Implement: named types (`I32`), generic types (`List[I32]`), ref types (`ref T`, `mut ref T`), pointer types (`*T`)
- [ ] 2.3 Implement: function types (`Func(I32) -> Bool`), tuple types (`(I32, Str)`), array types (`[I32; 10]`), Maybe/optional (`T?`)

## Phase 3: Pattern Parser (2 items)

- [ ] 3.1 Create `compiler-tml/src/parser/parse_pattern.tml` — pattern parsing for `when` and `let`
- [ ] 3.2 Implement: identifier, literal, wildcard (`_`), tuple, struct, enum variant, nested, or-pattern, guard (`if cond`)

## Phase 4: Expression Parser — Pratt (6 items)

- [ ] 4.1 Create `compiler-tml/src/parser/parse_expr.tml` — Pratt parser entry point
- [ ] 4.2 Implement precedence table: assign < or < and < eq < cmp < add < mul < unary < call < field
- [ ] 4.3 Implement prefix expressions: unary (`-`, `not`, `ref`, `mut ref`), literals, identifiers, parenthesized, block
- [ ] 4.4 Implement infix expressions: binary ops, comparison, logical, assignment, type cast (`as`)
- [ ] 4.5 Implement postfix expressions: call `()`, index `[]`, field `.name`, method `.method()`, optional chain `?.`
- [ ] 4.6 Implement complex expressions: `if/else`, `when` (match), `loop`, closures (`do(x) expr`), template literals

## Phase 5: Statement Parser (2 items)

- [ ] 5.1 Create `compiler-tml/src/parser/parse_stmt.tml` — statement parsing
- [ ] 5.2 Implement: `let` bindings, expression statements, `return`, `break`, `continue`, assignment, compound assignment

## Phase 6: Declaration Parser (5 items)

- [ ] 6.1 Create `compiler-tml/src/parser/parse_decl.tml` — top-level declaration parsing
- [ ] 6.2 Implement: `func` declarations (params, return type, body, generic params, where clauses)
- [ ] 6.3 Implement: `struct`, `enum` declarations (fields, variants, generic params)
- [ ] 6.4 Implement: `behavior` declarations (method signatures, associated types, default methods)
- [ ] 6.5 Implement: `impl` blocks (behavior impl, inherent impl), `use` imports, `const`, `type` alias

## Phase 7: Module Parser (1 item)

- [ ] 7.1 Create `compiler-tml/src/parser/parse_module.tml` — `parse_module(tokens: List[Token]) -> Outcome[Module, List[ParseError]]`

## Phase 8: Testing (2 items)

- [ ] 8.1 Test each construct individually: parse a func, a struct, an enum, an impl, a when expr, a closure — verify AST shape
- [ ] 8.2 Differential test: parse 20 stdlib files with C++ parser and TML parser — compare serialized AST output
