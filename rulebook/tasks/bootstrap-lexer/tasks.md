# Tasks: Bootstrap Lexer

## Progress: 0% (0/24 tasks complete)

## 1. Setup Phase
- [ ] 1.1 Create `src/lexer/` directory structure
- [ ] 1.2 Set up CMake configuration for lexer module
- [ ] 1.3 Create base header files with include guards

## 2. Core Types Phase
- [ ] 2.1 Implement `TokenKind` enum with all token types
- [ ] 2.2 Implement `SourceLocation` struct (line, column, offset)
- [ ] 2.3 Implement `Span` struct (start, end locations)
- [ ] 2.4 Implement `Token` struct (kind, value, span)
- [ ] 2.5 Implement `SourceFile` class for file handling

## 3. Lexer Core Phase
- [ ] 3.1 Implement character stream with UTF-8 support
- [ ] 3.2 Implement lookahead mechanism (peek, peek2)
- [ ] 3.3 Implement `advance()` and `consume()` methods
- [ ] 3.4 Implement whitespace and newline handling
- [ ] 3.5 Implement comment skipping (// and /* */)

## 4. Token Recognition Phase
- [ ] 4.1 Implement keyword recognition with hash table
- [ ] 4.2 Implement identifier lexing (Unicode support)
- [ ] 4.3 Implement integer literal lexing (dec, hex, bin, oct)
- [ ] 4.4 Implement float literal lexing (with exponent)
- [ ] 4.5 Implement string literal lexing (with escapes)
- [ ] 4.6 Implement character literal lexing
- [ ] 4.7 Implement operator lexing (including multi-char)

## 5. Error Handling Phase
- [ ] 5.1 Implement `LexerError` class with error codes
- [ ] 5.2 Implement error recovery (skip to next valid token)
- [ ] 5.3 Implement error message formatting with context

## 6. Testing Phase
- [ ] 6.1 Write unit tests for all token types
- [ ] 6.2 Write unit tests for edge cases (Unicode, escapes)
- [ ] 6.3 Write integration tests with sample TML files
- [ ] 6.4 Verify test coverage ≥95%

## 7. Documentation Phase
- [ ] 7.1 Document public API in header files
- [ ] 7.2 Update CHANGELOG.md with lexer implementation
