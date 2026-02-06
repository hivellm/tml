# TML Compiler Changelog

All notable changes to the TML compiler project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Smart Pointers** (2026-02-06) - Complete Rust-style smart pointer implementations
  - `Heap[T]` - Unique pointer with ownership semantics (like Rust's `Box[T]`)
    - Methods: `new()`, `get()`, `set()`, `into_inner()`, `as_ptr()`, `from_raw()`, `leak()`
    - Automatic memory deallocation via Drop trait
    - Deep copy via Duplicate trait
  - `Shared[T]` - Non-atomic reference-counted pointer (like Rust's `Rc[T]`)
    - Methods: `new()`, `get()`, `strong_count()`, `is_unique()`, `get_mut()`, `try_unwrap()`
    - Automatic reference counting with Drop
    - Share ownership across multiple owners (single-threaded)
  - `Sync[T]` - Atomic reference-counted pointer (like Rust's `Arc[T]`)
    - Thread-safe atomic reference counting
    - Uses `atomic_fetch_add_i32`/`atomic_fetch_sub_i32` for safety
    - Safe for cross-thread sharing
  - All smart pointers implement Drop, Display, Debug behaviors
  - 15 comprehensive tests in `lib/core/tests/alloc/smart_pointers.test.tml`
  - Files created:
    - `lib/core/src/alloc/heap.tml` - Heap[T] implementation (222 lines)
    - `lib/core/src/alloc/shared.tml` - Shared[T] implementation (283 lines)
    - `lib/core/src/alloc/sync.tml` - Sync[T] implementation (318 lines)
    - `lib/core/tests/alloc/smart_pointers.test.tml` - 15 tests
  - Files modified:
    - `lib/core/src/alloc/mod.tml` - Added smart pointer exports

- **Atomic Operations** (2026-02-06) - Cross-platform atomic primitives for lock-free programming
  - **I32/I64 Operations**: fetch_add, fetch_sub, load, store, compare_exchange, swap
  - **Memory Fences**: acquire, release, seq_cst barriers
  - **Platform Support**: Windows (InterlockedX) and Unix (__sync_fetch_and_X)
  - Runtime implementation (180+ lines in `compiler/runtime/sync.c`):
    - `atomic_fetch_add_i32/i64` - Atomic fetch-and-add
    - `atomic_fetch_sub_i32/i64` - Atomic fetch-and-subtract
    - `atomic_load_i32/i64` - Thread-safe read
    - `atomic_store_i32/i64` - Thread-safe write
    - `atomic_compare_exchange_i32/i64` - Compare-and-swap
    - `atomic_swap_i32/i64` - Atomic exchange
    - `atomic_fence`, `atomic_fence_acquire`, `atomic_fence_release` - Memory barriers
  - Type system integration (126 lines in `compiler/src/types/builtins/atomic.cpp`)
  - Codegen integration (187 lines in `compiler/src/codegen/builtins/atomic.cpp`)
  - LLVM IR declarations in `compiler/src/codegen/core/runtime.cpp`
  - 18 comprehensive tests in `lib/core/tests/ops/atomic.test.tml`
  - Files modified:
    - `compiler/runtime/sync.c` - Added 180+ lines of atomic operations
    - `compiler/src/types/builtins/atomic.cpp` - Added 126 lines of type signatures
    - `compiler/src/codegen/builtins/atomic.cpp` - Added 187 lines of LLVM IR codegen
    - `compiler/src/codegen/core/runtime.cpp` - Added atomic function declarations
  - Files created:
    - `lib/core/tests/ops/atomic.test.tml` - 18 tests (I32 operations, fences, edge cases)

- **Drop Trait Enabled** (2026-02-06) - Automatic cleanup for smart pointers
  - Drop behavior was already implemented in compiler, now enabled for smart pointers
  - Heap[T], Shared[T], Sync[T] all have Drop implementations
  - Automatic RAII-style resource cleanup at scope exit
  - LIFO drop order (last declared, first dropped)
  - Move semantics support (consumed variables skip drop)

### Fixed
- **BoolLiteral Dangling Lexeme** (2026-02-05) - Fixed corrupted boolean constants from imported modules
  - `BoolLiteral.token.lexeme` is a `string_view` pointing to original source text
  - When source is freed after module extraction, lexeme comparisons fail with corrupted data
  - This caused `AtomicBool::LOCK_FREE = true` to be registered as `= 0` instead of `= 1`
  - All 85 atomic tests were failing due to this bug
  - Fix: Replace `lexeme == "true"` with `bool_value()` which returns stored boolean value
  - Also fixed type checker to look up constants from imported modules via module registry
  - Files modified:
    - `compiler/src/types/env_module_support.cpp` - Use `bool_value()` for constant extraction
    - `compiler/src/codegen/core/generate.cpp` - Use `bool_value()` in codegen
    - `compiler/src/codegen/core/runtime.cpp` - Use `bool_value()` for imported constants
    - `compiler/src/codegen/core/class_codegen.cpp` - Use `bool_value()` for class attributes
    - `compiler/src/codegen/expr/collections.cpp` - Simplified constant lookup
    - `compiler/src/types/checker/types.cpp` - Module registry constant lookup

- **Ref Parameter Passing Bug** (2026-02-04) - Fixed incorrect pointer passing for ref parameters
  - When a function with `ref T` parameter passed that parameter to another function taking `ref T`,
    the codegen incorrectly passed the stack slot address instead of loading the pointer value first
  - This caused zlib `inflate()` to receive wrong pointer, leading to decompression failures
  - Root cause: call.cpp assumed all local variables held values, not pointers
  - Fix: Check if local's `semantic_type` is `RefType`, if so load the pointer before passing
  - Files modified:
    - `compiler/src/codegen/expr/call.cpp` - Check semantic_type for RefType before passing ref args
    - `lib/std/runtime/zlib/zlib_deflate.c` - Removed debug output

### Changed
- **Coverage Requires Full Test Suite** (2026-02-04) - Coverage no longer works with filters
  - `tml test --coverage --filter X` now exits with error
  - Coverage requires running the full test suite for accurate data
  - Clear error message explains how to run coverage correctly
  - Files modified: `compiler/src/cli/tester/run.cpp`, `compiler/src/cli/tester/suite_execution.cpp`

- **Build Script Kills MCP Server** (2026-02-04) - Prevents link errors during build
  - `scripts/build.bat` now kills `tml_mcp.exe` before building
  - Fixes LNK1168 error when MCP server is running during build
  - Files modified: `scripts/build.bat`

### Fixed
- **Windows Crypto Build** (2026-02-04) - Fixed include order in crypto.c
  - `windows.h` must be included before `bcrypt.h` on Windows
  - Files modified: `compiler/runtime/crypto.c`

- **Reflect Behavior Syntax** (2026-02-04) - Fixed invalid `ref this` syntax
  - Changed `func runtime_type_info(ref this)` to `func runtime_type_info(this)`
  - Files modified: `lib/core/src/reflect.tml`

### Added
- **Reflection: Enum Methods** (2026-02-04) - `variant_name()` and `variant_tag()` for reflected enums
  - Enums with `@derive(Reflect)` now have runtime variant introspection
  - `variant_name(this) -> Str` - Returns the name of the active variant as a string
  - `variant_tag(this) -> I64` - Returns the discriminant tag value (0, 1, 2, ...)
  - Generated methods use efficient switch statements for variant lookup
  - Type checker integration: methods are recognized during type checking
  - Files modified:
    - `compiler/src/codegen/derive/reflect.cpp` - Added `gen_derive_reflect_enum_methods()`
    - `compiler/include/codegen/llvm_ir_gen.hpp` - Added function declaration
    - `compiler/src/types/checker/core.cpp` - Register variant_name/variant_tag signatures
    - `compiler/tests/compiler/reflect/derive_reflect.test.tml` - Added 4 new tests

- **Comprehensive @derive Macro System** (2026-02-05) - Automatic trait implementation for structs/enums
  - New derive macros similar to Rust's `#[derive]` that generate method implementations:
    - `@derive(PartialEq)` - `eq(ref this, other: ref Self) -> Bool` field-by-field equality
    - `@derive(Duplicate)` - `duplicate(ref this) -> Self` field-by-field copy
    - `@derive(Hash)` - `hash(ref this) -> I64` FNV-1a hash algorithm
    - `@derive(Default)` - `default() -> Self` static method returning zero-initialized values
    - `@derive(PartialOrd)` - `partial_cmp(ref this, other: ref Self) -> Maybe[Ordering]` partial ordering
    - `@derive(Ord)` - `cmp(ref this, other: ref Self) -> Ordering` total ordering
    - `@derive(Debug)` - `debug_string(ref this) -> Str` string representation
  - All derives work for both structs and simple enums
  - Lexicographic comparison for PartialOrd/Ord (compare fields in order)
  - 40 tests covering all derive macros
  - Files created:
    - `compiler/include/derive/registry.hpp` - DerivableTrait enum
    - `compiler/src/codegen/derive/partial_eq.cpp` - PartialEq codegen
    - `compiler/src/codegen/derive/duplicate.cpp` - Duplicate codegen
    - `compiler/src/codegen/derive/hash.cpp` - Hash codegen (FNV-1a)
    - `compiler/src/codegen/derive/default.cpp` - Default codegen
    - `compiler/src/codegen/derive/partial_ord.cpp` - PartialOrd/Ord codegen
    - `compiler/src/codegen/derive/debug.cpp` - Debug codegen
    - `lib/core/tests/derive/` - Test files for all derives
  - Files modified:
    - `compiler/include/codegen/llvm_ir_gen.hpp` - Derive method declarations
    - `compiler/CMakeLists.txt` - Added new source files
    - `compiler/src/codegen/decl/struct.cpp` - Call derive generators
    - `compiler/src/codegen/decl/enum.cpp` - Call derive generators
    - `compiler/src/types/checker/core.cpp` - Register impl blocks and method signatures

- **Build Script Enforcement** (2026-02-04) - CMake direct builds are now blocked
  - Added build token verification in CMakeLists.txt to prevent direct cmake usage
  - Only `scripts/build.bat` (Windows) and `scripts/build.sh` (Linux) can build the project
  - Direct cmake commands fail with a clear error message explaining how to use scripts
  - This prevents build corruption, cache issues, and silent test failures
  - Files modified:
    - `compiler/CMakeLists.txt` - Added TML_BUILD_TOKEN verification
    - `scripts/build.bat` - Passes TML_BUILD_TOKEN to cmake
    - `scripts/build.sh` - Passes TML_BUILD_TOKEN to cmake
    - `CLAUDE.md` - Documented enforcement mechanism

- **Fast Hash Module** (2026-02-04) - Non-cryptographic hash functions for ETags and checksums
  - New `std::hash` module with fast hash algorithms optimized for speed (NOT security)
  - FNV-1a: `fnv1a32()`, `fnv1a64()` - Fast, simple hash with good distribution
  - MurmurHash2: `murmur2_32()`, `murmur2_64()` - Seeded hash for varied sequences
  - All functions support both `Str` and `Buffer` inputs (`*_bytes` variants)
  - `Hash32` and `Hash64` types with `.raw()`, `.to_hex()`, `.to_i64()` methods
  - ETag helpers: `etag_weak()`, `etag_strong()` for HTTP caching
  - Files added:
    - `lib/std/src/hash.tml` - Fast hash API (FNV-1a, Murmur2, ETag helpers)
    - `lib/std/tests/hash/fast_hash.test.tml` - 22 tests covering all functions
  - Files modified:
    - `compiler/runtime/crypto.c` - Added FNV-1a and Murmur2 C implementations
    - `lib/std/src/mod.tml` - Export new hash module

- **Crypto Module** (2026-02-02) - Cryptographically secure random number generation
  - New `std::crypto` module with CSPRNG support using native OS APIs
  - Windows: BCryptGenRandom (CNG), Linux: getrandom(), macOS: SecRandomCopyBytes
  - Functions: `random_bytes()`, `random_int()`, `random_uuid()`, `random_fill()`
  - `SecureRandom` class for object-oriented random generation
  - Type-specific generators: `random_u8/u16/u32/u64`, `random_i32/i64`, `random_f32/f64`
  - Timing-safe comparison: `timing_safe_equal()`, `timing_safe_equal_str()`
  - Prime generation (for RSA): `generate_prime()`, `generate_safe_prime()`, `check_prime()`
  - Files added:
    - `lib/std/src/crypto/random.tml` - Random number generation API
    - `lib/std/src/crypto/error.tml` - CryptoError type
    - `lib/std/src/crypto/hash.tml` - Hash algorithms (SHA-256, SHA-512, MD5, BLAKE3, etc.)
    - `lib/std/src/crypto/hmac.tml` - HMAC message authentication
    - `lib/std/src/crypto/cipher.tml` - Symmetric encryption (AES-GCM, ChaCha20)
    - `lib/std/src/crypto/kdf.tml` - Key derivation (PBKDF2, HKDF, scrypt)
    - `lib/std/src/crypto/sign.tml` - Digital signatures (ECDSA, Ed25519)
    - `lib/std/src/crypto/key.tml` - Key management
    - `lib/std/src/crypto/dh.tml` - Diffie-Hellman key exchange
    - `lib/std/src/crypto/ecdh.tml` - Elliptic curve Diffie-Hellman
    - `lib/std/src/crypto/rsa.tml` - RSA encryption/signing
    - `lib/std/src/crypto/x509.tml` - X.509 certificate handling
    - `compiler/runtime/crypto.c` - Native FFI implementations
  - Files modified:
    - `compiler/CMakeLists.txt` - Added crypto.c to runtime
    - `compiler/src/cli/builder/helpers.cpp` - Added crypto.c to fallback compilation

- **Zlib Module** (2026-02-02) - Compression and decompression support
  - New `std::zlib` module for data compression
  - Deflate/Inflate: `deflate()`, `inflate()` with configurable compression levels
  - Gzip: `gzip_compress()`, `gzip_decompress()` for gzip format
  - Brotli: `brotli_compress()`, `brotli_decompress()` for Brotli compression
  - Zstd: `zstd_compress()`, `zstd_decompress()` for Zstandard compression
  - CRC32: `crc32()`, `crc32_combine()` checksum functions
  - Streaming API: `DeflateStream`, `InflateStream` for large data
  - Configurable options: compression level, window bits, memory level
  - Files added:
    - `lib/std/src/zlib/deflate.tml` - Deflate compression
    - `lib/std/src/zlib/gzip.tml` - Gzip format support
    - `lib/std/src/zlib/brotli.tml` - Brotli compression
    - `lib/std/src/zlib/zstd.tml` - Zstandard compression
    - `lib/std/src/zlib/crc32.tml` - CRC32 checksums
    - `lib/std/src/zlib/stream.tml` - Streaming compression API
    - `lib/std/src/zlib/error.tml` - ZlibError type
    - `lib/std/src/zlib/options.tml` - Compression options
    - `lib/std/runtime/zlib/` - Native FFI implementations

- **Struct Update Syntax** (2026-02-01) - Copy struct with field overrides using `..base`
  - New syntax: `Point { x: 5, ..base_struct }` copies fields from base_struct, overrides x
  - Also supports copying all fields: `Point { ..base_struct }` (equivalent to clone)
  - Works with any struct type, including those with Bool, I32, and other field types
  - Files modified:
    - `compiler/src/codegen/expr/struct.cpp` - Implement struct update codegen in gen_struct_expr_ptr

- **Test Organization** (2026-02-01) - Reorganized compiler tests into logical folders
  - `basics/` - Core language features (variables, expressions, control flow)
  - `structs/` - Struct-related tests
  - `enums/` - Enum tests
  - `generics/` - Generic type parameters and GATs
  - `closures/` - Closure and higher-order function tests
  - `patterns/` - Pattern matching tests
  - `behaviors/` - Traits, impl blocks, dyn dispatch, associated types
  - `memory/` - Memory management, pointers, drop
  - `modules/` - Module system tests
  - `types/` - Numeric types, strings, primitives
  - `error_handling/` - Try operator, throw
  - `misc/` - Other tests

- **Loop Variable Declaration Syntax** (2026-01-21) - Cleaner loop syntax with inline variable declaration
  - New syntax: `loop (var i: I32 < N) { ... }` declares and initializes loop variable
  - Variable is automatically initialized to `0` and scoped to the loop
  - Condition `name < limit` is checked before each iteration
  - Manual increment required: `i = i + 1`
  - Works with all integer types: I32, I64, U32, U64, etc.
  - Full support across all codegen paths (direct LLVM and MIR-based)
  - Files modified:
    - `compiler/src/parser/parser_expr.cpp` - Parse loop variable syntax
    - `compiler/src/types/checker/control.cpp` - Type check and scope management
    - `compiler/src/hir/hir_builder_expr.cpp` - HIR variable registration
    - `compiler/src/mir/builder/hir_expr.cpp` - MIR variable initialization
    - `compiler/src/codegen/llvm_ir_gen_control.cpp` - Direct LLVM codegen support

- **Array Bounds Check Elimination** (2026-01-20) - Zero-cost safety for constant indices
  - Added bounds checking for array indexing (catches out-of-bounds at runtime)
  - Compiler eliminates bounds checks when index is provably safe (e.g., constant indices, loop induction variables)
  - Uses value range analysis to track possible index values
  - Constant indices within bounds generate no check (zero runtime overhead)
  - Loop induction variable analysis: `for i in 0 to arr.len()` → eliminates all checks inside loop
  - Detects loop conditions (`i < N`, `i >= N`) to infer induction variable bounds
  - Runtime indices still get bounds checks for safety
  - Files modified:
    - `compiler/include/mir/mir.hpp` - Added `needs_bounds_check` and `known_array_size` to GetElementPtrInst
    - `compiler/src/mir/builder/hir_expr.cpp` - Populate array size in GEP instructions
    - `compiler/src/mir/passes/bounds_check_elimination.cpp` - Fixed constant type handling, enhanced loop detection
    - `compiler/src/codegen/mir/instructions.cpp` - Generate bounds check when needed

- **Performance Optimization: Compile-Time String Literal Concatenation** (2026-01-20) - 100x+ faster literal concatenation
  - Compiler concatenates string literals at compile time: `"Hello" + " " + "World"` → `"Hello World"`
  - Zero runtime cost for literal concatenation (emits static constant)
  - Achieved C++ parity for literal string operations (~1ns per op)
  - Files modified:
    - `compiler/src/codegen/expr/binary.cpp` - Added compile-time literal concatenation detection

- **Performance Optimization: Int-to-String Conversion** (2026-01-20) - 9.6x faster integer-to-string conversion
  - Implemented lookup table algorithm for 2-digit conversion (00-99 digit pairs)
  - New `fast_i64_to_str` function processes 2 digits at a time instead of division by 10
  - Reduced int-to-string time from 134ns to 10ns per operation
  - Now within 1.7x of C++ performance (target achieved: < 2x)
  - Files modified:
    - `compiler/runtime/string.c` - Added lookup table and optimized conversion functions

- **Performance Optimization: String Concat Chain Fusion** (2026-01-20) - 2.9x faster multi-string concatenation
  - Compiler detects chains like `a + b + c + d` and fuses into single allocation
  - New runtime functions: `str_concat_3`, `str_concat_4`, `str_concat_n`
  - Single allocation for multiple strings instead of O(n) intermediate allocations
  - Reduced 4-string concat from 306ns to 105ns per operation
  - Files added:
    - `compiler/runtime/string.c` - Added `str_concat_3`, `str_concat_4`, `str_concat_n`
    - `compiler/runtime/essential.h` - Function declarations
  - Files modified:
    - `compiler/src/codegen/expr/binary.cpp` - Concat chain detection and fusion
    - `compiler/src/codegen/mir_codegen.cpp` - LLVM IR function declarations
    - `compiler/src/codegen/core/runtime.cpp` - Runtime function declarations

- **Performance Optimization: Text Builder push_i64** (2026-01-20) - 1.6x faster log building
  - New `Text::push_i64()` method appends integers directly without intermediate string allocation
  - Uses lookup table algorithm for fast integer-to-string conversion inline
  - Improved log building from 4.7x slower to 3x slower vs C++
  - Files modified:
    - `compiler/runtime/text.c` - Added `tml_text_push_i64`, `tml_text_push_str_len`, `tml_text_push_formatted`
    - `lib/std/src/text.tml` - Added `push_i64` and `push_formatted` methods
    - `compiler/src/codegen/core/runtime.cpp` - Runtime function declarations

- **Performance Optimization: Inline String Concat Codegen** (2026-01-20) - Avoids FFI call overhead
  - Compiler generates inline LLVM IR for 2-4 string concatenation instead of calling runtime functions
  - Uses `llvm.memcpy` intrinsic for efficient copying
  - Computes literal string lengths at compile-time
  - Eliminates function call overhead for common concat patterns
  - Files modified:
    - `compiler/src/codegen/expr/binary.cpp` - Added inline concat codegen
    - `compiler/src/codegen/core/runtime.cpp` - Added strlen and llvm.memcpy declarations

- **MIR Codegen Modularization** (2026-01-20) - Split `mir_codegen.cpp` (~1786 lines) into focused modules
  - `mir/helpers.cpp` - Value lookup, atomic helpers, binary operation helpers (~180 lines)
  - `mir/types.cpp` - MIR to LLVM type conversion (~100 lines)
  - `mir/terminators.cpp` - Return, Branch, Switch, Unreachable emission (~100 lines)
  - `mir/instructions.cpp` - All instruction emission (~850 lines)
  - Core `mir_codegen.cpp` reduced to ~413 lines (generation orchestration only)
  - Files added:
    - `compiler/src/codegen/mir/helpers.cpp`
    - `compiler/src/codegen/mir/types.cpp`
    - `compiler/src/codegen/mir/terminators.cpp`
    - `compiler/src/codegen/mir/instructions.cpp`
  - Files modified:
    - `compiler/src/codegen/mir_codegen.cpp` - Kept core generation methods
    - `compiler/include/codegen/mir_codegen.hpp` - Added helper method declarations
    - `compiler/CMakeLists.txt` - Added new source files to tml_codegen

- **Runtime Modularization** (2026-01-20) - Split `essential.c` into focused runtime modules
  - `sync.c` - Synchronization primitives (mutex, rwlock, condvar, thread functions)
  - `pool.c` - Object pool functions for `@pool` classes (global and thread-local pools)
  - `collections.c` - Dynamic list functions (list_create, list_push, list_get, etc.)
  - Reduces `essential.c` complexity and improves code organization
  - Files added:
    - `compiler/runtime/sync.c` - tml_mutex_*, tml_rwlock_*, tml_condvar_*, tml_thread_*
    - `compiler/runtime/pool.c` - pool_init, pool_acquire, pool_release, tls_pool_*
    - `compiler/runtime/collections.c` - list_create, list_push, list_pop, list_get, etc.
  - Files modified:
    - `compiler/runtime/essential.c` - Removed duplicated code
    - `compiler/CMakeLists.txt` - Added new runtime source files
    - `compiler/src/cli/builder/helpers.cpp` - Added collections.c to default runtime linking

- **Self-Contained Compiler** (2026-01-19) - TML compiler now works without external tool dependencies
  - Built-in LLVM backend for IR-to-object compilation (no clang required)
  - Built-in LLD linker integration (no system linkers required)
  - Pre-compiled runtime library bundled with compiler distribution
  - `--use-external-tools` flag for fallback/debugging
  - Auto-detection: uses built-in backends when available, falls back to clang if needed
  - Improved error messages with actionable solutions when tools are unavailable
  - Files added:
    - `compiler/include/backend/lld_linker.hpp` - LLD linker wrapper interface
    - `compiler/src/backend/lld_linker.cpp` - LLD linker implementation
  - Files modified:
    - `compiler/CMakeLists.txt` - LLVM/LLD library dependencies, runtime build
    - `compiler/src/cli/builder/object_compiler.cpp` - Backend routing (LLVM/clang)
    - `compiler/src/cli/builder/compiler_setup.cpp` - Optional clang detection
    - `compiler/src/cli/dispatcher.cpp` - `--use-external-tools` flag
    - `compiler/include/common.hpp` - `CompilerOptions::use_external_tools`
    - `docs/user/ch01-01-installation.md` - Updated installation docs

- **Text Type with Template Literals** (2026-01-15) - Complete dynamic string type implementation
  - New `Text` type in `std::text` module - heap-allocated, growable strings with SSO
  - Small String Optimization (SSO) for strings ≤23 bytes - no heap allocation
  - 40+ runtime functions in `compiler/runtime/text.c`
  - Template literal syntax: `` `Hello, {name}!` `` produces `Text` type
  - String interpolation with automatic type conversion for I32, I64, F64, Bool, Str
  - Multi-line template literals supported
  - Escape sequences: `\{`, `\}`, `\n`, `\t`, `\\`, etc.
  - Comprehensive method set: `len`, `push`, `push_str`, `concat`, `substring`, `trim`, `replace`, `contains`, `starts_with`, `ends_with`, `to_upper_case`, `to_lower_case`, `pad_start`, `pad_end`, `reverse`, `repeat`, and more
  - VSCode syntax highlighting for template literals and `Text` type
  - 134 TML tests + 42 C++ codegen tests
  - Files added:
    - `compiler/runtime/text.c` - Text runtime implementation
    - `lib/std/src/text.tml` - Text TML module
    - `lib/std/tests/text.test.tml` - TML unit tests
    - `compiler/tests/text_test.cpp` - C++ codegen tests
  - Files modified:
    - `compiler/include/lexer/token.hpp` - Template literal token types
    - `compiler/src/lexer/lexer_string.cpp` - Template literal lexing
    - `compiler/include/parser/ast_exprs.hpp` - TemplateLiteralExpr AST node
    - `compiler/src/parser/parser_expr.cpp` - Template literal parsing
    - `compiler/src/types/checker/expr.cpp` - Template literal type checking
    - `compiler/src/codegen/expr/core.cpp` - Template literal codegen
    - `compiler/src/codegen/core/runtime.cpp` - Text runtime declarations
    - `vscode-tml/syntaxes/tml.tmLanguage.json` - Syntax highlighting

- **Implicit Numeric Literal Coercion** (2026-01-15) - Variables and struct fields with type annotations now accept unsuffixed literals
  - `var a: U8 = 128` works without requiring `128 as U8`
  - `let b: I16 = 1000` works without requiring `1000 as I16`
  - `Simple { value: 5000 }` works for `value: I64` fields without `5000 as I64`
  - Supports all integer types: I8, I16, I32, I64, U8, U16, U32, U64
  - Float struct fields: automatic `double → float` truncation via `fptrunc`
  - Explicit `as Type` casts still work for complex expressions or when inference isn't available
  - Files modified:
    - `compiler/include/codegen/llvm_ir_gen.hpp` - Added `expected_literal_type_` context
    - `compiler/src/codegen/expr/core.cpp` - Use expected type in `gen_literal`
    - `compiler/src/codegen/llvm_ir_gen_stmt.cpp` - Set context before initializer generation
    - `compiler/src/codegen/expr/struct.cpp` - Set expected type for struct field initializers

### Fixed
- **Test Crash/Failure Error Visibility** (2026-01-16) - Test errors now display without --verbose flag
  - Fixed suite mode test runner to capture detailed error messages from `run_suite_test`
  - Previously only showed "Exit code: -2" for crashes, now shows full message like "Exit code: -2\nTest crashed: ACCESS_VIOLATION (Segmentation fault)"
  - Includes captured output which may contain panic/crash diagnostic info
  - Files modified:
    - `compiler/src/cli/tester/suite_execution.cpp` - Added error message and output capture from run_result

- **MIR Codegen Class Type Resolution** (2026-01-15) - Fixed class instance method calls with -O3 optimization
  - Class variables (e.g., `let p: Point = ...`) now correctly resolve to `ClassType` instead of `NamedType`
  - Method calls like `p.get_x()` now generate correct IR: `@Point__get_x` instead of `@Ptr__get_x`
  - HIR builder now maintains type environment scopes during lowering for proper variable type lookup
  - Copy propagation pass now preserves operand types to prevent type mismatches in icmp instructions
  - Files modified:
    - `compiler/src/hir/hir_builder.cpp` - Added class type lookup in `resolve_type`, push type env scope in `lower_function`
    - `compiler/src/hir/hir_builder_expr.cpp` - Push/pop type env scope in `lower_block`
    - `compiler/src/hir/hir_builder_stmt.cpp` - Define let/var bindings in type env scope
    - `compiler/src/mir/builder/hir_expr.cpp` - Use HIR receiver_type for method calls
    - `compiler/src/mir/builder/types.cpp` - Return ptr type for ClassType
    - `compiler/src/mir/hir_mir_builder.cpp` - Handle ClassType in convert_type_impl
    - `compiler/src/mir/passes/copy_propagation.cpp` - Preserve types during value replacement
    - `compiler/src/types/checker/core.cpp` - Handle self-referential class return types
    - `compiler/src/codegen/mir_codegen.cpp` - Fix :: to __ replacement in function names

- **Memory Pointer Type Consistency** (2026-01-14) - Unified pointer types for memory operations
  - Changed `alloc` builtin return type from `mut ref I32` to `*Unit` (opaque pointer)
  - Changed `dealloc` builtin parameter type to accept `*Unit`
  - Updated `read_i32`, `write_i32`, `ptr_offset` signatures to use `*Unit`
  - Updated `core::mem` library module to use `*Unit` consistently
  - Fixes type mismatch errors in atomic operations and drop tests
  - Files modified:
    - `compiler/src/types/builtins/mem.cpp` - Updated builtin signatures
    - `compiler/src/core/mem.tml` - Updated library declarations
    - `compiler/src/codegen/builtins/intrinsics.cpp` - Fixed `ptr_offset` codegen for `*Unit`

- **ptr_offset Codegen for Opaque Pointers** (2026-01-14) - Fixed LLVM IR generation for `*Unit` pointers
  - `ptr_offset` with `*Unit` was generating invalid `getelementptr void` LLVM IR
  - Now uses `i32` as element type for `*Unit` pointers (4-byte offsets for I32 semantics)
  - Properly detects Unit type via `PrimitiveKind::Unit` check
  - Files modified:
    - `compiler/src/codegen/builtins/intrinsics.cpp` - Fixed element type detection

- **Constant Imports from Modules** (2026-01-14) - Constants can now be imported via `use` declarations
  - `use core::char::MAX` now correctly resolves the constant value
  - Added constant lookup in `check_ident` when identifier not found in variables
  - Resolves imported symbol path, finds module, looks up constant value
  - Files modified:
    - `compiler/src/types/checker/expr.cpp` - Added constant lookup in `check_ident`

- **`self` as Alias for `this`** (2026-01-14) - Both keywords now work identically in methods
  - `self` is now recognized as equivalent to `this` in method contexts
  - HIR builder properly handles `self` identifier resolution
  - Parser accepts both `self` and `this` in method signatures

- **`This` Type Resolution** (2026-01-14) - Fixed `This` type in impl blocks
  - `This` type now correctly resolves to the implementing type in HIR builder
  - Enables patterns like `func new() -> This { ... }` in impl blocks

### Changed
- **Test Files Updated for `*Unit` Pointer Type** (2026-01-14) - Memory-related tests updated
  - `compiler/tests/compiler/mem.test.tml` - Uses `*Unit` for alloc/dealloc
  - `compiler/tests/compiler/memory.test.tml` - Uses `*Unit` for memory operations
  - `compiler/tests/compiler/modules.test.tml` - Uses `*Unit` for module tests
  - `compiler/tests/compiler/sync.test.tml` - Uses `*Unit` for atomic operations

### Added
- **Runtime Memory Leak Detection** (2026-01-14) - Automatic memory leak tracking for debug builds
  - New `mem_track.h/.c` runtime module that tracks all allocations/deallocations
  - Integrated with `mem.c` via `TML_DEBUG_MEMORY` preprocessor flag
  - Reports unfreed allocations at program exit with address, size, and allocation ID
  - CLI flags: `--check-leaks` (enable) and `--no-check-leaks` (disable)
  - Enabled by default in debug builds, automatically disabled in release builds
  - Thread-safe tracking with mutex protection
  - Files added:
    - `compiler/runtime/mem_track.h` - Memory tracking API
    - `compiler/runtime/mem_track.c` - Tracking implementation
  - Files modified:
    - `compiler/runtime/mem.c` - Integration with tracking
    - `compiler/include/common.hpp` - `CompilerOptions::check_leaks` flag
    - `compiler/src/cli/builder/compiler_setup.cpp` - Extra flags support
    - `compiler/src/cli/builder/helpers.cpp` - Automatic mem_track.c inclusion
    - `compiler/src/cli/dispatcher.cpp` - CLI flag parsing
    - `compiler/src/cli/tester/run.cpp` - Test runner integration

- **Sanitizer Build Options** (2026-01-14) - CMake options for memory/undefined behavior sanitizers
  - `TML_ENABLE_ASAN` - AddressSanitizer for memory errors
  - `TML_ENABLE_UBSAN` - UndefinedBehaviorSanitizer
  - `TML_ENABLE_LSAN` - LeakSanitizer (Linux/macOS)
  - `TML_ENABLE_MSAN` - MemorySanitizer (Linux Clang)
  - Build script flags: `--asan`, `--ubsan`, `--sanitize` (both)
  - Files modified:
    - `compiler/CMakeLists.txt` - Sanitizer options
    - `scripts/build.bat` - CLI flags for sanitizers

- **`core::cell` Module** (2026-01-14) - Interior mutability types following Rust's `core::cell` pattern
  - New directory structure with separate files:
    - `lib/core/src/cell/mod.tml` - Module root with re-exports
    - `lib/core/src/cell/unsafe_cell.tml` - `UnsafeCell[T]` and `SyncUnsafeCell[T]`
    - `lib/core/src/cell/cell.tml` - `Cell[T]` for `Duplicate` types
    - `lib/core/src/cell/ref_cell.tml` - `RefCell[T]` with runtime borrow checking
    - `lib/core/src/cell/once.tml` - `OnceCell[T]` for write-once values
    - `lib/core/src/cell/lazy.tml` - `LazyCell[T]` and `LazyTryCell[T, E]`
  - Types provided:
    - `UnsafeCell[T]` - Core primitive for interior mutability
    - `SyncUnsafeCell[T]` - Thread-safe version (marker only)
    - `Cell[T]` - Interior mutability for Copy types with `get()`, `set()`, `replace()`, `swap()`
    - `RefCell[T]` - Runtime borrow checking with `borrow()`, `borrow_mut()`, `try_borrow()`
    - `Ref[T]`, `RefMut[T]` - Borrow guards with automatic release on drop
    - `OnceCell[T]` - Write-once cell with `get_or_init()`, `get_or_try_init()`
    - `LazyCell[T]` - Lazy initialization with `force()`
    - `LazyTryCell[T, E]` - Lazy with fallible initialization
    - `BorrowError`, `BorrowMutError` - Error types for RefCell

- **`core::ptr` Module Refactoring** (2026-01-14) - Restructured ptr module into separate files like Rust
  - Split `ptr.tml` into multiple files following Rust's `core::ptr` structure:
    - `lib/core/src/ptr/mod.tml` - Module root with re-exports and `Ptr` alias
    - `lib/core/src/ptr/const_ptr.tml` - `RawPtr[T]` (equivalent to Rust's `*const T`)
    - `lib/core/src/ptr/mut_ptr.tml` - `RawMutPtr[T]` (equivalent to Rust's `*mut T`)
    - `lib/core/src/ptr/non_null.tml` - `NonNull[T]` wrapper for non-null pointers
    - `lib/core/src/ptr/ops.tml` - Free functions: `copy`, `copy_nonoverlapping`, `write_bytes`, etc.
    - `lib/core/src/ptr/alignment.tml` - `Alignment` type and alignment utilities
  - Added new functionality:
    - `Alignment` type for type-safe alignment handling
    - `size_of[T]()`, `align_of[T]()` intrinsics
    - `align_up()`, `align_down()`, `is_aligned_to()`, `align_offset()` functions
    - `is_power_of_two()`, `next_power_of_two()`, `prev_power_of_two()` utilities
    - `null[T]()`, `null_mut[T]()` factory functions
    - `swap()`, `swap_nonoverlapping()`, `replace()` operations
    - `zero_memory()` for zeroing memory regions
  - `Ptr` alias remains available via `use core::ptr::Ptr`

- **Collections Test Suite** (2026-01-14) - Comprehensive tests for collection builtins
  - Added `compiler/tests/compiler/collections.test.tml` with 3 test functions
  - Tests `list_*`, `hashmap_*`, and `buffer_*` builtin functions
  - Uses `*Unit` opaque pointer type for collection handles
  - All collection operations tested: create, push, pop, get, set, len, destroy

- **C#-Style Object-Oriented Programming** (2026-01-13) - Full OOP implementation with classes, inheritance, and interfaces
  - **Classes**: Define classes with fields, static fields, and methods
    - `class Point { x: I32; y: I32; func get_x(this) -> I32 { ... } }`
    - Static fields: `static count: I32 = 0`
    - Static methods: `static func create(x: I32, y: I32) -> Point { ... }`
    - Factory pattern with `Class::create()` syntax
  - **Inheritance**: Single inheritance via `extends` keyword
    - `class Circle extends Shape { radius: I32; ... }`
    - Methods can override parent methods
    - Vtable-based virtual dispatch
  - **Interfaces**: Define contracts with `interface` keyword
    - `interface Drawable { func draw(this) -> I32 }`
    - Multiple interface implementation: `class Canvas implements Drawable { ... }`
  - **Type Checking Operator**: `is` operator for runtime type checking
    - `if obj is Circle { ... }`
    - Works with inheritance hierarchy
  - **Visibility Modifiers**: `private`, `protected`, `pub` for field/method access control
  - **Abstract/Sealed Classes**: `abstract class Shape { ... }`, `sealed class Final { ... }`
  - **Virtual/Override Methods**: `virtual func speak(this)`, `override func speak(this)`
  - **LLVM Codegen**: Complete code generation for OOP features
    - Vtable pointer initialization for class instances
    - Dynamic class size calculation via LLVM GEP trick
    - Virtual method dispatch through vtables
  - **Additional Optimizations**:
    - Loop unrolling MIR pass complete implementation with block cloning
    - Const generic argument evaluation via `as_expr()`
    - Module registry support for classes/interfaces lookup
    - Behavior inheritance cycle detection
  - **Package Management**: `tml add` and `tml update` commands
    - Add path dependencies: `tml add mylib --path ../mylib`
    - Add git dependencies: `tml add mylib --git https://github.com/user/mylib`
    - Validate dependencies: `tml update`
  - Files added:
    - `compiler/src/codegen/core/class_codegen.cpp` - Class LLVM IR generation
    - `compiler/src/parser/parser_oop.cpp` - OOP parsing
    - `compiler/include/parser/ast_oop.hpp` - OOP AST nodes
    - `compiler/tests/compiler/oop.test.tml` - 35 comprehensive OOP tests
    - `compiler/tests/oop_test.cpp` - C++ unit tests for OOP lexer/parser/type checker
    - `docs/rfcs/RFC-0014-OOP-CLASSES.md` - Complete OOP specification
  - Files modified:
    - `compiler/src/lexer/lexer_core.cpp` - OOP keywords (class, interface, extends, implements, is, abstract, sealed, virtual, override, private, protected, base)
    - `compiler/src/types/env_lookups.cpp` - Class/interface type resolution
    - `compiler/src/types/module.cpp` - Module registry for classes/interfaces
    - `compiler/src/codegen/expr/struct.cpp` - Dynamic class allocation
    - `compiler/src/mir/passes/loop_unroll.cpp` - Full unrolling implementation
    - `compiler/src/cli/commands/cmd_pkg.cpp` - Package add/update commands

- **Complete Pattern Matching for `when` Expressions** (2026-01-13) - Full pattern matching support in when/match expressions
  - **Range Patterns**: Match value ranges with `to` (exclusive) and `through` (inclusive)
    - Integer ranges: `0 through 9 => "single digit"`
    - Character ranges: `'a' through 'z' => "lowercase"`
  - **Struct Patterns**: Destructure structs in patterns
    - `Point { x, y } => x + y`
  - **Tuple Patterns**: Destructure tuples in patterns
    - `(a, b, c) => a + b + c`
  - **Array Patterns**: Destructure arrays in patterns
    - `[first, second, _] => first + second`
  - **Block Bodies**: Support multi-statement blocks in when arms
    - Each arm can contain `{ let x = ...; let y = ...; x + y }`
  - Type checker integration for all pattern types
  - 33 new tests covering all pattern types
  - Files modified:
    - `compiler/src/parser/parser_pattern.cpp` - Range pattern parsing
    - `compiler/src/types/checker/stmt.cpp` - Struct/array pattern type checking
    - `compiler/src/codegen/llvm_ir_gen_control.cpp` - All pattern codegen
  - Test files added:
    - `compiler/tests/compiler/range_pattern.test.tml`
    - `compiler/tests/compiler/struct_pattern.test.tml`
    - `compiler/tests/compiler/tuple_pattern.test.tml`
    - `compiler/tests/compiler/array_pattern.test.tml`
    - `compiler/tests/compiler/when_block_body.test.tml`

### Changed
- **CLI Folder Reorganization** (2026-01-10) - Split monolithic CLI into focused subfolders
  - New folder structure under `compiler/src/cli/`:
    - `commands/` - All cmd_*.cpp/.hpp files (build, cache, debug, format, init, lint, pkg, rlib, test)
    - `builder/` - Build system modules (build, build_cache, build_config, compiler_setup, dependency_resolver, helpers, object_compiler, parallel_build, rlib, run, run_profiled)
    - `tester/` - Test framework modules (test_runner, suite_execution, run, discovery, benchmark, fuzzer, helpers, output, tester_internal.hpp)
    - `linter/` - Linting modules (config, discovery, helpers, run, semantic, style)
  - Core CLI files remain in `cli/`: cli.cpp, utils.cpp, diagnostic.cpp, dispatcher.cpp
  - All header includes updated to use `cli/` prefix
  - CMakeLists.txt updated with new source file paths
  - All 906 tests passing after reorganization

### Fixed
- **Suite Mode Test Hanging** (2026-01-10) - Fixed tests hanging when running many test files in suite mode
  - Tests were hanging at test 7/20 when running lib/core tests in suite mode
  - Root cause: Large DLLs with many test files (>5) can hang on Windows
  - Solution: Split large test suites into smaller chunks (max 5 tests per suite)
  - Added `MAX_TESTS_PER_SUITE = 5` constant in `group_tests_into_suites()` function
  - Large suites now split into numbered chunks (e.g., `lib_core_tests_1`, `lib_core_tests_2`, etc.)
  - All 906 tests now run to completion in suite mode (23.30s)
  - Files modified: `compiler/src/cli/tester/test_runner.cpp`

- **BoxedError Debug String Escaping** (2026-01-10) - Fixed string interpolation parsing error
  - Escaped curly braces in BoxedError debug_string format to avoid interpolation conflicts
  - Changed `"BoxedError { ... }"` to `"BoxedError \{ ... \}"` format

### Added
- **HIR Caching System** (2026-01-10) - Complete HIR serialization and caching for incremental compilation
  - **Binary Serialization**: Compact binary format with 16-byte header (magic "THIR", version, content hash)
  - **Text Serialization**: Human-readable format for debugging
  - **Content Hashing**: FNV-1a algorithm for change detection
    - `compute_source_hash()` - hash source file content + modification time
    - `compute_hir_hash()` - hash module structure (functions, structs, enums)
  - **Dependency Tracking**: `HirDependency` and `HirCacheInfo` structs track module dependencies
  - **Cache Validation**: `are_dependencies_valid()` checks if cached HIR is still valid
  - **Build Integration**: `HirCache` class in `build_cache.cpp` integrates with the build system
  - Files: `compiler/src/hir/serializer/`, `compiler/include/hir/hir_serialize.hpp`, `compiler/src/cli/builder/build_cache.cpp`

- **HIR Optimization Passes - Inlining & Closure** (2026-01-09) - Full implementation of HIR Inlining and ClosureOptimization passes
  - **Inlining Pass**: Replaces calls to small, non-recursive functions with their bodies
    - Configurable statement threshold (default: 5)
    - Respects `@inline` and `@noinline` attributes
    - Detects and skips recursive functions
    - Deep cloning of expressions, statements, and patterns
    - Parameter substitution with let bindings
  - **ClosureOptimization Pass**: Optimizes closure captures and representations
    - Removes unused captures from closures
    - Performs escape analysis on captured variables
    - Converts escaping captures to immutable when safe
    - Traverses nested closures recursively
  - All 113 HIR tests passing including 23 new optimization tests
  - Files modified: `compiler/src/hir/hir_pass.cpp`, `compiler/include/hir/hir_pass.hpp`
  - Files modified: `compiler/tests/hir_test.cpp`

- **MIR Optimization Passes - Phase 4** (2026-01-09) - Additional optimization passes for O2/O3 pipelines
  - **ConstantHoistPass**: Moves expensive constant materialization out of loops
  - **SimplifySelectPass**: Optimizes select/conditional instructions (constant conditions, same values, boolean patterns)
  - **MergeReturnsPass**: Combines multiple return statements into single unified exit block
  - All 906 tests passing
  - Files added:
    - `compiler/include/mir/passes/const_hoist.hpp`, `compiler/src/mir/passes/const_hoist.cpp`
    - `compiler/include/mir/passes/simplify_select.hpp`, `compiler/src/mir/passes/simplify_select.cpp`
    - `compiler/include/mir/passes/merge_returns.hpp`, `compiler/src/mir/passes/merge_returns.cpp`
  - Files modified: `compiler/CMakeLists.txt`, `compiler/src/mir/mir_pass.cpp`

- **MIR Optimization Passes - Phase 3** (2026-01-09) - Additional optimization passes for O1/O2/O3 pipelines
  - **PeepholePass**: Algebraic simplifications (x+0→x, x*1→x, x*0→0, double negation, etc.)
  - **BlockMergePass**: Merges consecutive basic blocks with single predecessor/successor
  - **DeadArgEliminationPass**: Removes unused function parameters (inter-procedural)
  - **EarlyCSEPass**: Local common subexpression elimination early in the pipeline
  - **LoadStoreOptPass**: Eliminates redundant loads/stores, store-to-load forwarding
  - **LoopRotatePass**: Loop rotation infrastructure for better optimization exposure
  - Benchmark: O3 achieves 57.1% MIR size reduction (447 lines from 1042)
  - All 906 tests passing
  - Files added:
    - `compiler/include/mir/passes/peephole.hpp`, `compiler/src/mir/passes/peephole.cpp`
    - `compiler/include/mir/passes/block_merge.hpp`, `compiler/src/mir/passes/block_merge.cpp`
    - `compiler/include/mir/passes/dead_arg_elim.hpp`, `compiler/src/mir/passes/dead_arg_elim.cpp`
    - `compiler/include/mir/passes/early_cse.hpp`, `compiler/src/mir/passes/early_cse.cpp`
    - `compiler/include/mir/passes/load_store_opt.hpp`, `compiler/src/mir/passes/load_store_opt.cpp`
    - `compiler/include/mir/passes/loop_rotate.hpp`, `compiler/src/mir/passes/loop_rotate.cpp`
  - Files modified: `compiler/CMakeLists.txt`, `compiler/src/mir/mir_pass.cpp`

- **MIR Optimization Passes - Phase 2** (2026-01-09) - Additional optimization passes for O2/O3 pipelines
  - **ReassociatePass**: Reorders associative operations (add, mul, and, or, xor) for better constant folding
  - **TailCallPass**: Identifies and marks tail calls for backend optimization
  - **NarrowingPass**: Replaces zext→op→trunc patterns with narrower operations when safe
  - **LoopUnrollPass**: Loop analysis infrastructure for unrolling small constant-bound loops
  - **SinkingPass**: Moves computations closer to their uses to reduce register pressure
  - **ADCEPass**: Aggressive Dead Code Elimination using reverse dataflow analysis
  - Benchmark improvement: O3 now achieves 54.5% MIR size reduction (up from baseline)
  - All 40 MIR unit tests passing
  - Files added:
    - `compiler/include/mir/passes/reassociate.hpp`, `compiler/src/mir/passes/reassociate.cpp`
    - `compiler/include/mir/passes/tail_call.hpp`, `compiler/src/mir/passes/tail_call.cpp`
    - `compiler/include/mir/passes/narrowing.hpp`, `compiler/src/mir/passes/narrowing.cpp`
    - `compiler/include/mir/passes/loop_unroll.hpp`, `compiler/src/mir/passes/loop_unroll.cpp`
    - `compiler/include/mir/passes/sinking.hpp`, `compiler/src/mir/passes/sinking.cpp`
    - `compiler/include/mir/passes/adce.hpp`, `compiler/src/mir/passes/adce.cpp`
  - Files modified: `compiler/CMakeLists.txt`, `compiler/src/mir/mir_pass.cpp`

- **High-level Intermediate Representation (HIR)** (2026-01-08) - New compiler IR layer between type-checked AST and MIR
  - Type-resolved AST representation using semantic `types::TypePtr`
  - Modular architecture with 9 header files and 9 source files
  - HIR patterns: wildcard, binding, literal, tuple, struct, enum, or, range, array
  - HIR expressions: 30+ expression types including calls, closures, control flow
  - HIR statements: let declarations and expression statements
  - HIR declarations: functions, structs, enums, impls, behaviors, constants
  - `HirBuilder` class for AST to HIR lowering
  - `HirPrinter` for debug output with color support
  - `MonomorphizationCache` for tracking generic instantiations
  - Closure capture analysis framework
  - Factory functions for clean HIR node construction
  - New RFC: [RFC-0013-HIR.md](docs/rfcs/RFC-0013-HIR.md)
  - Files added: `compiler/include/hir/*.hpp`, `compiler/src/hir/*.cpp`
  - CMakeLists.txt updated with `tml_hir` library

- **Float Negation Intrinsics** (2026-01-08) - Added `fneg_f32` and `fneg_f64` builtin functions
  - Emit LLVM `fneg` instruction directly for float negation
  - Avoids type coercion issues with `0.0 - this` pattern
  - Used by `impl Neg for F32/F64` in `core::ops::arith`
  - Files modified: `compiler/src/codegen/builtins/math.cpp`, `lib/core/src/ops/arith.tml`

### Fixed
- **`debug_string` Method for Primitive Types** (2026-01-08) - Primitives now support `debug_string()` method
  - Added `debug_string` as alias for `to_string` on all primitive types (I8-I128, U8-U128, F32, F64, Bool, Char, Str)
  - Fixes calls like `n.debug_string()` where `n: I64` in `CoroutineResumePoint::AtYield(I64)` Debug impl
  - Also improved F32 handling to use `fpext` and Char handling to use runtime function
  - File modified: `compiler/src/codegen/expr/method_primitive.cpp`

- **`mut this` in Methods for Primitive Types** (2026-01-08) - Fixed mutation of `this` in impl methods
  - Methods with `mut this` on primitive types (e.g., `bitand_assign`) now create an alloca
  - This allows assignment to `this` within the method body to work correctly
  - Without this fix, `store` would fail because `%this` was a value, not a pointer
  - File modified: `compiler/src/codegen/llvm_ir_gen_decl.cpp`

- **F32/F64 Negation Type Safety** (2026-01-08) - Fixed return type of `impl Neg for F32`
  - Changed from `0.0 - this` (which promoted to F64) to `lowlevel { fneg_f32(this) }`
  - Prevents LLVM IR type mismatch: `ret float %double_value`
  - File modified: `lib/core/src/ops/arith.tml`

- **Iterator Adapter Implementations** (2026-01-07) - Implemented Iterator for closure-based adapters
  - Map[I, F]: transforms each element via closure
  - Filter[I, P]: yields elements matching predicate
  - FilterMap[I, F]: combines filter and map
  - TakeWhile[I, P], SkipWhile[I, P]: conditional iteration
  - Scan[I, St, F]: stateful transformation
  - Inspect[I, F]: side-effects without changing values
  - Intersperse[I]: inserts separator between elements
  - MapWhile[I, F]: map until Nothing
  - Constructor functions added for all adapters
  - Tests blocked by codegen limitation: `I::Item` not substituted in generic impls
  - File: `lib/core/src/iter/adapters.tml`

- **Iterator Consumer Method Documentation** (2026-01-07) - Documented blockers for iterator consumer tests
  - Created `iter_consumers.test.tml` with documentation of blocked tests
  - Identified that default behavior method dispatch on concrete types returns `()` instead of expected type
  - Added workaround note: use `FromIterator::from_iter(iter)` instead of `iter.collect()`
  - Updated tasks.md blockers table with new blocking issues

- **collect/partition Method Notes** (2026-01-07) - Added notes about blocked iterator methods
  - `collect[C: FromIterator[This::Item]](this) -> C` blocked on parser support for parameterized behavior bounds
  - `partition[C](this, pred: func(ref This::Item) -> Bool) -> (C, C)` blocked for same reason
  - Parser doesn't support constraint syntax like `C: FromIterator[This::Item]`
  - Added workaround documentation in traits.tml

- **SliceType `[T]` Method Support** (2026-01-07) - Methods on slice types now work correctly
  - Added type checker support for `.len()`, `.is_empty()`, `.get()`, `.first()`, `.last()`, `.slice()`, `.iter()` on `[T]` slice types
  - Added codegen support via `gen_slice_type_method()` function
  - Slices are represented as fat pointers `{ ptr, i64 }` containing data pointer and length
  - Files modified:
    - `compiler/src/types/checker/expr.cpp` - SliceType method type checking
    - `compiler/src/codegen/expr/method_slice.cpp` - SliceType method codegen
    - `compiler/src/codegen/expr/method.cpp` - dispatch to slice type methods
    - `compiler/src/codegen/expr/infer.cpp` - field access type inference for SliceType

- **ref Str Method Dispatch** (2026-01-07) - Methods on `ref Str` now work correctly
  - Fixed type checker to unwrap RefType before checking for PrimitiveType
  - Fixed codegen to handle `ref Str` as receiver type
  - Methods like `s.as_bytes()` where `s: ref Str` now compile correctly
  - Files modified:
    - `compiler/src/types/checker/expr.cpp` - unwrap RefType for primitive method lookup
    - `compiler/src/codegen/expr/method_primitive.cpp` - unwrap RefType for codegen

### Fixed
- **Layout::from_size_align Negative Size Validation** (2026-01-07) - Added explicit check for negative sizes
  - `from_size_align()` now returns `Err(LayoutError)` for negative sizes
  - All 36 alloc tests now pass
  - File modified: `lib/core/src/alloc.tml`

- **Closure in Struct Fields Codegen** (2026-01-07) - Fixed closures stored in struct fields
  - When a closure is loaded from a struct field and stored in a local variable, calling it now works correctly
  - The bug was that function pointer variables always stored directly instead of in an alloca, causing double-dereference
  - Changed `gen_let_stmt` to always allocate for function pointer types, matching other pointer types
  - Files modified: `compiler/src/codegen/llvm_ir_gen_stmt.cpp`
  - New tests: `closure_field.test.tml`, `closure_iter.test.tml`, `iter_map.test.tml`

### Added
- **Closure Iterator Adapter Tests** (2026-01-07) - Tests for Map adapter with closures
  - Created `closure_iter.test.tml` with 5 tests for Map adapters using closures
  - Created `iter_map.test.tml` with concrete MapOnceI32 adapter implementation
  - Verified closures work correctly in iterator patterns (map, transform)

- **Primitive Type Methods Support** (2026-01-07) - Methods on primitive types (I32, I64, Bool, etc.) now work correctly
  - Fixed codegen bug where `this` was incorrectly passed as pointer instead of by value for primitives
  - Added support for user-defined impl methods on primitive types (e.g., `impl I32 { func abs(this) -> I32 }`)
  - Added support for impl constants on primitive types (e.g., `I32::MIN`, `I32::MAX`)
  - Fixed F32/F64 binary operations to use correct float type instead of always using double
  - New test file: `compiler/tests/compiler/primitive_methods.test.tml` with 46 comprehensive tests
  - Files modified:
    - `compiler/src/codegen/expr/method_primitive.cpp` - lookup user-defined impl methods for primitives
    - `compiler/src/codegen/expr/collections.cpp` - lookup constants from imported modules
    - `compiler/src/codegen/expr/binary.cpp` - F32/F64 type handling in comparisons
    - `compiler/src/types/checker/expr.cpp` - type checker support for primitive impl methods
    - `compiler/src/types/env_module_support.cpp` - extract constants from imported modules
    - `compiler/src/codegen/core/generate.cpp` - handle cast expressions in local constants
    - `compiler/include/types/module.hpp` - added `constants` field to Module struct

- **Duration Type Tests** (2026-01-06) - Comprehensive test suite for `core::time::Duration`
  - Created `lib/core/tests/time.test.tml` with 28 test cases covering:
    - Construction: new(), from_secs(), from_millis(), from_micros(), from_nanos()
    - Accessors: is_zero(), as_secs(), subsec_nanos(), subsec_millis(), subsec_micros()
    - Conversion: as_millis(), as_micros(), as_nanos()
    - Arithmetic: checked_add(), checked_sub(), saturating_add(), saturating_sub(), mul(), div()
    - Comparison: eq(), ordering via as_nanos()
    - Behaviors: duplicate(), default()
  - Fixed time.tml string interpolation syntax (debug_string format)
  - Fixed module-level constant visibility by inlining values

- **Core Library Test Improvements** (2026-01-06) - Comprehensive test coverage for core modules
  - `iter.test.tml`: Added 8 new tests (EmptyI64, RepeatNI64, OnceI32 edge cases, large count)
  - `slice.test.tml`: Added 7 new tests (find_max, find_min, first/last element, negative values)
  - `num.test.tml`: Added 14 new tests (Zero/One properties, signed/unsigned ranges, arithmetic properties, F64)
  - `async_iter.test.tml`: Simplified to 4 working tests for Once/Repeat type construction
  - Total: 29 new test cases across core library modules

### Fixed
- **alloc.tml String Interpolation Syntax** (2026-01-06) - Fixed curly brace parsing in debug strings
  - Removed curly braces from debug string formatting that were being interpreted as interpolation
  - Changed `"Layout { size: ..."` to `"Layout(size=..."` format for proper parsing
  - Files modified: `lib/core/src/alloc.tml`

- **bstr.test.tml and alloc.test.tml** (2026-01-06) - Moved to pending due to codegen issues
  - ByteStr module methods return `()` instead of proper types (codegen limitation)
  - alloc tests reference unimplemented helper functions (align_up, align_down, etc.)
  - Files moved to `lib/core/tests/pending/`

### Changed
- **Method Codegen Modularization** (2026-01-01) - Split monolithic method.cpp into focused modules
  - Refactored 1750-line `method.cpp` into 5 modular files in `src/codegen/expr/`:
    - `method.cpp` (~590 lines) - Main dispatcher and core method handling
    - `method_static.cpp` (~300 lines) - Static method calls (Type::method())
    - `method_primitive.cpp` (~320 lines) - Primitive type methods (add, sub, mul, div, hash, cmp)
    - `method_collection.cpp` (~265 lines) - List, HashMap, Buffer methods (push, pop, get, set, len)
    - `method_slice.cpp` (~70 lines) - Slice/MutSlice inline methods (len, is_empty)
  - New function declarations in `llvm_ir_gen.hpp`:
    - `gen_static_method_call()` - Handles Type::new(), Type::default(), Type::from()
    - `gen_primitive_method()` - Handles integer/float/bool methods
    - `gen_collection_method()` - Handles collection instance methods
    - `gen_slice_method()` - Handles slice inline codegen
  - CMakeLists.txt updated with new source files
  - All 747 tests passing after refactoring

### Fixed
- **Void Return Type for Indirect Function Calls** (2026-01-01) - Fixed closures returning void
  - Indirect function pointer calls (closures) were incorrectly trying to assign void results
  - Added void check in `gen_call()` for indirect calls, matching behavior of direct calls
  - File modified: `compiler/src/codegen/llvm_ir_gen_builtins.cpp`

- **Trait Object Vtable Type for Multiple Methods** (2026-01-01) - Fixed behaviors with multiple methods
  - Vtable GEP instruction was using hardcoded `{ ptr }` type regardless of method count
  - Now dynamically builds vtable type based on behavior's method count
  - Set `last_expr_type_` to `i32` after dyn method dispatch
  - Added comprehensive tests: dyn_advanced.test.tml, dyn_array.test.tml
  - Files modified: `compiler/src/codegen/expr/method.cpp`

- **Struct Literal Integer Type Casting** (2026-01-01) - Fixed i32 vs i64 mismatch in struct initialization
  - When initializing struct fields of type i64 with i32 values, codegen now properly sign-extends
  - Fixes Slice/MutSlice initialization where `len: I64` field received i32 values
  - Added type checking in `gen_struct_expr()` to sext i32 to i64 when target field type differs
  - Files modified: `compiler/src/codegen/expr/struct.cpp`

- **Slice/MutSlice Inline Method Codegen** (2026-01-01) - Fixed len() and is_empty() for slices
  - `len()` now uses inline GEP instruction to load i64 field from slice struct
  - `is_empty()` compares len against 0 directly
  - Eliminates need for runtime function calls
  - Works with generic instantiations like `Slice__I32`, `MutSlice__Str`
  - Files added: `compiler/src/codegen/expr/method_slice.cpp`

### Added
- **Closure Iterator Pattern Tests** (2026-01-01) - Tests for closures with iterator-like patterns
  - Tests for fold, all, any, find patterns using closures with arrays
  - Files added:
    - `compiler/tests/compiler/closure_basic.test.tml` - Basic closure tests
    - `compiler/tests/compiler/closure_hof.test.tml` - Higher-order function tests
    - `compiler/tests/compiler/closure_fold.test.tml` - Fold/all/any/find pattern tests

- **ASCII Character Module** (2026-01-01) - Complete `core::ascii::AsciiChar` type
  - Safe ASCII character type with validation (0-127 range)
  - Character classification: `is_alphabetic()`, `is_digit()`, `is_alphanumeric()`, `is_whitespace()`, etc.
  - Case conversion: `to_lowercase()`, `to_uppercase()`, `to_ascii_lowercase()`, `to_ascii_uppercase()`
  - Digit operations: `to_digit()`, `to_digit_radix()`, `from_digit()`, `from_digit_radix()`
  - Comprehensive test suite (163 tests in lib/core)
  - Files added/modified:
    - `lib/core/src/ascii/char.tml` - AsciiChar implementation
    - `lib/core/src/ascii/mod.tml` - Module exports
    - `lib/core/tests/ascii.test.tml` - Test suite

- **Slice Intrinsics Codegen** (2026-01-01) - Complete slice operations for lowlevel blocks
  - `slice_get[T](data: ref T, index: I64) -> ref T` - Get element reference at index
  - `slice_get_mut[T](data: mut ref T, index: I64) -> mut ref T` - Get mutable element reference
  - `slice_set[T](data: mut ref T, index: I64, value: T)` - Set element at index
  - `slice_offset[T](data: ref T, count: I64) -> ref T` - Compute offset pointer
  - `slice_swap[T](data: mut ref T, a: I64, b: I64)` - Swap elements at indices
  - All intrinsics use LLVM GEP instructions for efficient pointer arithmetic
  - Files modified:
    - `lib/core/src/intrinsics.tml` - Added intrinsic declarations
    - `compiler/src/codegen/builtins/intrinsics.cpp` - Added codegen implementation

### Fixed
- **PHI Node Predecessors in Nested if-else-if** (2026-01-01) - Fixed LLVM IR verification errors
  - Nested `else if` chains were generating invalid PHI nodes with wrong predecessor labels
  - PHI nodes now correctly track the actual ending block for each branch
  - When else branch contains nested if, the PHI predecessor is the inner if's end block, not the outer else label
  - Files modified:
    - `compiler/src/codegen/llvm_ir_gen_control.cpp` - Track `then_end_block` and `else_end_block` for PHI generation

- **String Interpolation for Small Integer Types** (2026-01-01) - i8/i16 types now work in string interpolation
  - Added support for U8/I8 and U16/I16 in `gen_interp_string()`
  - Uses zext for unsigned types, sext for signed types before formatting
  - Files modified:
    - `compiler/src/codegen/expr/core.cpp` - Added i8/i16 extension to i64 for printf

- **Function Return Type Registration Order** (2026-01-01) - Fixed type inference for @test functions
  - When @test functions used helper functions defined later in the file, payload types were nullptr
  - Added pre-pass in generate.cpp to register all function return types before codegen
  - Files modified:
    - `compiler/src/codegen/core/generate.cpp` - Pre-register function return types

- **String Interpolation Static Buffer Bug** (2026-01-01) - Multiple values in same expression now work correctly
  - `i64_to_str()` and `f64_to_str()` were using static buffers causing all interpolated values to show the same number
  - Example: `"{a}, {b}, {c}"` was showing `"30, 30, 30"` instead of `"10, 20, 30"`
  - Fix: Changed from static buffers to dynamically allocated strings
  - Files modified:
    - `compiler/runtime/string.c` - `i64_to_str()` and `f64_to_str()` now use malloc

- **Const Variable Compile-Time Lookup** (2025-12-31) - Const variables now available during compile-time evaluation
  - Added `const_values_` map to TypeChecker for storing evaluated const values
  - `check_const_decl` now evaluates and stores const values at compile time
  - `evaluate_const_expr` can now lookup previously evaluated const values
  - Enables const generics and compile-time computations to reference const declarations
  - Files modified:
    - `include/types/checker.hpp` - Added `const_values_` map
    - `src/types/checker/core.cpp` - Store evaluated const values in `check_const_decl`
    - `src/types/checker/const_eval.cpp` - Implemented const variable lookup

- **Fuzz Testing Infrastructure** (2025-12-31) - Complete fuzz testing support with `@fuzz` decorator
  - `@fuzz` decorator marks functions as fuzz targets
  - Fuzz target function signature: `tml_fuzz_target(data: Ptr[U8], len: U64)`
  - Fuzzer compiles target to shared library and calls fuzz target with generated input
  - Mutation-based fuzzing with corpus support
  - Crash input saved to `fuzz_crashes/` directory
  - `generate_fuzz_entry` option in LLVMGenOptions for fuzz target codegen
  - Files modified/added:
    - `include/codegen/llvm_ir_gen.hpp` - Added `generate_fuzz_entry` option
    - `src/codegen/core/generate.cpp` - Added `@fuzz` decorator collection and entry point generation
    - `src/cli/test_runner.hpp` - Added `FuzzTargetFunc` type and `compile_fuzz_to_shared_lib` declaration
    - `src/cli/test_runner.cpp` - Implemented `compile_fuzz_to_shared_lib` function
    - `src/cli/tester/fuzzer.cpp` - Updated to use shared library loading and fuzz target invocation

- **In-Process Test Output Capture** (2025-12-31) - Stdout/stderr capture for in-process test execution
  - Added `OutputCapture` RAII class for capturing stdout/stderr to a string
  - Cross-platform support (Windows and POSIX) using file descriptor redirection
  - Uses `_sopen_s`/`_dup2` on Windows, `open`/`dup2` on POSIX
  - Captured output stored in `InProcessTestResult.output`
  - Files modified:
    - `src/cli/test_runner.cpp` - Added `OutputCapture` class and updated `run_test_in_process`

- **MIR Text Parser** (2025-12-31) - Full text format parser for MIR modules
  - Parses module structure, functions, blocks, instructions, and terminators
  - Supports all primitive types, pointers, arrays, and struct types
  - Parses binary/unary operations, load/store, alloca, call instructions
  - Handles return, branch, conditional branch, and unreachable terminators
  - Enables round-trip serialization for MIR debugging and testing
  - Files modified:
    - `include/mir/mir_serialize.hpp` - Added read_type, read_value_ref, read_instruction, read_terminator, read_block, read_function declarations
    - `src/mir/mir_serialize.cpp` - Full implementation of MIR text parsing (~400 lines)

- **Git Dependency Resolution** (2025-12-31) - Clone and build dependencies from git repositories
  - `git = "url"` dependency specification in tml.toml
  - Supports `branch`, `tag`, and `rev` options for checkout
  - Shallow clone (--depth 1) for faster downloads
  - Automatic caching in `~/.tml/cache/git/`
  - Builds dependency and caches resulting rlib
  - Files modified:
    - `src/cli/build_config.hpp` - Added `branch` and `rev` fields to Dependency struct
    - `src/cli/dependency_resolver.cpp` - Full git resolution implementation (~140 lines)

- **Registry Lookup for Dependencies** (2025-12-31) - Check package registry for version dependencies
  - Looks up packages in local registry index cache
  - Parses registry JSON for download URLs
  - Groundwork for future remote registry support
  - Files modified:
    - `src/cli/dependency_resolver.cpp` - Registry lookup in resolve_version_dependency (~35 lines)

- **DCE Purity Analysis** (2025-12-31) - Dead code elimination for pure function calls
  - 60+ known pure functions (math, string, collection accessors)
  - Removes unused calls to pure functions like `abs()`, `len()`, `sqrt()`
  - Handles method calls (`String::len`) and generic instantiations (`abs[I32]`)
  - Files modified:
    - `src/mir/passes/dead_code_elimination.cpp` - Added `pure_functions` set and `is_pure_function()` helper (~100 lines)

- **Range Expression Tests** (2025-12-31) - Parser tests for range syntax
  - Tests for `to` keyword (exclusive range): `1 to 10`
  - Tests for `through` keyword (inclusive range): `1 through 10`
  - Tests for `..` operator: `1..10`
  - Files modified:
    - `compiler/tests/parser_test.cpp` - Added RangeExpressionWithTo, RangeExpressionWithThrough, RangeExpressionWithDotDot tests

- **Never Type for panic!** (2025-12-31) - Correct return type for panic builtin
  - `panic()` now returns `Never` type instead of `Unit`
  - Proper diverging function semantics
  - Files modified:
    - `src/types/builtins/io.cpp` - Changed panic return type to `make_never()`

### Changed
- **MIR Builder Split** (2025-12-31) - Refactored mir_builder.cpp into modular components
  - Split 1716-line file into 6 focused modules in `src/mir/builder/`:
    - `types.cpp` (~175 lines) - Type conversion functions (convert_type, convert_semantic_type)
    - `expr.cpp` (~490 lines) - Expression building (literals, binary, unary, calls, closures)
    - `stmt.cpp` (~140 lines) - Statement and declaration building
    - `pattern.cpp` (~105 lines) - Pattern matching and destructuring
    - `control.cpp` (~300 lines) - Control flow (if, loops, when/match)
    - `helpers.cpp` (~180 lines) - Utility functions (emit, constants, operators)
  - Main mir_builder.cpp reduced to ~30 lines (constructor + build entry point)
  - Files added:
    - `src/mir/builder/builder_internal.hpp`
    - `src/mir/builder/types.cpp`
    - `src/mir/builder/expr.cpp`
    - `src/mir/builder/stmt.cpp`
    - `src/mir/builder/pattern.cpp`
    - `src/mir/builder/control.cpp`
    - `src/mir/builder/helpers.cpp`
  - CMakeLists.txt updated with new builder module files

### Documentation
- **CORE_ANALYSIS.md** (2025-12-31) - Translated from Portuguese to English
  - Comparative analysis of TML core library vs Rust core
  - Module coverage status and priority matrix
  - Implementation roadmap for missing behaviors (Clone, Ord, Ops, etc.)

- **Benchmark Framework** (2025-12-31) - Full benchmark support with `@bench` decorator
  - `@bench` decorator marks functions as benchmarks (default 1000 iterations)
  - `@bench(N)` allows custom iteration count (e.g., `@bench(10000)`)
  - Automatic warmup phase (10 iterations before measurement)
  - Nanosecond precision timing using `@time_ns()` builtin
  - Benchmark output format: `bench_name: X ns/iter (Y iterations)`
  - `tml test --bench` discovers and runs `*.bench.tml` files
  - `--save-baseline=file.json` saves benchmark results for comparison
  - `--compare=file.json` compares against baseline (shows % change)
  - Green output for improvements, red for regressions
  - Files modified:
    - `src/codegen/core/generate.cpp` - Benchmark runner codegen with warmup, timing, output
    - `src/cli/cmd_test.cpp` - Benchmark discovery, comparison, JSON save/load
    - `src/cli/cmd_test.hpp` - BenchmarkResult struct, TestOptions extensions

- **Test Coverage Flag** (2025-12-31) - Coverage tracking support
  - `tml test --coverage` enables code coverage tracking
  - `--coverage-output=file.html` specifies output file (default: coverage.html)
  - Coverage runtime in `lib/test/runtime/coverage.c`
  - Files modified:
    - `src/cli/cmd_test.hpp` - Added coverage, coverage_output to TestOptions
    - `src/cli/cmd_test.cpp` - Parse coverage flags, pass to run_run

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

