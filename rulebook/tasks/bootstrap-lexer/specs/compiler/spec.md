# Lexer Specification

## Purpose

The lexer transforms TML source code into a stream of tokens for the parser. It handles Unicode identifiers, all literal types, operators, and provides accurate source location tracking for error messages.

## ADDED Requirements

### Requirement: Token Stream Generation
The lexer SHALL produce a sequential stream of tokens from source code input.

#### Scenario: Basic tokenization
Given a source file containing `let x = 42`
When the lexer processes the file
Then it produces tokens: `LET`, `IDENT("x")`, `ASSIGN`, `INT(42)`, `EOF`

#### Scenario: Multi-line tokenization
Given a source file with multiple lines
When the lexer processes the file
Then each token includes correct line and column information

### Requirement: Keyword Recognition
The system MUST recognize all TML keywords and distinguish them from identifiers.

#### Scenario: Keyword identification
Given the input `func if then else when`
When the lexer tokenizes the input
Then it produces `FUNC`, `IF`, `THEN`, `ELSE`, `WHEN` tokens (not identifiers)

#### Scenario: Keyword-like identifiers
Given the input `function iffy thename`
When the lexer tokenizes the input
Then it produces `IDENT("function")`, `IDENT("iffy")`, `IDENT("thename")`

### Requirement: Unicode Identifier Support
The lexer MUST support Unicode characters in identifiers according to UAX#31.

#### Scenario: Unicode identifiers
Given the input `let café = 1` or `let 变量 = 2`
When the lexer tokenizes the input
Then it correctly recognizes `café` and `变量` as valid identifiers

#### Scenario: Invalid Unicode start
Given the input `let 123abc = 1`
When the lexer tokenizes the input
Then it reports an error for invalid identifier start

### Requirement: Integer Literal Parsing
The lexer SHALL parse integer literals in decimal, hexadecimal, binary, and octal formats.

#### Scenario: Decimal integers
Given the input `42 1_000_000`
When the lexer tokenizes the input
Then it produces `INT(42)`, `INT(1000000)`

#### Scenario: Hexadecimal integers
Given the input `0xFF 0x1234_ABCD`
When the lexer tokenizes the input
Then it produces `INT(255)`, `INT(305441741)`

#### Scenario: Binary integers
Given the input `0b1010 0b1111_0000`
When the lexer tokenizes the input
Then it produces `INT(10)`, `INT(240)`

#### Scenario: Octal integers
Given the input `0o755 0o777`
When the lexer tokenizes the input
Then it produces `INT(493)`, `INT(511)`

### Requirement: Float Literal Parsing
The lexer SHALL parse floating-point literals including scientific notation.

#### Scenario: Basic floats
Given the input `3.14 0.5 10.0`
When the lexer tokenizes the input
Then it produces `FLOAT(3.14)`, `FLOAT(0.5)`, `FLOAT(10.0)`

#### Scenario: Scientific notation
Given the input `1e10 2.5E-3 1.0e+5`
When the lexer tokenizes the input
Then it produces correct float values with exponents

### Requirement: String Literal Parsing
The lexer MUST parse string literals with escape sequence support.

#### Scenario: Simple strings
Given the input `"hello" "world"`
When the lexer tokenizes the input
Then it produces `STRING("hello")`, `STRING("world")`

#### Scenario: Escape sequences
Given the input `"line1\nline2\ttab\\slash\"quote"`
When the lexer tokenizes the input
Then it correctly interprets `\n`, `\t`, `\\`, `\"`

#### Scenario: Unicode escapes
Given the input `"\u{1F600}"` (grinning face emoji)
When the lexer tokenizes the input
Then it produces the correct Unicode character

### Requirement: Operator Tokenization
The lexer SHALL recognize all TML operators including multi-character operators.

#### Scenario: Single-character operators
Given the input `+ - * / %`
When the lexer tokenizes the input
Then it produces `PLUS`, `MINUS`, `STAR`, `SLASH`, `PERCENT`

#### Scenario: Multi-character operators
Given the input `== != <= >= -> ** >>>`
When the lexer tokenizes the input
Then it produces `EQ_EQ`, `NOT_EQ`, `LT_EQ`, `GT_EQ`, `ARROW`, `STAR_STAR`, `GT_GT_GT`

#### Scenario: Compound assignment
Given the input `+= -= *= /= %=`
When the lexer tokenizes the input
Then it produces `PLUS_EQ`, `MINUS_EQ`, `STAR_EQ`, `SLASH_EQ`, `PERCENT_EQ`

### Requirement: Comment Handling
The lexer MUST skip comments and not produce tokens for them.

#### Scenario: Single-line comments
Given the input `let x = 1 // this is a comment\nlet y = 2`
When the lexer tokenizes the input
Then comments are skipped and only code tokens are produced

#### Scenario: Multi-line comments
Given the input `let x /* comment */ = 1`
When the lexer tokenizes the input
Then the comment is skipped and tokens `LET`, `IDENT("x")`, `ASSIGN`, `INT(1)` are produced

#### Scenario: Nested comments
Given the input `/* outer /* inner */ still outer */`
When the lexer tokenizes the input
Then nested comments are handled correctly

### Requirement: Source Location Tracking
The lexer MUST track accurate source locations for all tokens.

#### Scenario: Line and column tracking
Given a multi-line source file
When the lexer produces tokens
Then each token includes correct line number (1-based) and column number (1-based)

#### Scenario: Byte offset tracking
Given a source file with Unicode content
When the lexer produces tokens
Then each token includes correct byte offset from file start

### Requirement: Error Recovery
The lexer MUST recover from errors and continue tokenizing.

#### Scenario: Invalid character recovery
Given the input `let x = $ + 1`
When the lexer encounters the invalid `$` character
Then it reports an error and continues to tokenize `+` and `1`

#### Scenario: Unterminated string recovery
Given the input `let x = "unterminated\nlet y = 2`
When the lexer encounters the unterminated string
Then it reports an error and attempts to recover at the next line
