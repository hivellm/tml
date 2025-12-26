# TML Compiler Changelog

All notable changes to the TML compiler project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- **CRITICAL: Generic Functions + Closures** (2025-12-26) - Generic functions accepting closures now work correctly
  - Fixed `gen_closure()` to set `last_expr_type_ = "ptr"` for closure expressions
  - Closures now correctly passed as function pointers (`ptr`) instead of integers (`i32`)
  - Unblocks stdlib functional programming: `map()`, `filter()`, `fold()`, `and_then()`, `or_else()`
  - Example: `func apply[T](x: T, f: func(T) -> T) -> T` now works with closure arguments
  - Tests: `closure_simple.test.tml` ✅, `generic_closure_simple.test.tml` ✅
  - See [BUGS.md](BUGS.md) for technical details

### Completed
- **Object File Build System** (2025-12-26) - 96% complete (147/153 tasks) ✅
  - ✅ **Phases 1-7 COMPLETE**: Object files, build cache, static/dynamic/RLIB libraries, C header generation, manifest system
  - ✅ Phases 8-10: Documentation, examples, testing, performance optimization
  - ✅ **Phase 6 (RLIB Format)**: Full implementation complete
    - TML native library format (.rlib) with JSON metadata
    - Archive creation/extraction using lib.exe (Windows) / ar (Linux)
    - CLI commands: `tml rlib info`, `tml rlib exports`, `tml rlib validate`
    - Build integration: `tml build --crate-type=rlib`
    - Content-based hashing for dependency tracking
    - Complete specification: [docs/specs/18-RLIB-FORMAT.md](docs/specs/18-RLIB-FORMAT.md)
  - ✅ **Phase 7 (Manifest)**: COMPLETE! Full manifest system implementation
    - tml.toml manifest format specification
    - Complete TOML parser (SimpleTomlParser) supporting:
      - [package], [lib], [[bin]], [dependencies], [build], [profile.*] sections
      - Semver version constraints (^1.2.0, ~1.2.3, >=1.0.0)
      - Path dependencies and inline tables
    - `tml init` command to generate new projects:
      - `tml init --lib`: Create library project
      - `tml init --bin`: Create binary project
      - Auto-generates tml.toml + src/ directory + sample code
    - Build integration: `tml build` automatically reads tml.toml
      - Manifest values used as defaults (emit-ir, emit-header, cache, optimization-level)
      - Command-line flags override manifest settings
      - Automatic output type detection ([lib] vs [[bin]])
    - Data structures: PackageInfo, LibConfig, BinConfig, Dependency, BuildSettings, ProfileConfig
    - Complete specification: [docs/specs/19-MANIFEST.md](docs/specs/19-MANIFEST.md)
  - ✅ Unit tests for object_compiler (6 tests passing)
  - ✅ Integration test infrastructure for cache and FFI workflows
  - ✅ Cache management: size limit (1GB default), LRU eviction, `cache clean/info` commands
  - ✅ Comprehensive documentation in COMPILER-ARCHITECTURE.md (section 2.3 - 227 lines)
  - See [rulebook/tasks/object-file-build-system/tasks.md](rulebook/tasks/object-file-build-system/tasks.md) for details

### Added
- **Maybe[T] and Outcome[T,E] Combinators** (2025-12-26) - Functional programming patterns for stdlib
  - Maybe[T]: `map()`, `and_then()`, `filter()`, `or_else()`
  - Outcome[T,E]: `map_ok()`, `map_err()`, `and_then_ok()`, `or_else_ok()`
  - Implemented in `packages/std/src/types/mod.tml`
  - Requires explicit type annotations on closure parameters (type inference WIP)
- **Iterator Combinators** (2025-12-26) - Functional iterator methods for std::iter
  - Core combinators: `sum()`, `count()`, `take()`, `skip()`
  - Lazy evaluation with zero-cost abstractions
  - Working with Range type and basic iteration
  - Note: `fold()`, `any()`, `all()` disabled temporarily (closure type inference bugs)
  - Example: `range(0, 100).take(10).sum()` → 45
- **Module Method Codegen Fix** (2025-12-26) - Methods from imported modules now work correctly
  - Fixed `lookup_func()` to resolve `Type::method` → `module::Type::method`
  - Fixed `last_expr_type_` tracking for method calls returning structs/enums
  - Module impl methods now generate proper LLVM IR
  - Example: `Range::next()` from `std::iter` now compiles and runs
- **Trait Objects** (2025-12-24) - Dynamic dispatch with `dyn Behavior` syntax
  - Vtable generation for behavior implementations
  - Method resolution through vtables
  - Example: `func print_any(obj: dyn Display) { obj.display() }`
- **Build Scripts** (2025-12-23) - Cross-platform build automation
  - `scripts/build.sh` and `scripts/build.bat` for building
  - `scripts/test.sh` and `scripts/test.bat` for running tests
  - `scripts/clean.sh` and `scripts/clean.bat` for cleaning
  - Target triple-based build directories (like Rust's `target/`)
- **Vitest-like Test Output** (2025-12-23) - Modern test runner UI
  - Colored output with ANSI codes
  - Test groups organized by directory
  - Beautiful summary with timing
- **Test Timeout** (2025-12-23) - Prevent infinite loops in tests
  - Default 20 second timeout per test
  - Configurable via `--timeout=N` CLI flag
- **Parallel Test Execution** (2025-12-23) - Multi-threaded test runner
  - Auto-detection of CPU cores
  - `--test-threads=N` flag for manual control
- **Benchmarking** (2025-12-23) - Performance testing with `@bench` decorator
  - Automatic 1000-iteration execution
  - Microsecond timing with `tml_time_us()`
- **Polymorphic print()** (2025-12-23) - Single print function for all types
  - Accepts I32, Bool, Str, F64, etc.
  - Compiler resolves correct runtime function
- **Module System** (2025-12-23) - Full `use` declaration support
  - Module imports working (`use test`, `use core::mem`)
  - Module registry and resolution
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
- `print_i32()` - Use `polymorphic print()` instead (since v1.2)
- `print_bool()` - Use `polymorphic print()` instead (since v1.2)

### Documentation
- 18 language specification documents in docs/specs/
- API stability guide and implementation documentation
- Developer guidelines in CLAUDE.md
- Comprehensive implementation status tracking
- `COMPILER_MISSING_FEATURES.md` - Catalog of unimplemented features
- `REORGANIZATION_SUMMARY.md` - Package reorganization details

### Known Limitations
See [COMPILER_MISSING_FEATURES.md](./docs/COMPILER_MISSING_FEATURES.md) for comprehensive details:
- ✅ **Pattern binding in when expressions** - Now working! `Just(v)` unwraps correctly
- ✅ **Module method lookup** - Now working! `Type::method` from imported modules resolved correctly
- ⚠️ **Function pointer types** - Type inference for closures incomplete (blocks `fold`, `any`, `all`)
- ⚠️ **Generic enum redefinition** - Types like `Maybe__I32` emitted multiple times in LLVM IR
- ⚠️ **Closure capture** - Basic closures work, but captured variables not yet implemented
- ⚠️ **Generic where clauses** - Parsed but not enforced by type checker
- ❌ **Tuple types** - Not yet implemented
- ❌ **Struct patterns** - Pattern matching for struct destructuring pending
- ❌ **String interpolation** - Manual concatenation required
- ⚠️ **I64 comparisons** - Type mismatch bug in codegen (blocks string operations)
- ⚠️ **Pointer references** - `mut ref I32` codegen issue (blocks memory operations)

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

