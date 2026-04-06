# Proposal: TML Token & AST Type Definitions

**Task**: phase13a_tml-token-ast-types
**Status**: Planned
**Priority**: P0
**Estimated effort**: 2 weeks
**Risk**: Low

## Problem

Before a single line of lexer or parser code can be written in TML, the data types
those components operate on must exist. The C++ compiler defines its token and AST
types across nine header files totalling ~3,433 LOC. Phase 13 (self-hosting the
compiler frontend) cannot start until TML equivalents are in place.

The dependency is hard: phase13b (lexer) needs `Token` and `TokenKind`; phase13c
(parser) needs all AST node types; phase13d (integration) needs serialization for the
hybrid pipeline bridge. Nothing in Phase 13 makes forward progress without this
foundation.

## Proposed Solution

Create the `compiler-tml/src/` directory tree with three categories of files:

**Source & span types** (`source.tml`) — `Source` (file path, content as Str, line
offsets) and `SourceSpan` (start/end byte offsets, line, column). Every AST node
stores a `SourceSpan` so the C++ type checker can produce precise diagnostics after
deserialization.

**Token types** (`token.tml`) — `TokenKind` flat enum with all ~93 variants matching
the C++ `TokenKind` exactly (same names for serialization compatibility). `Token`
bundles kind, raw text value, and span. A keyword `HashMap` is pre-populated at
construction time and exposed as `keyword_lookup(name: Str) -> Maybe[TokenKind]`.

**AST node types** (`ast/` subdirectory, eight modules) — covering common helpers,
type expressions (~16 variants), patterns (~23 variants), expressions (~54 variants),
statements (~8 variants), declarations (~24 variants), OOP/impl nodes (~49 structs),
and the top-level `Module` struct. Every recursive child is wrapped in `Heap[T]` to
satisfy TML ownership rules without lifetime annotations.

Serialization methods (`serialize` / `deserialize` via phase12e `BinaryWriter` /
`BinaryReader`) are added to `Token` and `Module` for the hybrid pipeline bridge.

Target: ~2,200 LOC TML (C++ headers are 3,433 LOC including verbose template
boilerplate absent from TML).

## Key Decisions

- **`Heap[T]` for all recursive nodes** — avoids stack-size blowup and mirrors the
  C++ AST's use of `std::unique_ptr`. Every `Expr` variant containing child
  expressions wraps its payload struct: `Binary(Heap[BinaryExpr])`, etc.

- **Flat `TokenKind` enum** — no sub-enums; matches C++ exactly. Variant ordering
  is a serialization contract: reordering variants is a breaking change requiring a
  lockstep C++ deserializer update.

- **Spans on every node** — even leaf nodes (identifiers, literals) carry a
  `SourceSpan`. The C++ side uses spans for all diagnostics after deserialization;
  dropping them would break error reporting.

- **Data types only, no behavior impls** — `Display`, `Debug`, `Clone` impls are
  out of scope. Only serialization bridge methods are required, keeping this task
  focused and low-risk.

## Files to Create/Modify

| File | Notes |
|------|-------|
| `compiler-tml/src/source.tml` | `Source`, `SourceSpan`, `Span::merge` |
| `compiler-tml/src/token.tml` | `TokenKind` (93 variants), `Token`, `keyword_lookup` |
| `compiler-tml/src/ast/mod.tml` | Re-exports for all ast submodules |
| `compiler-tml/src/ast/common.tml` | `Visibility`, `Mutability`, `AstId` |
| `compiler-tml/src/ast/types.tml` | `TypeExpr` enum, ~16 type node variants |
| `compiler-tml/src/ast/patterns.tml` | `Pattern` enum, ~23 pattern variants |
| `compiler-tml/src/ast/exprs.tml` | `Expr` enum, ~54 expression variants |
| `compiler-tml/src/ast/stmts.tml` | `Stmt` enum, ~8 statement variants |
| `compiler-tml/src/ast/decls.tml` | `Decl` enum, ~24 declaration variants |
| `compiler-tml/src/ast/oop.tml` | `ImplBlock`, `BehaviorDecl`, method nodes (~49 structs) |
| `compiler-tml/src/ast/module.tml` | `Module` struct + serialize/deserialize |

## Success Criteria

- `mcp__tml__check` passes with zero errors on all new files.
- All ~93 `TokenKind` variants are defined; `keyword_lookup` returns the correct
  kind for every TML keyword and `Nothing` for non-keywords.
- All ~174 AST node types are defined; variant counts match C++ header counts per
  category (task 6.1 verification).
- `Token` and `Module` round-trip through `serialize` -> `deserialize` with no data
  loss (tasks 5.3, 5.4).

## Dependencies

- **Depends on**: phase12 — `BinaryWriter`, `BinaryReader`, and the
  `compiler-tml/` project scaffold must already exist.
- **Blocks**: phase13b (lexer), phase13c (parser), phase13d (frontend integration).
