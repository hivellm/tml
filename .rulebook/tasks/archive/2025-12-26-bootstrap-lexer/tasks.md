# Tasks: Bootstrap Lexer

## Progress: 100% (24/24 tasks complete)

## 1. Setup Phase
- [x] 1.1 Create `src/lexer/` directory structure
- [x] 1.2 Set up CMake configuration for lexer module
- [x] 1.3 Create base header files with include guards

## 2. Core Types Phase
- [x] 2.1 Implement `TokenKind` enum with all token types
- [x] 2.2 Implement `SourceLocation` struct (line, column, offset)
- [x] 2.3 Implement `Span` struct (start, end locations)
- [x] 2.4 Implement `Token` struct (kind, value, span)
- [x] 2.5 Implement `SourceFile` class for file handling

## 3. Lexer Core Phase
- [x] 3.1 Implement character stream with UTF-8 support
- [x] 3.2 Implement lookahead mechanism (peek, peek2)
- [x] 3.3 Implement `advance()` and `consume()` methods
- [x] 3.4 Implement whitespace and newline handling
- [x] 3.5 Implement comment skipping (// and /* */)

## 4. Token Recognition Phase
- [x] 4.1 Implement keyword recognition with hash table
- [x] 4.2 Implement identifier lexing (Unicode support)
- [x] 4.3 Implement integer literal lexing (dec, hex, bin, oct)
- [x] 4.4 Implement float literal lexing (with exponent)
- [x] 4.5 Implement string literal lexing (with escapes)
- [x] 4.6 Implement character literal lexing
- [x] 4.7 Implement operator lexing (including multi-char)

## 5. Error Handling Phase
- [x] 5.1 Implement `LexerError` class with error codes
- [x] 5.2 Implement error recovery (skip to next valid token)
- [x] 5.3 Implement error message formatting with context

## 6. Testing Phase
- [x] 6.1 Write unit tests for all token types
- [x] 6.2 Write unit tests for edge cases (Unicode, escapes)
- [x] 6.3 Write integration tests with sample TML files
- [x] 6.4 Verify test coverage ≥95%

## 7. Documentation Phase
- [x] 7.1 Document public API in header files
- [x] 7.2 Update CHANGELOG.md with lexer implementation

## Implementation Notes

**Completed**: Lexer fully modularized into 7 modules:
- `lexer_core.cpp` - Main lexer class and control flow
- `lexer_ident.cpp` - Identifier and keyword recognition
- `lexer_number.cpp` - Integer and float literal parsing
- `lexer_operator.cpp` - Operator token recognition
- `lexer_string.cpp` - String and character literal parsing
- `lexer_token.cpp` - Token creation and utilities
- `lexer_utils.cpp` - UTF-8 and helper functions

**Status**: Fully functional, tested, and integrated into compiler pipeline.
