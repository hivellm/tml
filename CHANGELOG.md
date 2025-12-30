# TML Compiler Changelog

All notable changes to the TML compiler project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Null Literal** (2025-12-30) - Support for `null` as a first-class literal type
  - `null` has type `Ptr[Unit]` and is compatible with any pointer type `Ptr[T]`
  - Can be assigned to any pointer variable: `let ptr: Ptr[I32] = null`
  - Can be compared with any pointer: `if ptr == null { ... }`
  - Lexer recognizes `null` as `NullLiteral` token
  - Parser handles null in literal expressions
  - Type checker types null as `Ptr[Unit]` with compatibility rules
  - LLVM codegen emits `null` pointer constant
  - Full test coverage in `compiler/tests/compiler/null_literal.test.tml`
  - Files modified:
    - `include/lexer/token.hpp` - Added `NullLiteral` to TokenKind enum
    - `src/lexer/lexer_core.cpp` - Added `null` to keywords table
    - `src/lexer/token.cpp` - Updated `is_literal` range check
    - `src/parser/parser_expr.cpp` - Added NullLiteral to literal parsing
    - `src/types/checker/expr.cpp` - Added null literal type as `Ptr[Unit]`
    - `src/types/checker/helpers.cpp` - Added null pointer compatibility in `types_compatible()`
    - `src/codegen/llvm_ir_gen_expr.cpp` - Added null literal codegen
    - `src/codegen/core/types.cpp` - Added `Ptr` type handling

- **Glob Imports** (2025-12-30) - Support for `pub use module::*` syntax
  - Import all public symbols from a module with single statement
  - Parser recognizes `::*` syntax in use declarations
  - Type checker handles glob imports via `import_all_from()`
  - Enables cleaner module re-exports (e.g., `pub use traits::*`)
  - Files modified:
    - `include/parser/ast.hpp` - Added `is_glob` field to UseDecl
    - `src/parser/parser_decl.cpp` - Parse `::*` glob import syntax
    - `src/types/checker/core.cpp` - Handle glob imports in `process_use_decl()`

- **Module Parse Error Reporting** (2025-12-30) - Detailed errors and panic on module load failures
  - Module parse errors now display detailed error information
  - Compiler exits with code 1 when a module fails to load
  - Shows file path, line/column, and error message for each parse error
  - Helps identify syntax issues in imported modules
  - Files modified:
    - `src/types/env_module_support.cpp` - Added ParseResult struct and error reporting

- **Core Alloc Module Completion** (2025-12-30) - Full Rust-compatible `core::alloc` implementation
  - `Layout` additions: `array_of()`, `with_size()`, `with_align()`, `is_zero_sized()`, `dangling()`
  - `AllocatorRef[A]` type for borrowing allocators (implements `Allocator` behavior)
  - `by_ref()` function to create allocator references
  - Global allocator functions: `alloc_global()`, `alloc_global_zeroed()`, `dealloc_global()`, `realloc_global()`
  - Helper functions: `alloc_single()`, `alloc_array()`, `alloc_array_zeroed()`, `dealloc_single()`, `dealloc_array()`
  - 100% API compatibility with Rust's `core::alloc` module

- **Core Iterator Module** (2025-12-30) - Working `core::iter` implementation
  - Concrete iterator types: `EmptyI32`, `EmptyI64`, `OnceI32`, `OnceI64`, `RepeatNI32`, `RepeatNI64`
  - Factory functions: `empty_i32()`, `once_i32()`, `repeat_n_i32()`, etc.
  - Iterator behavior trait with `next()` method
  - Iterator adapters: `Take`, `Skip`, `Chain`, `Enumerate`, `Zip`, `StepBy`, `Fuse`
  - All adapters implement the Iterator behavior with proper type associations
  - Files:
    - `lib/core/src/iter/traits.tml` - Iterator, IntoIterator, FromIterator behaviors
    - `lib/core/src/iter/sources.tml` - Empty, Once, RepeatN iterator types
    - `lib/core/src/iter/adapters.tml` - Take, Skip, Chain, etc. adapters
    - `lib/core/src/iter/mod.tml` - Module with glob re-exports
    - `lib/core/tests/iter.test.tml` - Comprehensive tests

