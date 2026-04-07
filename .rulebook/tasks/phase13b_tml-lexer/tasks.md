# Tasks: TML Lexer — Rewrite in TML

**Status**: Planned (0/24)
**Depends on**: phase13a (Token/AST types defined)
**Blocks**: phase13c (parser needs token stream)
**Duration**: 3–4 weeks
**Risk**: Low — lexer is pure data transformation with well-defined input/output

---

## Phase 1: Lexer Core (5 items)

- [ ] 1.1 Create `compiler-tml/src/lexer/mod.tml` — module root, `Lexer` type definition
- [ ] 1.2 Implement `Lexer` struct: source (Str), pos (I64), line (I64), col (I64), tokens (List[Token])
- [ ] 1.3 Implement `Lexer.new(source: Str) -> Lexer` constructor
- [ ] 1.4 Implement `Lexer.tokenize() -> Outcome[List[Token], LexError]` — main loop calling next_token until EOF
- [ ] 1.5 Implement whitespace/newline skipping and line/column tracking

## Phase 2: Comments & Simple Tokens (4 items)

- [ ] 2.1 Implement single-line comment (`//`) — skip to end of line
- [ ] 2.2 Implement doc comments (`///`) — emit as DocComment token
- [ ] 2.3 Implement block comments (`/* */`) with nesting support
- [ ] 2.4 Implement single-char tokens: `(`, `)`, `{`, `}`, `[`, `]`, `,`, `;`, `.`

## Phase 3: Identifiers & Keywords (3 items)

- [ ] 3.1 Implement identifier scanning: `[a-zA-Z_][a-zA-Z0-9_]*`
- [ ] 3.2 Implement keyword lookup: check scanned identifier against keyword HashMap (~40 keywords)
- [ ] 3.3 Test: all TML keywords correctly tokenized (func, let, when, loop, impl, behavior, etc.)

## Phase 4: Operators (3 items)

- [ ] 4.1 Implement multi-char operators: `->`, `=>`, `::`, `..`, `?.`, `==`, `!=`, `<=`, `>=`, `+=`, etc.
- [ ] 4.2 Implement single-char operators: `+`, `-`, `*`, `/`, `%`, `=`, `<`, `>`, `&`, `|`, `!`, `~`, `^`
- [ ] 4.3 Test: all operator tokens correctly recognized including ambiguous cases (`->` vs `-`)

## Phase 5: Literals (5 items)

- [ ] 5.1 Implement integer literals: decimal, hex (`0x`), octal (`0o`), binary (`0b`), with `_` separators
- [ ] 5.2 Implement float literals: `1.0`, `1.5e10`, `1.5E-3`
- [ ] 5.3 Implement string literals: `"hello"` with escape sequences (`\n`, `\t`, `\\`, `\"`, `\r`, `\0`, `\xNN`, `\uNNNN`)
- [ ] 5.4 Implement raw strings: `r"no escapes"`, `r#"includes "quotes""#`
- [ ] 5.5 Implement template literals: backtick strings with `{expr}` interpolation boundaries

## Phase 6: Incremental Testing (4 items)

- [ ] 6.1 Test: tokenize `func main() -> I32 { return 0 }` — verify correct token sequence
- [ ] 6.2 Test: tokenize all keyword/operator combinations — 100% coverage of token kinds
- [ ] 6.3 Test: tokenize string edge cases (empty, escaped, raw, multiline, template with nested braces)
- [ ] 6.4 Differential test: tokenize 20 stdlib files with BOTH C++ lexer and TML lexer — compare token lists

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
