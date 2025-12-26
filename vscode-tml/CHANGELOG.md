# Change Log

All notable changes to the "tml-language" extension will be documented in this file.

## [0.3.1] - 2025-12-26

### Changed
- Updated to support latest TML compiler features
  - Iterator combinators (`sum`, `count`, `take`, `skip`)
  - Module method resolution improvements
  - Better support for standard library patterns

### Documentation
- Updated README to reflect compiler improvements
- Added examples for iterator usage

## [0.1.0] - 2024-12-23

### Added
- Initial release of TML Language Support
- Complete syntax highlighting for TML language
  - Keywords (35+ keywords)
  - Types (primitives, generics, user-defined)
  - Operators (arithmetic, logical, bitwise, comparison)
  - Literals (integers, floats, strings, characters, booleans)
  - Comments (line, block, doc, AI context comments)
  - Directives (@test, @benchmark, @when, @auto, etc.)
  - Functions and methods
  - Constants and variables
- Language configuration
  - Auto-closing brackets, quotes, and parentheses
  - Comment toggling
  - Bracket matching and highlighting
  - Code folding support
- Example files demonstrating TML syntax
  - basic.tml - Core language features
  - types.tml - Types, structs, enums, and behaviors
  - advanced.tml - Advanced features (closures, effects, contracts)

### Features
- Syntax highlighting for:
  - 35+ keywords (func, type, behavior, extend, if, when, loop, etc.)
  - Primitive types (Bool, I8-I128, U8-U128, F32, F64, String, Char)
  - Built-in types (List, Map, HashMap, Maybe, Outcome, etc.)
  - All operators including word-based logical operators (and, or, not)
  - Number literals with type suffixes (42i32, 3.14f64)
  - Hexadecimal (0xFF), binary (0b1010), octal (0o755) numbers
  - String literals (regular, raw, multi-line, byte strings)
  - Directives with arguments (@test, @when(os: linux), etc.)
  - Stable IDs (@a1b2c3d4)
  - AI context comments (// @ai:context:)
  - Doc comments (///, //!)
- Auto-closing pairs for brackets, quotes, parentheses
- Smart indentation
- Code folding regions

## [Unreleased]

### Planned Features
- IntelliSense/autocomplete support
- Go to definition
- Find references
- Symbol search
- Code snippets for common patterns
- Refactoring support
- Error diagnostics
- Formatting provider
- Debugger support
