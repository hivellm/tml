# Tasks: TML Token & AST Type Definitions

**Status**: Planned (0/24)
**Depends on**: phase12 (Phase 0 foundation complete)
**Blocks**: phase13b (lexer), phase13c (parser)
**Duration**: 2 weeks
**Risk**: Low — pure data structure definitions, no algorithms

---

## Phase 1: Source & Span Types (3 items)

- [ ] 1.1 Create `compiler-tml/src/source.tml` — `Source` type (file path, content as Str, line offsets)
- [ ] 1.2 Implement `SourceSpan` type (source ref, start offset, end offset, line, column)
- [ ] 1.3 Implement `Span::merge(other: Span) -> Span` for combining spans across nodes

## Phase 2: Token Types (5 items)

- [ ] 2.1 Create `compiler-tml/src/token.tml` — `TokenKind` enum with all ~93 variants (keywords, operators, literals, punctuation)
- [ ] 2.2 Implement `Token` struct: kind, value (Str), span (SourceSpan)
- [ ] 2.3 Implement keyword lookup: `Str -> Maybe[TokenKind]` via HashMap for ~40 keywords
- [ ] 2.4 Implement `TokenKind.is_operator()`, `is_keyword()`, `is_literal()` helper methods
- [ ] 2.5 Test: verify all 93 token kinds are defined, keyword lookup works for all keywords

## Phase 3: AST Core Types (4 items)

- [ ] 3.1 Create `compiler-tml/src/ast/mod.tml` — module root with re-exports
- [ ] 3.2 Create `compiler-tml/src/ast/common.tml` — `Visibility` enum (Pub/Private), `Mutability`, `AstId` (I64)
- [ ] 3.3 Create `compiler-tml/src/ast/types.tml` — ~16 type expression nodes (`TypeExpr` enum: Named, Ref, Ptr, Array, Tuple, Func, etc.)
- [ ] 3.4 Create `compiler-tml/src/ast/patterns.tml` — ~23 pattern nodes (`Pattern` enum: Ident, Literal, Struct, Tuple, Enum, Wildcard, etc.)

## Phase 4: AST Expression & Statement Nodes (6 items)

- [ ] 4.1 Create `compiler-tml/src/ast/exprs.tml` — `Expr` enum with ~54 variants (Binary, Unary, Call, MethodCall, Field, Index, If, When, Loop, Block, Closure, etc.)
- [ ] 4.2 Each variant wraps data in `Heap[T]` for recursive types (e.g., `Binary(Heap[BinaryExpr])`)
- [ ] 4.3 Create `compiler-tml/src/ast/stmts.tml` — `Stmt` enum with ~8 variants (Let, Expr, Return, Break, Continue, While, For, Assign)
- [ ] 4.4 Create `compiler-tml/src/ast/decls.tml` — `Decl` enum with ~24 variants (Func, Struct, Enum, Behavior, Impl, Use, Const, TypeAlias, etc.)
- [ ] 4.5 Create `compiler-tml/src/ast/oop.tml` — OOP-related AST nodes: ImplBlock, BehaviorDecl, MethodDecl, AssocType (~49 struct types)
- [ ] 4.6 Create `compiler-tml/src/ast/module.tml` — `Module` struct: name, path, declarations list, imports

## Phase 5: Serialization Bridge (4 items)

- [ ] 5.1 Implement `Token.serialize(writer: ref BinaryWriter)` and `Token.deserialize(reader: ref BinaryReader)` using phase12e serializers
- [ ] 5.2 Implement `Module.serialize(writer: ref BinaryWriter)` — recursive AST walk serializing each node
- [ ] 5.3 Test: round-trip Token list (serialize → deserialize → compare)
- [ ] 5.4 Test: round-trip Module (serialize → deserialize → compare all fields)

## Phase 6: Verification (2 items)

- [ ] 6.1 Verify node count: TML AST has same number of variants as C++ AST for each category
- [ ] 6.2 `mcp__tml__check` on all new files — zero type errors