### Fixed
- **Function Parameter Types in Codegen** (2025-12-30) - Fix parameter type lookup for module-internal function calls
  - When generating code for functions within the same module, parameter types were defaulting to `i32`
  - Added `param_types` vector to `FuncInfo` struct to store LLVM parameter types
  - Updated all `FuncInfo` creation sites to include parameter types
  - Codegen now checks `FuncInfo.param_types` when `TypeEnv` lookup fails
  - Fixes `alloc_global()` and similar module functions being called with wrong types
  - Files modified:
    - `include/codegen/llvm_ir_gen.hpp` - Added `param_types` to `FuncInfo` struct
    - `src/codegen/llvm_ir_gen_decl.cpp` - Populate param_types in function registration
    - `src/codegen/core/generate.cpp` - Populate param_types for impl methods
    - `src/codegen/llvm_ir_gen_builtins.cpp` - Check `FuncInfo.param_types` in `gen_call`

- **Str.len() Method** (2025-12-30) - Fix `.len()` method on strings falling through to `list_len`
  - `Str` type's `.len()` method was incorrectly calling `list_len` instead of `str_len`
  - Added proper handling for `Str` type in the `.len()` method codegen
  - Returns `I64` (sign-extended from `str_len`'s `I32` return)
  - Files modified:
    - `src/codegen/expr/method.cpp` - Added `Str` handling before `list_len` fallback

- **alloc() Builtin Type** (2025-12-30) - Changed `alloc()` parameter from `I32` to `I64`
  - `alloc(size)` now takes `I64` to match `Layout.size()` return type
  - Removed unnecessary `sext` instruction in codegen
  - Files modified:
    - `src/types/builtins/mem.cpp` - Changed signature from `I32` to `I64`
    - `src/codegen/builtins/mem.cpp` - Removed size conversion

- **Generic Impl Codegen** (2025-12-30) - Skip generic impl blocks during module code generation
  - Generic impls like `impl[T] Iterator for Empty[T]` were causing "Unknown method" errors
  - Module codegen now skips generic impls (they require concrete type instantiation)
  - Concrete type implementations work correctly
  - Files modified:
    - `src/codegen/core/runtime.cpp` - Skip generic impls in `emit_module_pure_tml_functions()`

- **Parallel Build System** (2025-12-29) - Multi-threaded compilation with dependency resolution
  - Full `compile_job()` implementation: lexer → parser → type checker → codegen → LLVM IR → object file
  - `DependencyGraph` class for build ordering:
    - Cycle detection via DFS
    - Topological sort for optimal build order
    - Automatic import parsing from TML source files
  - `ParallelBuilder` class with thread pool:
    - Default 8 threads for optimal performance
    - Configurable thread count (`-jN` flag)
    - Thread-safe job queue with condition variables
    - Progress reporting and build statistics
  - `tml build-all` command for parallel multi-file builds
  - Thread-safe .ll file generation with unique thread ID suffixes
  - Cache-aware compilation with timestamp checking
  - **~11x speedup** vs sequential build (benchmarked: 23 files in 1.9s vs 20.7s)
  - Files added/modified:
    - `src/cli/parallel_build.hpp` - Enhanced with DependencyGraph, ParallelBuildOptions
    - `src/cli/parallel_build.cpp` - Full implementation of parallel compilation pipeline
    - `src/cli/dispatcher.cpp` - Added `build-all` command

- **Build Cache & Incremental Compilation** (2025-12-29) - MIR-level caching for fast rebuilds
  - `--no-cache` flag to force full recompilation (bypass all caches)
  - Object file caching with timestamp validation
  - `MirCache` class for incremental compilation:
    - Binary MIR serialization/deserialization
    - Cache invalidation based on: source hash, optimization level, debug settings
    - Index file for fast cache lookups
    - Methods: `has_valid_cache()`, `load_mir()`, `save_mir()`, `get_cached_object()`
  - Files added:
    - `src/cli/build_cache.hpp` - PhaseTimer, MirCache, cache utilities
    - `src/cli/build_cache.cpp` - Full cache implementation

- **Compiler Phase Timing** (2025-12-29) - Detailed performance profiling
  - `--time` flag to show per-phase timing breakdown
  - `PhaseTimer` class for measuring phase durations (microsecond precision)
  - `ScopedPhaseTimer` RAII helper for automatic timing
  - Reports include: phase name, duration (ms), percentage of total time
  - `TML_PHASE_TIME()` macro for easy integration

- **Link-Time Optimization (LTO)** (2025-12-29) - Whole-program optimization support
  - `--lto` flag to enable LTO during build
  - Full LTO (`-flto`) and ThinLTO (`-flto=thin`) support
  - Parallel LTO with configurable job count (`-flto-jobs=N`)
  - Uses LLD linker for faster LTO linking
  - Works with executables and dynamic libraries
  - Files modified:
    - `src/cli/object_compiler.hpp` - Added `lto`, `thin_lto`, `lto_jobs` options
    - `src/cli/object_compiler.cpp` - LTO flags for compilation and linking
    - `src/cli/dispatcher.cpp` - CLI parsing for `--lto` and `--time` flags

- **Extended Build Options** (2025-12-29) - New `BuildOptions` struct and `run_build_ex()`
  - Unified options: verbose, emit_ir, emit_mir, no_cache, emit_header, show_timings, lto
  - CLI help updated with new flags
  - Files modified:
    - `src/cli/cmd_build.hpp` - Added BuildOptions struct
    - `src/cli/cmd_build.cpp` - Added run_build_ex(), cache checking in run_build()

- **Escape Analysis & Function Inlining** (2025-12-29) - Advanced optimization passes
  - **Escape Analysis** (`EscapeAnalysisPass`):
    - Tracks escape state: NoEscape, ArgEscape, ReturnEscape, GlobalEscape, Unknown
    - Analyzes heap allocations, function calls, stores, and returns
    - Fixed-point iteration for escape information propagation
    - Identifies stack-promotable allocations via `EscapeInfo.is_stack_promotable`
  - **Stack Promotion** (`StackPromotionPass`):
    - Converts non-escaping heap allocations to stack allocations
    - Reports `allocations_promoted` and `bytes_saved` statistics
    - Works with `tml_alloc`, `heap_alloc`, `Heap::new` allocations
  - **Function Inlining** (`InliningPass`):
    - Cost-based inlining with configurable thresholds
    - `InlineCost` struct: instruction_cost, call_overhead_saved, threshold
    - `InliningOptions`: base_threshold (250), recursive_limit (3), max_callee_size (500)
    - Optimization level-aware thresholds (O1: 1x, O2: 2x, O3: 4x)
    - Small function bonuses (+100 for <10 instructions, +50 for <50)
    - `InliningStats` reporting: calls_analyzed, calls_inlined, always_inline, etc.
  - **@inline/@noinline Attributes**:
    - Functions marked `@inline` or `@always_inline` are always inlined
    - Functions marked `@noinline` or `@never_inline` are never inlined
    - Decorators propagated from AST to MIR `Function.attributes`
    - Recursive inlining limit still applies to @inline functions
  - New files:
    - `include/mir/passes/escape_analysis.hpp` - Escape analysis pass header
    - `src/mir/passes/escape_analysis.cpp` - Escape analysis implementation
    - `include/mir/passes/inlining.hpp` - Inlining pass header
    - `src/mir/passes/inlining.cpp` - Inlining implementation
  - Modified: `mir.hpp` (added `Function.attributes`), `mir_builder.cpp` (decorator propagation)

- **Mid-level IR (MIR)** (2025-12-29) - SSA-form intermediate representation for optimization
  - Complete MIR infrastructure between type-checked AST and LLVM IR generation
  - SSA form with explicit control flow via basic blocks
  - Type-annotated values for easy optimization and lowering
  - MIR types: primitives, pointers, arrays, slices, tuples, structs, enums, functions
  - Instructions: arithmetic, comparisons, memory ops, control flow, calls, phi nodes
  - Terminators: return, branch, conditional branch, switch, unreachable
  - New source files:
    - `include/mir/mir.hpp` - Core MIR data structures (types, values, instructions, blocks)
    - `include/mir/mir_builder.hpp` - Builder API for constructing MIR
    - `include/mir/mir_pass.hpp` - Optimization pass infrastructure
    - `include/mir/mir_serialize.hpp` - MIR serialization/deserialization
    - `src/mir/mir_type.cpp` - MIR type utilities and conversions
    - `src/mir/mir_function.cpp` - Function and block management
    - `src/mir/mir_printer.cpp` - Human-readable MIR output
    - `src/mir/mir_builder.cpp` - MIR construction from AST
    - `src/mir/mir_serialize.cpp` - Binary/text serialization
    - `src/mir/mir_pass.cpp` - Pass manager and utilities
  - Optimization passes in `src/mir/passes/`:
    - `constant_folding.cpp` - Evaluate constant expressions at compile time
    - `constant_propagation.cpp` - Replace uses of constants with their values
    - `copy_propagation.cpp` - Replace copies with original values
    - `dead_code_elimination.cpp` - Remove unused instructions
    - `common_subexpression_elimination.cpp` - Reuse computed values
    - `unreachable_code_elimination.cpp` - Remove unreachable blocks
    - `escape_analysis.cpp` - Escape analysis and stack promotion
    - `inlining.cpp` - Function inlining with cost analysis
  - Pass manager with optimization levels (O0, O1, O2, O3)
  - Analysis utilities: value usage, side effects, constant detection

- **Local Module Imports** (2025-12-29) - Rust-style `use` imports for local `.tml` files
  - Support for `use module_name` to import sibling `.tml` files in the same directory
  - Functions accessed via `module::function()` syntax (e.g., `algorithms::factorial_iterative(10)`)
  - Public functions marked with `pub` are exported from modules
  - Compiler generates proper LLVM IR with module-prefixed function names
  - Example: `use algorithms` imports `algorithms.tml`, functions become `@tml_algorithms_*`
  - Files modified:
    - `include/codegen/llvm_ir_gen.hpp` - Added `current_module_prefix_` member variable
    - `src/codegen/llvm_ir_gen_decl.cpp` - Modified `gen_func_decl` to use module prefix
    - `src/codegen/core/runtime.cpp` - Set module prefix in `emit_module_pure_tml_functions`
    - `src/types/env_module_support.cpp` - Local module file resolution (existing)
  - Enables multi-file TML projects with proper module organization

### Changed
- **Type Checker Refactoring** (2025-12-27) - Split monolithic checker.cpp (2151 lines) into modular components
  - New directory: `src/types/checker/`
  - `checker/helpers.cpp` - Shared utilities, Levenshtein distance, type compatibility
  - `checker/core.cpp` - check_module, register_*, check_func_decl, check_impl_decl
  - `checker/expr.cpp` - check_expr, check_literal, check_ident, check_binary, check_call
  - `checker/stmt.cpp` - check_stmt, check_let, check_var, bind_pattern
  - `checker/control.cpp` - check_if, check_when, check_loop, check_for, check_return, check_break
  - `checker/types.cpp` - check_tuple, check_array, check_struct_expr, check_closure, check_path
  - `checker/resolve.cpp` - resolve_type, resolve_type_path, block_has_return, error helpers
  - Removed: Original `checker.cpp` (all code moved to checker/)

- **Type Builtins Refactoring** (2025-12-27) - Reorganized env_builtins*.cpp files
  - New directory: `src/types/builtins/`
  - `builtins/register.cpp` (was `env_builtins.cpp`) - Main registration
  - `builtins/types.cpp` - Primitive type builtins
  - `builtins/io.cpp` - I/O builtins (print, println)
  - `builtins/string.cpp` - String builtins
  - `builtins/time.cpp` - Time builtins
  - `builtins/mem.cpp` - Memory builtins
  - `builtins/atomic.cpp` - Atomic operation builtins
  - `builtins/sync.cpp` - Synchronization builtins
  - `builtins/math.cpp` - Math builtins
  - `builtins/collections.cpp` - Collection builtins
  - Removed: Original `env_builtins*.cpp` files (all moved to builtins/)

### Added
- **String Interpolation** (2025-12-27)
  - Full support for interpolated strings: `"Hello {name}!"`
  - Expressions within `{}` are evaluated and converted to strings
  - Supports any expression inside braces including arithmetic: `"Result: {a + b}"`
  - Escaped braces: `\{` and `\}` produce literal braces
  - New lexer tokens: `InterpStringStart`, `InterpStringMiddle`, `InterpStringEnd`
  - New AST node: `InterpolatedStringExpr` with `InterpolatedSegment` segments
  - Type checker validates all interpolated expressions
  - LLVM codegen uses `str_concat` for efficient string building
  - Tests added: `LexerTest.Interpolated*`, `ParserTest.Interpolated*`
  - Files modified:
    - `include/tml/lexer/token.hpp` - Added interpolated string token types
    - `include/tml/lexer/lexer.hpp` - Added `interp_depth_`, `in_interpolation_` state
    - `include/tml/parser/ast.hpp` - Added `InterpolatedSegment`, `InterpolatedStringExpr`
    - `include/tml/parser/parser.hpp` - Added `parse_interp_string_expr()` declaration
    - `src/lexer/lexer_string.cpp` - Implemented `lex_interp_string_continue()`
    - `src/lexer/lexer_operator.cpp` - Handle `}` in interpolation context
    - `src/parser/parser_expr.cpp` - Implemented `parse_interp_string_expr()`
    - `src/types/checker.cpp` - Added `check_interp_string()`
    - `src/ir/builder_expr.cpp` - Added IR generation for interpolated strings
    - `src/codegen/llvm_ir_gen_expr.cpp` - Added `gen_interp_string()`
    - `tests/lexer_test.cpp`, `tests/parser_test.cpp` - Added comprehensive tests

- **Where Clause Type Checking** (2025-12-27)
  - Full where clause constraint enforcement at call sites
  - Register behavior implementations via `impl Behavior for Type`
  - Type checking validates that type arguments satisfy all required behaviors
  - Error messages: "Type 'X' does not implement behavior 'Y' required by constraint on T"
  - Tests added: `WhereClauseParsingAndStorage`, `WhereClauseWithPrimitiveType`, etc.
  - Files modified:
    - `src/types/checker.cpp` - Added `register_impl()` call in `check_impl_decl()`, constraint checking in call handling
    - `tests/types_test.cpp` - Added 5 new where clause tests

- **Grouped Use Imports** (2025-12-27)
  - Support for `use std::io::{Read, Write}` syntax
  - Parser handles grouped imports and stores symbols in `UseDecl.symbols`
  - Type checker imports each symbol individually from the module
  - Tests added: `UseDeclaration`, `UseDeclarationGrouped`, `UseDeclarationGroupedMultiple`
  - Files modified:
    - `src/parser/parser_decl.cpp` - Already implemented, verified working
    - `tests/parser_test.cpp` - Added 4 new use declaration tests

- **Error Message Improvements** (2025-12-27)
  - Similar name suggestions for undefined identifiers using Levenshtein distance
  - Example: "Undefined variable: valeu. Did you mean: `value`?"
  - Suggestions work for variables, functions, types, enums, and behaviors
  - Maximum 3 suggestions shown, ordered by similarity
  - Distance threshold scales with name length (min 2, max name.length/2)
  - Files modified:
    - `include/tml/types/checker.hpp` - Added `find_similar_names()`, `get_all_known_names()`, `levenshtein_distance()`
    - `include/tml/types/env.hpp` - Added `all_structs()`, `all_behaviors()`, `all_func_names()`, `Scope::symbols()`
    - `src/types/checker.cpp` - Implemented suggestion functions, updated error messages
    - `src/types/env_lookups.cpp` - Implemented new accessor methods

- **Advanced Borrow Checker Features** (2025-12-27)
  - Reborrow handling: allow `&mut T -> &T` coercion and reborrowing from references
  - Two-phase borrow support: method calls like `vec.push(vec.len())` now work correctly
  - Lifetime elision rules: documented implementation following Rust's 3 rules
  - New `is_two_phase_borrow_active_` flag for method call borrow tracking
  - New `create_reborrow()` function for reborrow tracking
  - New `begin/end_two_phase_borrow()` functions for method call support
  - Files modified:
    - `include/tml/borrow/checker.hpp` - Added two-phase borrow flag and new methods
    - `src/borrow/checker_ops.cpp` - Implemented reborrow and two-phase borrow logic
    - `src/borrow/checker_expr.cpp` - Use two-phase borrows in method calls
    - `src/borrow/checker_core.cpp` - Added lifetime elision rules documentation

- **Complete Optimization Pipeline** (2025-12-27)
  - Full optimization level support: `-O0` (none), `-O1`, `-O2`, `-O3` (aggressive)
  - Size optimization modes: `-Os` (size), `-Oz` (aggressive size)
  - Debug info generation: `--debug` / `-g` flag for DWARF debug symbols
  - CLI flags: `--release` (equals -O3), `-O0` through `-O3`, `-Os`, `-Oz`
  - Global `CompilerOptions` struct for optimization settings
  - Updated build command help with all optimization options
  - Files modified:
    - `include/tml/common.hpp` - Added optimization_level and debug_info to CompilerOptions
    - `src/cli/object_compiler.cpp` - Added Os/Oz support to get_optimization_flag
    - `src/cli/object_compiler.hpp` - Updated documentation for optimization levels
    - `src/cli/build_config.cpp` - Extended validation for levels 0-5
    - `src/cli/dispatcher.cpp` - Added CLI parsing for all optimization flags
    - `src/cli/cmd_build.cpp` - Use global CompilerOptions for all builds
    - `src/cli/utils.cpp` - Updated help text with optimization options

- **FFI Support (@extern and @link decorators)** (2025-12-26)
  - New `@extern(abi)` decorator to declare C/C++ external functions without TML body
  - New `@link(library)` decorator to specify external libraries to link
  - Supported ABIs: `"c"`, `"c++"`, `"stdcall"`, `"fastcall"`, `"thiscall"`
  - Custom symbol names via `@extern("c", name = "symbol")`
  - LLVM calling conventions: `x86_stdcallcc`, `x86_fastcallcc`, `x86_thiscallcc`
  - Type checker validates @extern functions have no body
  - Linker integration passes `-l` flags for libraries
  - Files modified:
    - `include/tml/parser/ast.hpp` - Added `extern_abi`, `extern_name`, `link_libs` to FuncDecl
    - `src/parser/parser_decl.cpp` - Process @extern/@link decorators
    - `src/types/checker.cpp` - Validate @extern functions
    - `src/codegen/llvm_ir_gen_decl.cpp` - Emit LLVM `declare` with calling conventions
    - `src/cli/cmd_build.cpp` - Pass link flags to clang
  - New tests: 5 FFI unit tests in `tests/codegen_test.cpp`

- **Comprehensive Codegen Builtins Test Suite** (2025-12-27) - 86 new C++ unit tests for all builtin functions
  - New test file: `tests/codegen_builtins_test.cpp`
  - Math tests (7): sqrt, pow, abs, floor, ceil, round, black_box
  - Time tests (5): time_ms, time_us, time_ns, elapsed_ms, sleep_ms
  - Memory tests (7): alloc, dealloc, mem_copy, mem_set, mem_zero, mem_compare, mem_eq
  - Atomic tests (11): atomic_load/store/add/sub/exchange/cas/and/or, fence variants
  - Sync tests (16): spinlock, thread_yield/sleep/id, channel_*, mutex_*, waitgroup_*
  - String tests (12): str_len/hash/eq/concat/substring/contains/starts_with/ends_with/to_upper/to_lower/trim/char_at
  - Collections tests (25): list_*, hashmap_*, buffer_*
  - IO tests (3): print, println, print_i32
  - New type checker registration files:
    - `env_builtins_math.cpp` - Math builtins (sqrt, pow, abs, floor, ceil, round, black_box)
    - `env_builtins_collections.cpp` - Collection builtins (list_*, hashmap_*, buffer_*)
  - Updated `env_builtins_sync.cpp` with thread_sleep, channel_*, mutex_*, waitgroup_*
  - All 86 tests passing, 272 core tests passing total

- **Core Codegen Refactoring** (2025-12-27) - Split llvm_ir_gen.cpp (1820 lines) into modular components
  - New directory: `src/codegen/core/`
  - `core/utils.cpp` - Constructor, fresh_reg, emit, emit_line, report_error, add_string_literal
  - `core/types.cpp` - Type conversion/mangling, resolve_parser_type_with_subs, unify_types
  - `core/generic.cpp` - Generic instantiation (generate_pending_instantiations)
  - `core/runtime.cpp` - Runtime declarations, module imports, string constants
  - `core/dyn.cpp` - Dynamic dispatch and vtables (register_impl, emit_vtables)
  - `core/generate.cpp` - Main generate() function, infer_print_type
  - Removed: Original `llvm_ir_gen.cpp` (all code moved to core/)
  - Build verified: 93 codegen tests (91 passing, 2 pre-existing failures)

- **Expression Codegen Refactoring** (2025-12-27) - Split llvm_ir_gen_types.cpp (1667 lines) into modular components
  - New directory: `src/codegen/expr/`
  - `expr/infer.cpp` (287 lines) - Type inference (infer_expr_type)
  - `expr/struct.cpp` (298 lines) - Struct expressions (gen_struct_expr, gen_field, etc.)
  - `expr/print.cpp` (176 lines) - Format print (gen_format_print)
  - `expr/collections.cpp` (128 lines) - Arrays and paths (gen_array, gen_index, gen_path)
  - `expr/method.cpp` (812 lines) - Method calls (gen_method_call)
  - Removed: `llvm_ir_gen_types.cpp` (all code moved to expr/)
  - Build verified: 277 tests passing

### Fixed
- **Unit Test Fixes** (2025-12-26) - Fixed failing C++ unit tests
  - Fixed `ParserTest.IndexExpressions` - Parser now correctly distinguishes between generic args `List[I32]` and index expressions `arr[0]` by checking if content after `[` is a literal
  - Fixed TypeChecker tests that used implicit return syntax (TML requires explicit `return` statements):
    - `TypeCheckerTest.ResolveBuiltinTypes` - Added `return` to function bodies
    - `TypeCheckerTest.ResolveReferenceTypes` - Added `return` to function bodies
    - `TypeCheckerTest.ResolveSliceType` - Added `return` to function body
    - `TypeCheckerTest.SimpleFunctionDecl` - Added `return` to function body
    - `TypeCheckerTest.AsyncFunction` - Added `return` to function body
    - `TypeCheckerTest.ImplBlock` - Added `return` to impl method
    - `TypeCheckerTest.TypeAlias` - Added `return` to function body
    - `TypeCheckerTest.IfExpression` - Added `return` to if/else branches
    - `TypeCheckerTest.WhenExpression` - Added `return` before when expression
    - `TypeCheckerTest.MultipleFunctions` - Added `return` to all functions
    - `TypeCheckerTest.CompleteModule` - Added `return` to impl methods
  - Fixed `test_out_dir` integration test - Renamed fixture file to prevent test runner from executing it

### Changed
- **Runtime Functions Complete** (2025-12-26) - All codegen builtins now have runtime implementations
  - New runtime file: `runtime/math.c` - Math functions for codegen
  - Updated `runtime/time.c` - Added Instant/Duration API functions
  - Updated `runtime/thread.c` - Added wrapper functions for codegen compatibility
  - Updated `runtime/essential.c` - Consolidated all essential runtime functions
  - New functions implemented:
    - Math: `black_box_i32/i64`, `simd_sum_i32/f64`, `simd_dot_f64`, `float_to_fixed/precision/string`, `float_round/floor/ceil/abs/sqrt/pow`, `float32/64_bits`, `float32/64_from_bits`, `infinity`, `nan_val`, `is_inf`, `is_nan`, `nextafter32`
    - Time: `elapsed_secs`, `instant_now`, `instant_elapsed`, `duration_as_millis_f64`, `duration_format_secs`
    - Sync: `thread_sleep`, `thread_id`, `channel_create/destroy/len`, `mutex_create/destroy`, `waitgroup_create/destroy`

- **Codegen Builtins Refactoring** (2025-12-26) - Split monolithic builtin handler for maintainability
  - Moved from single 2165-line `llvm_ir_gen_builtins.cpp` to modular structure
  - New files in `src/codegen/builtins/`:
    - `io.cpp` - print, println, panic
    - `mem.cpp` - alloc, dealloc, mem_*
    - `atomic.cpp` - atomic_*, fence_*
    - `sync.cpp` - spinlock, threading, channels, mutex, waitgroup
    - `time.cpp` - time_*, elapsed_*, Instant::*, Duration::*
    - `math.cpp` - float_*, sqrt, pow, SIMD, black_box
    - `collections.cpp` - list_*, hashmap_*, buffer_*
    - `string.cpp` - str_* functions
  - Each handler returns `std::optional<std::string>` for clean dispatch
  - Main `gen_call` now dispatches to handlers, keeping enum/generic/user-defined call logic
  - Updated CLAUDE.md with build instructions

### Added
- **Atomic Operations & Synchronization Primitives** (2025-12-26) - Full type checker support for concurrency builtins
  - Atomic operations now registered as builtins in type checker:
    - `atomic_load(ptr: *Unit) -> I32` - Thread-safe read
    - `atomic_store(ptr: *Unit, val: I32) -> Unit` - Thread-safe write
    - `atomic_add(ptr: *Unit, val: I32) -> I32` - Atomic fetch-and-add
    - `atomic_sub(ptr: *Unit, val: I32) -> I32` - Atomic fetch-and-subtract
    - `atomic_exchange(ptr: *Unit, val: I32) -> I32` - Atomic swap
    - `atomic_cas(ptr: *Unit, expected: I32, new: I32) -> Bool` - Compare-and-swap
    - `atomic_and(ptr: *Unit, val: I32) -> I32` - Atomic bitwise AND
    - `atomic_or(ptr: *Unit, val: I32) -> I32` - Atomic bitwise OR
    - `atomic_xor(ptr: *Unit, val: I32) -> I32` - Atomic bitwise XOR
  - Memory fences for ordering guarantees:
    - `fence() -> Unit` - Full memory barrier (SeqCst)
    - `fence_acquire() -> Unit` - Acquire barrier
    - `fence_release() -> Unit` - Release barrier
  - Spinlock primitives:
    - `spin_lock(lock: *Unit) -> Unit` - Acquire spinlock (blocking)
    - `spin_unlock(lock: *Unit) -> Unit` - Release spinlock
    - `spin_trylock(lock: *Unit) -> Bool` - Try to acquire (non-blocking)
  - Thread primitives:
    - `thread_yield() -> Unit` - Yield to other threads
    - `thread_id() -> I64` - Get current thread ID
  - New source files: `env_builtins_atomic.cpp`, `env_builtins_sync.cpp`
  - Test: `atomic.test.tml` moved from pending to core tests (all 11 tests passing)

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

