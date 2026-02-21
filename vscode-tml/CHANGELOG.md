# Change Log

All notable changes to the "tml-language" extension will be documented in this file.

## [0.18.0] - 2026-02-21

### Added
- **Type-Specific Atomic Intrinsics** - Syntax highlighting for `atomic_load_i32`, `atomic_load_i64`, `atomic_store_i32`, `atomic_store_i64`, `atomic_add_i32`, `atomic_add_i64`, `atomic_sub_i32`, `atomic_sub_i64`, `atomic_cas_i32`, `atomic_cas_i64`, `atomic_exchange_i32`, `atomic_exchange_i64`, `atomic_and_i32`, `atomic_and_i64`, `atomic_or_i32`, `atomic_or_i64`, `atomic_xor_i32`, `atomic_xor_i64`
- **Fence Intrinsics** - Added `atomic_fence`, `atomic_fence_acquire`, `atomic_fence_release`
- **Modern Memory Intrinsics** - Added `ptr_read`, `ptr_write`, `copy_nonoverlapping`
- **Extended Assertions** - 10 new assertion builtins: `assert_true`, `assert_false`, `assert_lt`, `assert_gt`, `assert_lte`, `assert_gte`, `assert_in_range`, `assert_str_len`, `assert_str_empty`, `assert_str_not_empty`
- **Compile-Time Constants** - Highlighting for `__FILE__`, `__DIRNAME__`, `__LINE__`
- **Backtick Auto-Closing** - Template literal backticks now auto-close in editor
- **Expanded Module Completions** - 35+ modules in LSP: std::json, std::regex, std::crypto, std::crypto::hash, std::compress, std::random, std::search, std::hash, std::url, std::uuid, std::semver, std::glob, std::os, std::text, std::process, std::path, std::log, std::datetime, core::encoding
- **Collection Type Completions** - Vec, HashMap, HashSet, BTreeMap, BTreeSet, Deque, Buffer
- **Text Wrapper Type** - Added `Text` to wrapper types with documentation
- **@derive Directive** - Syntax highlighting for `@derive` (replaces `@auto`)
- **@simd Directive** - Syntax highlighting for `@simd` SIMD vector operations
- **@should_panic Directive** - Test directive for expected panics
- **Extended Preprocessor Symbols** - ANDROID, IOS, FREEBSD, UNIX, POSIX, WASM32, RISCV64, PTR_32, PTR_64, LITTLE_ENDIAN, BIG_ENDIAN, MSVC, GNU, MUSL, TEST
- **Additional Type Highlighting** - Regex, Uuid, Version, DateTime, SystemTime, Xoshiro256, ThreadRng

### Changed
- Updated `TML_MODULES` in LSP server from 16 to 35 entries
- Updated `TML_COLLECTION_TYPES` to match current stdlib (removed Map/Set aliases)
- Updated `TML_WRAPPER_TYPES` with Text type and corrected Heap/Sync descriptions
- Moved `Text` from primitive types to wrapper types
- Updated keyword count from 50+ to 57 in documentation
- Updated README with comprehensive release notes for 0.14-0.17

## [0.13.0] - 2026-01-15

### Added
- **Syntax Validation & Diagnostics** - Real-time error reporting using the TML compiler
  - Syntax errors displayed in the Problems panel
  - Type errors with location information
  - Compiler warnings as diagnostics
  - JSON error format parsing from compiler output
  - Debounced validation to avoid excessive compiler calls
  - Configurable with `tml.enableDiagnostics` setting

### Configuration
- New setting: `tml.enableDiagnostics` - Enable/disable real-time syntax validation (default: true)

### Changed
- Enhanced LSP server with document validation on open/change
- Improved error handling and cleanup for temporary files
- Added support for configuration change notifications

### Technical
- Added `validateTextDocument()` function for compiler integration
- Implemented debounced validation queue (500ms delay)
- Added support for JSON diagnostic format from compiler (`--error-format=json`)

## [0.12.0] - 2026-01-15

### Added
- **Import Statement Completion** - Smart completions for `use` statements
  - Module path completion (std::, core::, test::)
  - Submodule navigation with auto-trigger
  - Member completions for each module
  - Wildcard import suggestions (`*`)
  - Import snippets (use, use wildcard, use alias)

- **Effect & Capability System Support**
  - Effect completions: pure, io, throws, async, unsafe, diverges, alloc, nondet
  - Capability completions: Read, Write, Exec, Net, Fs, Env, Time, Random
  - Hover documentation for all effects and capabilities
  - Semantic highlighting for effects in `with` clauses

- **Contract Support**
  - Contract keyword completions: requires, ensures, invariant, assert, assume
  - Hover documentation with syntax examples
  - Semantic highlighting for contract keywords

- **Module Hover Information**
  - Hover over module names to see documentation and exports

### Changed
- Enhanced `src/server/server.ts` with import context detection and effect/capability support
- Improved semantic tokens provider to highlight effects, capabilities, and contracts

## [0.11.0] - 2026-01-15

### Added
- **Build Integration** - Commands and tasks for TML compilation
  - `TML: Build` command (Ctrl+Shift+B)
  - `TML: Build (Release)` command
  - `TML: Run` command (F5)
  - `TML: Test` command
  - `TML: Clean` command
  - Task provider for automatic task discovery
  - Problem matchers for compiler error parsing

