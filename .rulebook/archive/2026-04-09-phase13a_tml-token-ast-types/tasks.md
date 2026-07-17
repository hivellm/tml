# Tasks: TML Token & AST Type Definitions

**Status**: Complete (24/24)
**Depends on**: phase12 (Phase 0 foundation complete)
**Blocks**: phase13b (lexer), phase13c (parser)
**Duration**: 2 weeks
**Risk**: Low — pure data structure definitions, no algorithms

---

## Phase 1: Source & Span Types (3 items)

- [x] 1.1 Create `compiler-tml/src/source.tml` — `Source` type (file path, content as Str, line offsets)
- [x] 1.2 Implement `SourceSpan` type (source ref, start offset, end offset, line, column)
- [x] 1.3 Implement `Span::merge(other: Span) -> Span` for combining spans across nodes

## Phase 2: Token Types (5 items)

- [x] 2.1 Create `compiler-tml/src/token.tml` — `TokenKind` enum with all 139 variants (keywords, operators, literals, punctuation) — order locked to compiler/include/lexer/token.hpp
- [x] 2.2 Implement `Token` struct: kind, value (Str), span (SourceSpan)
- [x] 2.3 Implement keyword lookup: `Str -> Maybe[TokenKind]` for all keywords + true/false/null
- [x] 2.4 Implement `token_kind_is_operator/is_keyword/is_literal` helper functions
- [x] 2.5 Verified: all 139 token kinds defined, keyword lookup covers every Kw* variant, `tml check` clean

## Phase 3: AST Core Types (4 items)

- [x] 3.1 Create `compiler-tml/src/ast/mod.tml` — module root with re-exports for Visibility, Mutability, AstId, LiteralKind, TypeExpr, Pattern, Expr, Stmt, Decl, Module
- [x] 3.2 Create `compiler-tml/src/ast/common.tml` — `Visibility` (Private/Public/PubCrate), `Mutability` (Immutable/Mutable), `AstId` (I64), `LiteralKind` (6 variants)
- [x] 3.3 `TypeExpr` enum (10 variants: Named, Ref, Ptr, Array, Slice, Tuple, Func, Infer, Dyn, ImplBehavior) — in `ast/nodes.tml` (merged with patterns/module per cyclic import structure)
- [x] 3.4 `Pattern` enum (9 variants: Wildcard, Ident, Literal, Tuple, Struct, Enum, Or, Range, Array) — in `ast/nodes.tml`

## Phase 4: AST Expression & Statement Nodes (6 items)

- [x] 4.1 Create `compiler-tml/src/ast/exprs.tml` — `Expr` enum with 35 variants (all C++ variants matched: Literal, Ident, Unary, Binary, Call, MethodCall, Field, Index, Tuple, Array, Struct, If, Ternary, IfLet, When, Loop, While, For, Block, Return, Break, Continue, Closure, Range, Cast, Is, Try, Await, Throw, Path, Lowlevel, InterpolatedString, TemplateLiteral, Base, New)
- [x] 4.2 All recursive variants wrap data in `Heap[T]` (e.g., `Binary(BinaryExpr)` where `BinaryExpr.left: Heap[Expr]`)
- [x] 4.3 Create `compiler-tml/src/ast/stmts.tml` — `Stmt` enum with 5 variants (Let, Var, LetElse, Expr, Decl)
- [x] 4.4 Create `compiler-tml/src/ast/decls.tml` — `Decl` enum with 13 variants (Func, Struct, Union, Enum, Trait, Impl, TypeAlias, Const, Use, Mod, Class, Interface, Namespace)
- [x] 4.5 OOP AST nodes (ClassDecl, InterfaceDecl, NamespaceDecl + ClassField) — merged into `ast/decls.tml`; `ast/oop.tml` is a re-export shim
- [x] 4.6 `Module` struct (name, docs, decls, span) + `module_new` constructor — in `ast/nodes.tml`

## Phase 5: Serialization Bridge (4 items)

- [x] 5.1 Implement `write_token` / `read_token` / `read_token_tag` in `ast/serial.tml` using BinaryWriter/BinaryReader
- [x] 5.2 Implement `write_module` / `read_module` with header (magic 0x4D4F4420, version 1.0) + name + docs + span + decl count
- [x] 5.3 Test: module header round-trip (`serial_roundtrip.test.tml` — magic value, bad magic, wire format, full round-trip)
- [x] 5.4 Test: AST node round-trip (`ast_roundtrip.test.tml`, `node_roundtrip.test.tml` — pre-existing; token round-trip blocked by SourceLocation/TmlSourceLocation layout mismatch, tracked separately)

## Phase 6: Verification (2 items)

- [x] 6.1 Verified node count parity: TypeExpr 10/10, Pattern 9/9, Expr 35/35, Stmt 5/5, Decl 13/13 (all match C++)
- [x] 6.2 `tml check` on all 10 source files — zero type errors

## Tail (mandatory — enforced by rulebook v5.3.0)
- [x] Update or create documentation covering the implementation
- [x] Write tests covering the new behavior
- [x] Run tests and confirm they pass
