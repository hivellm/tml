# Proposal: TML Lexer — Rewrite in TML

**Task**: phase13b_tml-lexer
**Status**: Planned
**Priority**: P0
**Estimated effort**: 3-4 weeks
**Risk**: Low

## Why

The TML self-hosting compiler requires a lexer written in TML to tokenize source code. This is the first executable component of the self-hosting frontend and unblocks the parser (phase13c). Without it, Phase 13 cannot proceed.

## Problem

The C++ lexer (`compiler/src/lexer/lexer.cpp` and related files, ~2,830 LOC across
nine files) must be rewritten in TML as the first executable step of self-hosting the
compiler frontend. Until the TML lexer exists and is verified to produce identical
token streams, the parser cannot be ported and Phase 13 stalls.

The lexer is the lowest-risk component to port first: it is a pure function from
source text to a flat list of tokens, with no cross-module state, no recursive types,
and a completely deterministic specification.

## Proposed Solution

Implement a faithful TML port of the C++ lexer, split into six source files mirroring
the C++ structure. The lexer operates character-by-character (no regex) and produces
`List[Token]` using the `TokenKind` enum defined in phase13a.

**Core loop** (`lexer/mod.tml`, `core.tml`) — `Lexer` struct holding source text,
current byte position, line and column counters, and the accumulated token list.
`Lexer.tokenize()` returns `Outcome[List[Token], LexError]`. Whitespace and newline
skipping advances line/column counters correctly.

**Comments** — single-line `//` (skip to EOL), doc comments `///` (emit
`DocComment` token with content), block comments `/* */` with nesting support (a
nested `/*` inside a block comment requires a matching `*/`).

**Identifiers & keywords** (`identifiers.tml`) — scan `[a-zA-Z_][a-zA-Z0-9_]*`,
then check the keyword `HashMap` from `token.tml`. If matched, emit the keyword
token kind; otherwise emit `Ident`.

**Operators** (`operators.tml`) — longest-match scanning for multi-char operators
(`->`, `=>`, `::`, `..`, `?.`, `==`, `!=`, `<=`, `>=`, `+=`, `-=`, `*=`, `/=`,
`|>`, etc.) before falling back to single-char operators.

**Number literals** (`numbers.tml`) — decimal, hex (`0x`), octal (`0o`), binary
(`0b`), all with `_` separator support; floats with optional exponent (`1.5e-3`).

**String literals** (`strings.tml`) — standard double-quoted strings with all escape
sequences (`\n \t \\ \" \r \0 \xNN \uNNNN`); raw strings `r"..."` and `r#"..."#`;
template literals (backtick strings) emitting `TemplateStart`, `TemplateMiddle`,
`TemplateEnd` tokens at interpolation `{` / `}` boundaries, with correct brace
depth tracking for nested expressions.

Target: ~1,800 LOC TML.

## Key Decisions

- **Character-by-character scanning, no regex** — matches C++ implementation
  exactly, avoids a regex dependency, and keeps the lexer auditable line-by-line.

- **Keyword HashMap at construction** — the 40 TML keywords are inserted once when
  `Lexer.new()` is called. Lookup is O(1) per identifier token.

- **Template literal brace depth tracking** — a `brace_depth: I64` counter is
  maintained inside template literal scanning. The lexer emits `TemplateMiddle`
  only when a `}` returns depth to zero, not for nested `{}` inside the
  interpolation expression. This matches C++ behavior exactly.

- **All escape sequences must match C++ byte-for-byte** — the differential test
  (task 6.4) will catch any mismatch; the implementation must handle `\xNN` as a
  raw byte and `\uNNNN` as a UTF-8 encoded code point.

- **LexError carries span** — errors include the `SourceSpan` of the offending
  character so the C++ side can report them with correct line/column information.

## Files to Create/Modify

| File | Notes |
|------|-------|
| `compiler-tml/src/lexer/mod.tml` | `Lexer` type, `tokenize()`, module root |
| `compiler-tml/src/lexer/core.tml` | Main dispatch loop, whitespace, newlines |
| `compiler-tml/src/lexer/identifiers.tml` | Identifier + keyword scanning |
| `compiler-tml/src/lexer/operators.tml` | Multi-char and single-char operators |
| `compiler-tml/src/lexer/numbers.tml` | Integer and float literal scanning |
| `compiler-tml/src/lexer/strings.tml` | String, raw string, template literal scanning |

## Success Criteria

- `mcp__tml__check` passes on all lexer files with zero errors.
- `func main() -> I32 { return 0 }` tokenizes to the exact expected sequence
  (task 6.1).
- All ~93 `TokenKind` variants are reachable via the lexer; 100% token-kind
  coverage test passes (task 6.2).
- String edge cases pass: empty string, all escape sequences, raw strings, template
  literals with nested braces (task 6.3).
- Differential test: tokenizing 20 stdlib `.tml` files with both C++ and TML lexers
  produces identical token lists (task 6.4).

## Dependencies

- **Depends on**: phase13a — `Token`, `TokenKind`, `SourceSpan` must be defined.
- **Blocks**: phase13c (parser consumes `List[Token]`).
