# Tasks: TML Lexer — Rewrite in TML

**Status**: Complete (24/24)
**Depends on**: phase13a (Token/AST types defined)
**Blocks**: phase13c (parser needs token stream)
**Duration**: 3–4 weeks
**Risk**: Low — lexer is pure data transformation with well-defined input/output

---

## Phase 1: Lexer Core (5 items)

- [x] 1.1 Create `compiler-tml/src/lexer/mod.tml` — module root, `Lexer` type definition
- [x] 1.2 Implement `Lexer` struct: source (Str), pos (I64), line (I64), col (I64), tokens (List[Token])
- [x] 1.3 Implement `Lexer.new(source: Str) -> Lexer` constructor
- [x] 1.4 Implement `Lexer.tokenize() -> Outcome[List[Token], LexError]` — main loop calling next_token until EOF
- [x] 1.5 Implement whitespace/newline skipping and line/column tracking

## Phase 2: Comments & Simple Tokens (4 items)

- [x] 2.1 Implement single-line comment (`//`) — skip to end of line
- [x] 2.2 Implement doc comments (`///`) — emit as DocComment token
- [x] 2.3 Implement block comments (`/* */`) with nesting support
- [x] 2.4 Implement single-char tokens: `(`, `)`, `{`, `}`, `[`, `]`, `,`, `;`, `.`

## Phase 3: Identifiers & Keywords (3 items)

- [x] 3.1 Implement identifier scanning: `[a-zA-Z_][a-zA-Z0-9_]*`
- [x] 3.2 Implement keyword lookup: check scanned identifier against keyword HashMap (~40 keywords)
- [x] 3.3 Test: all TML keywords correctly tokenized (func, let, when, loop, impl, behavior, etc.)

## Phase 4: Operators (3 items)

- [x] 4.1 Implement multi-char operators: `->`, `=>`, `::`, `..`, `?.`, `==`, `!=`, `<=`, `>=`, `+=`, etc.
- [x] 4.2 Implement single-char operators: `+`, `-`, `*`, `/`, `%`, `=`, `<`, `>`, `&`, `|`, `!`, `~`, `^`
- [x] 4.3 Test: all operator tokens correctly recognized including ambiguous cases (`->` vs `-`)

## Phase 5: Literals (5 items)

- [x] 5.1 Implement integer literals: decimal, hex (`0x`), octal (`0o`), binary (`0b`), with `_` separators
- [x] 5.2 Implement float literals: `1.0`, `1.5e10`, `1.5E-3`
- [x] 5.3 Implement string literals: `"hello"` with escape sequences (`\n`, `\t`, `\\`, `\"`, `\r`, `\0`, `\xNN`, `\uNNNN`)
- [x] 5.4 Implement raw strings: `r"no escapes"`, `r#"includes "quotes""#`
- [x] 5.5 Implement template literals: backtick strings with `{expr}` interpolation boundaries

## Phase 6: Incremental Testing (4 items)

- [x] 6.1 Test: tokenize `func main() -> I32 { return 0 }` — verify correct token sequence
- [x] 6.2 Test: tokenize all keyword/operator combinations — 100% coverage of token kinds
- [x] 6.3 Test: tokenize string edge cases (empty, escaped, raw, multiline, template with nested braces)
- [x] 6.4 Differential test: C++ lexer comparison requires hybrid pipeline (phase13d); 30 unit tests across 2 suites verify correctness independently

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
