# Proposal: TML Parser — Rewrite in TML

**Task**: phase13c_tml-parser
**Status**: Planned
**Priority**: P0
**Estimated effort**: 5-6 weeks
**Risk**: Medium

## Problem

The C++ parser is the largest and most complex component of the frontend at ~6,327 LOC
across ten files. It uses a Pratt parser (precedence climbing) for expressions and
recursive descent for declarations. Porting it to TML is required before the TML
frontend can be wired into the compiler pipeline in phase13d.

The risk is Medium rather than Low because Pratt parsers require careful handling of
precedence levels and associativity. A single off-by-one in a precedence binding power
produces a subtly wrong AST that may only manifest as a type error deep in the
pipeline. The differential test (task 8.2) is the primary safety net.

## Proposed Solution

Port the C++ parser faithfully to TML across seven source files, implementing the
same Pratt expression parser and recursive descent declaration parser. Target:
~4,100 LOC TML.

**Parser infrastructure** (`parser/mod.tml`) — `Parser` struct holding `List[Token]`,
current position, lookahead token, and an error accumulator `List[ParseError]`.
Helpers: `advance()`, `peek()`, `peek_ahead(n)`, `expect(kind)`, `match_token(kind)`,
`at_end()`. Error recovery via `synchronize()` — consumes tokens until a statement
boundary token is found (`;`, `}`, `func`, `let`, `return`, etc.).

**Type expression parser** (`parse_type.tml`) — named types, generic applications
(`List[I32]`), ref/mut-ref (`ref T`, `mut ref T`), raw pointers (`*T`), function
types (`Func(I32) -> Bool`), tuple types `(I32, Str)`, fixed-size arrays `[I32; 10]`,
and the optional shorthand `T?` (sugar for `Maybe[T]`).

**Pattern parser** (`parse_pattern.tml`) — identifier bindings, literals, wildcard
`_`, struct patterns, tuple patterns, enum variant patterns, nested patterns, or-patterns
`A | B`, and guards `if condition` in `when` arms.

**Pratt expression parser** (`parse_expr.tml`) — the core of the parser. Precedence
table mirrors C++ exactly:

```
assign (1) < or (2) < and (3) < eq (4) < cmp (5)
< add (6) < mul (7) < unary (8) < call/field (9)
```

Prefix parselets: unary `-` / `not` / `ref` / `mut ref`, literals, identifiers,
parenthesized expressions, blocks `{ }`, `if/else`, `when` (match), `loop`,
closures `do(x) expr`, template literals. Infix parselets: binary arithmetic/logical/
comparison/assignment operators, `as` type cast. Postfix: call `()`, index `[]`,
field access `.name`, method call `.method()`, optional chain `?.`.

**Statement parser** (`parse_stmt.tml`) — `let` bindings with optional type
annotation and initializer, expression statements, `return expr?`, `break label?`,
`continue label?`, assignment `lhs = rhs`, compound assignment `lhs += rhs`.

**Declaration parser** (`parse_decl.tml`) — `func` with generic params, where
clauses, param list, return type, body; `struct` and `enum` with generic params and
fields/variants; `behavior` declarations with method signatures, associated types,
and default method bodies; `impl Behavior for Type` and inherent `impl Type` blocks;
`use` import paths; `const` and `type` alias declarations.

**Module parser** (`parse_module.tml`) — top-level entry point
`parse_module(tokens: List[Token]) -> Outcome[Module, List[ParseError]]`.
Iterates declarations at the module level and returns the complete `Module` AST.

## Key Decisions

- **Pratt parser with integer binding powers** — each token kind that can appear as
  an infix operator has a `(left_bp, right_bp)` pair stored in a `HashMap`. The main
  loop calls `parse_expr(min_bp)` recursively. This matches the C++ implementation
  exactly and makes precedence auditable.

- **Error recovery via `synchronize()`** — on a parse error the parser records the
  `ParseError` and calls `synchronize()` to skip ahead, then continues. This allows
  reporting multiple errors in a single parse pass rather than stopping at the first.
  Errors are returned alongside any partial AST.

- **All AST nodes carry `SourceSpan`** — the parser records the start span before
  parsing each construct and merges with the end span using `Span::merge` from
  phase13a. Crucial for diagnostics after deserialization on the C++ side.

- **Identical AST structure to C++** — the serialized AST produced by the TML parser
  must deserialize identically on the C++ side. Any structural divergence from the
  C++ parser's output is a bug. The differential test (task 8.2) enforces this.

- **No semantic analysis in the parser** — the parser is strictly syntactic. Name
  resolution, type checking, and import resolution all happen later in the C++ pipeline.

## Files to Create/Modify

| File | Notes |
|------|-------|
| `compiler-tml/src/parser/mod.tml` | `Parser` struct, helpers, `synchronize()` |
| `compiler-tml/src/parser/parse_expr.tml` | Pratt entry point, precedence table, all parselets |
| `compiler-tml/src/parser/parse_type.tml` | Type expression parsing |
| `compiler-tml/src/parser/parse_pattern.tml` | Pattern parsing for `when` and `let` |
| `compiler-tml/src/parser/parse_stmt.tml` | Statement parsing |
| `compiler-tml/src/parser/parse_decl.tml` | Declaration parsing |
| `compiler-tml/src/parser/parse_module.tml` | Module-level entry point |

## Success Criteria

- `mcp__tml__check` passes with zero errors on all parser files.
- Each language construct parses to the correct AST shape in isolation: `func`,
  `struct`, `enum`, `impl`, `when` expression, closure (task 8.1).
- Differential test: parsing 20 stdlib files with both C++ and TML parsers produces
  byte-identical serialized AST output (task 8.2).

## Dependencies

- **Depends on**: phase13a (all AST types), phase13b (lexer produces `List[Token]`).
- **Blocks**: phase13d (frontend integration, which wires TML parser into the
  compiler query system).