- **Semantic Highlighting** - Enhanced syntax highlighting via LSP
  - Function declarations and calls
  - Type declarations and references
  - Builtin types with special highlighting
  - Decorators (@test, @bench, etc.)
  - Enum variants

- **Editor Integration**
  - Context menu with Build/Run options
  - Editor title run button
  - Configurable compiler path (`tml.compilerPath`)
  - Additional build arguments (`tml.buildArgs`)

### Changed
- Updated `package.json` with commands, keybindings, and menus
- Added `src/client/commands.ts` for command implementations
- Added `src/client/taskProvider.ts` for task provider
- Enhanced `src/server/server.ts` with semantic tokens

## [0.10.0] - 2026-01-15

### Added
- **Language Server Protocol (LSP)** - Full LSP implementation for enhanced IDE features
  - `src/client/extension.ts` - LSP client implementation
  - `src/server/server.ts` - LSP server with completion and hover providers

- **Autocompletion** - IntelliSense support for TML
  - All TML keywords (50+ including OOP keywords)
  - Primitive types (Bool, I8-I128, U8-U128, F32, F64, etc.)
  - Collection types (List, Map, Set, Vec, Buffer)
  - Wrapper types (Maybe, Outcome, Result, Option, Heap, Shared, Sync)
  - Enum variants (Just, Nothing, Ok, Err, Less, Equal, Greater)
  - Builtin functions (print, println, panic, assert, size_of, etc.)
  - Code snippets for common patterns (func, if, for, while, when, type, impl, class, interface)

- **Hover Information** - Documentation on hover
  - Keyword descriptions
  - Type documentation with details
  - Builtin function signatures
  - Variant documentation

- **Markdown Code Block Support** - Syntax highlighting in markdown files
  - ```tml code blocks now have proper syntax highlighting
  - `syntaxes/tml.markdown.tmLanguage.json` - Markdown injection grammar

### Changed
- Updated to TypeScript-based extension architecture
- Added `tsconfig.json` for TypeScript compilation
- Updated `package.json` with LSP dependencies and build scripts
- Updated `.vscodeignore` for proper packaging

### Development
- Run `npm install` to install dependencies
- Run `npm run compile` to build the extension
- Run `npm run watch` for development with auto-rebuild

## [0.6.0] - 2025-12-27

### Added
- **Type Conversion Support** - Syntax highlighting for type conversion methods
  - `Type::from(value)` - Static type conversion methods (e.g., `I32::from(x)`)
  - `Type::default()` - Default value constructors for all primitive types

- **Hash Method Support** - `.hash()` method for all primitive types returning `I64`

- **Maybe[T] and Outcome[T,E] Builtin Enums** - First-class support for optional and result types
  - `Maybe[T]` with `Just(T)` and `Nothing` variants
  - `Outcome[T,E]` with `Ok(T)` and `Err(E)` variants
  - Methods: `is_just()`, `is_nothing()`, `unwrap()`, `unwrap_or()`, `expect()`
  - Methods: `is_ok()`, `is_err()`, `unwrap()`, `unwrap_err()`, `expect()`, `expect_err()`

- **Primitive Type Methods** - Enhanced method support
  - `.duplicate()` - Clone/copy semantics for primitives
  - `.to_string()` - String conversion for primitives and Ordering
  - `.debug_string()` - Debug representation for Ordering
  - `.cmp()` - Comparison returning Ordering enum

### Changed
- Updated syntax grammar to properly highlight builtin type methods
- Improved support for generic type parameters in Maybe[T] and Outcome[T,E]

## [0.5.0] - 2025-12-26

### Added
- **FFI Decorator Support** - Syntax highlighting for C/C++ interoperability decorators
  - `@extern(abi)` - Declare external C/C++ functions
  - `@link(library)` - Specify external libraries to link
  - Supported ABIs: `"c"`, `"c++"`, `"stdcall"`, `"fastcall"`, `"thiscall"`

### Changed
- Updated directive patterns to recognize `@extern` and `@link` decorators
- Added highlighting for decorator arguments

## [0.4.0] - 2025-12-26

### Added
- **Concurrency Primitives Support** - Syntax highlighting for new atomic and sync builtins
  - Atomic operations: `atomic_load`, `atomic_store`, `atomic_add`, `atomic_sub`, `atomic_exchange`, `atomic_cas`, `atomic_and`, `atomic_or`, `atomic_xor`
  - Memory fences: `fence`, `fence_acquire`, `fence_release`
  - Spinlock primitives: `spin_lock`, `spin_unlock`, `spin_trylock`
  - Thread primitives: `thread_yield`, `thread_id`
- Memory builtins: `alloc`, `dealloc`, `mem_alloc`, `mem_free`, `mem_copy`, `mem_move`, `mem_set`, `mem_zero`

### Changed
- Updated syntax grammar to recognize builtin functions as `support.function.builtin.tml`
- Improved function call highlighting

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
- Go to definition
- Find references
- Symbol search
- Context-aware completions (type-aware)
- Refactoring support
- Formatting provider (via `tml format`)
- Debugger support
