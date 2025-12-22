# TML Compiler Changelog

All notable changes to the TML compiler project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Const Declarations** (2025-12-22) - Global constants with full compiler support
  - Parser: `parse_const_decl()` in `parser_decl.cpp`
  - Type Checker: `check_const_decl()` in `checker.cpp`
  - Codegen LLVM: Inline substitution via `global_constants_` map
  - Codegen C: Generates `#define` directives
  - Test: `test_const.tml` passing
- **panic() Builtin** (2025-12-22) - Runtime panic with error messages
  - Type signature: `panic(msg: Str) -> Never`
  - Runtime: `tml_panic()` in `tml_core.c` (prints to stderr, calls exit(1))
  - Codegen C: Fully working
  - Codegen LLVM: Complete (requires runtime linking configuration)
  - Test: `test_panic.tml` passing (C backend)
- **Standard Library Package** - Reorganized to TML standards
  - Module structure: option.tml, result.tml, collections/, io/, fs/, etc.
  - Types: Maybe[T], Outcome[T,E], Vec[T], Map[K,V], etc.
  - Compiles successfully (limited functionality due to pattern binding)
- **Test Framework Package** - Core testing infrastructure
  - Assertions: assert(), assert_eq(), assert_ne()
  - Module structure: assertions/, runner/, bench/, report/
  - Compiles successfully (advanced features require compiler improvements)
- **Documentation System** - Mandatory documentation guidelines
  - `rulebook/DOCUMENTATION.md` - Guidelines for compiler changes
  - `docs/COMPILER_MISSING_FEATURES.md` - Comprehensive feature catalog (10 features)
  - Updated `rulebook/AGENT_AUTOMATION.md` and `RULEBOOK.md`
  - Golden Rule: "If it's not documented, it's not implemented"
- **Stability Annotation System** - Full API stability tracking with @stable/@deprecated annotations
  - `StabilityLevel` enum with Stable/Unstable/Deprecated levels
  - Extended `FuncSig` with stability fields and helper methods
  - Annotated core I/O functions (print, println as stable; print_i32, print_bool as deprecated)
  - Comprehensive documentation (STABILITY_GUIDE.md, STABILITY_IMPLEMENTATION.md, STABILITY_STATUS.md)
- **Parser Loop Protection** - Infinite loop detection in tuple pattern parsing
- **Implementation Status Document** - Comprehensive tracking of project progress

### Changed
- **Package Structure** (2025-12-22) - Reorganized std and test packages to TML standards
  - Removed Rust-like nested `mod.tml` wrappers
  - Simplified to file-based modules
  - Fixed enum variant syntax (removed named tuple fields temporarily)
  - Removed multi-line use groups (parser limitation)
  - Status: Both packages compile successfully
- **Major Refactoring** - Split monolithic files into focused modules
  - Lexer: 908 lines → 7 modules (~130 lines each)
  - IR Builder: 907 lines → 6 modules (~150 lines each)
  - IR Emitter: 793 lines → 4 modules (~200 lines each)
  - Borrow Checker: 753 lines → 5 modules (~150 lines each)
  - Type Environment: 1192 lines → 5 modules (~240 lines each)
  - Formatter: Split into 6 focused modules
  - CLI: Split into 7 command modules
- **Test Suite** - Updated 224+ tests with explicit type annotations
- **CMakeLists.txt** - Updated to reference all new modular files

### Fixed
- Boolean literal recognition - Added "false" to keyword table
- Boolean literal values - Set token values for true/false
- Lexer keyword access - Created accessor function for cross-module use
- UTF-8 character length - Fixed incomplete function extraction
- Compiler warning - Suppressed `-Wno-override-module` flag

### Deprecated
- `print_i32()` - Use `toString(value) + print()` instead (since v1.2)
- `print_bool()` - Use `toString(value) + print()` instead (since v1.2)

### Documentation
- 18 language specification documents in docs/specs/
- API stability guide and implementation documentation
- Developer guidelines in CLAUDE.md
- Comprehensive implementation status tracking
- `COMPILER_MISSING_FEATURES.md` - Catalog of unimplemented features
- `REORGANIZATION_SUMMARY.md` - Package reorganization details

### Known Limitations
See [COMPILER_MISSING_FEATURES.md](./docs/COMPILER_MISSING_FEATURES.md) for comprehensive details:
- ❌ **Pattern binding in when expressions** - Cannot unwrap enum payloads (e.g., `Just(v)`)
- ❌ **If-let pattern matching** - `if let` syntax not supported
- ❌ **Generic where clauses** - Type constraints not enforced
- ❌ **Function types** - Cannot use `func() -> Type` in type aliases
- ❌ **Named tuple fields** - Enum variants must use positional fields only
- ❌ **Use statement groups** - `use path::{a, b, c}` not supported
- ❌ **String interpolation** - Manual concatenation required
- ❌ **Closures** - `do(x) expr` syntax not implemented
- ⚠️ **LLVM runtime linking** - panic() needs linking configuration

## [0.1.0] - 2025-12-22

### Added
- Initial TML compiler implementation
- Complete lexer with UTF-8 support
- Parser with AST generation
- Type checker with ownership/borrowing
- IR builder and emitter
- LLVM IR and C code generators
- Code formatter
- CLI with build/run/format commands
- Test suite with GoogleTest (247 tests, 92% passing)
- Language specifications (18 documents)

### Infrastructure
- CMake build system
- C++20 codebase
- GoogleTest integration
- MSVC/Clang/GCC support
- Modular library architecture (7 libraries)

### Language Features
- Explicit type annotations required
- 32 keywords
- Primitive types: I8-I128, U8-U128, F32, F64, Bool, Char, Str
- Ownership and borrowing (Rust-like)
- Pattern matching
- Closures
- Generic types
- Module system
- ~100+ builtin functions

### Tooling
- `tml build` - Compile TML to executable
- `tml run` - Run TML programs
- `tml check` - Type check without compilation
- `tml format` - Code formatting
- `tml parse` - Parse and show AST

## Version History

- **0.1.0** (2025-12-22) - Initial implementation
- **Unreleased** - Ongoing refactoring and enhancements

---

## Legend

- **Added** - New features
- **Changed** - Changes to existing functionality
- **Deprecated** - Soon-to-be removed features
- **Removed** - Removed features
- **Fixed** - Bug fixes
- **Security** - Security fixes
- **Documentation** - Documentation changes

