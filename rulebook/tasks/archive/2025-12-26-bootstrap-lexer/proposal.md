# Proposal: Bootstrap Lexer

## Why

The lexer is the first stage of the TML compiler pipeline, responsible for converting source code into tokens. This is a foundational component that all subsequent compiler stages depend on. Without a correct and efficient lexer, the parser cannot function. The bootstrap compiler requires a complete lexer implementation in C++ that handles all TML tokens including keywords, operators, literals, identifiers, and handles Unicode correctly. This is critical for achieving self-hosting capability.

## What Changes

### New Components

1. **Token Types** (`src/lexer/token.hpp`)
   - Complete enumeration of all TML tokens
   - Token structure with position tracking (line, column, offset)
   - Token value storage (string, number, etc.)

2. **Lexer Core** (`src/lexer/lexer.hpp`, `src/lexer/lexer.cpp`)
   - Character stream handling with lookahead
   - Unicode support (UTF-8)
   - Keyword recognition
   - Operator tokenization (including multi-character operators)
   - String literal parsing (with escape sequences)
   - Number literal parsing (integers, floats, hex, binary, octal)
   - Comment handling (single-line `//` and multi-line `/* */`)
   - Error recovery and reporting

3. **Source Location** (`src/lexer/source.hpp`)
   - Source file abstraction
   - Position tracking
   - Span representation for error messages

### Token Categories

Based on `docs/specs/02-LEXICAL.md`:
- **Keywords**: `func`, `type`, `let`, `var`, `const`, `if`, `then`, `else`, `when`, `loop`, `return`, `break`, `continue`, `import`, `module`, `public`, `private`, `mut`, `ref`, `and`, `or`, `not`, `in`, `is`, `as`, `async`, `await`, `caps`, `extend`, `implement`, `trait`, `where`, `unsafe`, `do`
- **Operators**: `+`, `-`, `*`, `/`, `%`, `**`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `->`, `!`, `?`, `&`, `|`, `^`, `~`, `<<`, `>>`, `>>>`, `.`, `:`, `::`, `,`, `;`, `@`
- **Delimiters**: `(`, `)`, `[`, `]`, `{`, `}`
- **Literals**: Integers, Floats, Strings, Characters, Booleans

## Impact

- **Affected specs**: 02-LEXICAL.md, 16-COMPILER-ARCHITECTURE.md
- **Affected code**: New `src/lexer/` directory
- **Breaking change**: NO (new component)
- **User benefit**: Enables TML source code to be tokenized for parsing

## Dependencies

- None (first component)

## Success Criteria

1. All token types from spec are recognized
2. Unicode identifiers work correctly
3. All escape sequences in strings are handled
4. Error messages include accurate line/column information
5. Performance: >1M tokens/second on modern hardware
6. Test coverage ≥95%
