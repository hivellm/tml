# TML v1.0 — Toolchain CLI

## 1. Overview

`tml` is the unified command-line tool for TML.

```bash
tml --version
# tml 1.0.0 (abc1234 2025-12-27)

tml --help
# TML Toolchain
#
# USAGE:
#     tml <COMMAND> [OPTIONS]
#
# COMMANDS:
#     init        Initialize a new project (IMPLEMENTED)
#     build       Compile the project (IMPLEMENTED)
#     run         Build and execute (IMPLEMENTED)
#     test        Run tests (IMPLEMENTED)
#     check       Check for errors without compiling (IMPLEMENTED)
#     fmt         Format source code (IMPLEMENTED)
#     cache       Manage build cache (IMPLEMENTED)
#     rlib        Inspect RLIB libraries (IMPLEMENTED)
#     lint        Run linter (FUTURE)
#     doc         Generate documentation (FUTURE)
#     repl        Interactive REPL (FUTURE)
#     clean       Remove build artifacts (FUTURE)
```

## 2. Main Commands

### 2.1 tml init — Initialize Project

```bash
# Initialize in current directory
tml init

# Structure created:
# ./
# ├── tml.toml
# ├── src/
# │   └── main.tml (for binary) or lib.tml (for library)
# └── build/

# Options
tml init --bin               # Create binary project (default)
tml init --lib               # Create library project
tml init --name myproject    # Set project name (default: directory name)
tml init --no-src            # Don't create src/ or source files

# Examples
tml init --lib --name my_library
tml init --bin --name my_app
```

### 2.2 tml build — Compile

```bash
# Debug build
tml build

# Release build
tml build --release

# Specific target
tml build --target wasm32
tml build --target x86_64-linux

# Library types
tml build --crate-type=lib       # Static library (.lib/.a)
tml build --crate-type=dylib     # Dynamic library (.dll/.so/.dylib)
tml build --crate-type=rlib      # TML library format (.rlib with metadata)

# C header generation for FFI
tml build --emit-header          # Generate .h file from public functions

# Other options
tml build --emit-ir              # Emit LLVM IR (.ll files)
tml build --emit-mir             # Emit MIR (Mid-level IR) for debugging
tml build --no-cache             # Disable build cache (force full recompilation)
tml build --legacy               # Use legacy sequential pipeline (bypass query system)
tml build --out-dir=path         # Specify output directory
tml build --verbose              # Show detailed build output
tml build --time                 # Show detailed compiler phase timings
tml build --lto                  # Enable Link-Time Optimization

# Backend selection (EXPERIMENTAL)
tml build --backend=llvm         # Use LLVM backend (default)
tml build --backend=cranelift    # Use Cranelift backend (experimental, in development)

# Borrow checker selection
tml build --polonius             # Use Polonius borrow checker (more permissive than NLL)

# Preprocessor options
tml build -DDEBUG                # Define preprocessor symbol
tml build -DVERSION=1.0          # Define symbol with value
tml build --define=FEATURE       # Alternative syntax
tml build --target=x86_64-linux  # Cross-compile to target triple

# Custom output directory
tml build --out-dir=path/to/dir  # Save build artifacts to custom directory

# Combined example: Library + header in custom directory
tml build mylib.tml --crate-type=lib --emit-header --out-dir=examples/ffi

# With features
tml build --features "async,serde"

# JSON output (for IDEs/LLMs)
tml build --message-format json
```

Output:
```
   Compiling myproject v1.0.0
    Building src/lib.tml
    Finished dev [unoptimized + debuginfo] in 0.54s
```

Library build output:
```bash
# Static library (default output directory)
$ tml build mylib.tml --crate-type=lib
build: f:/path/to/build/debug/mylib.lib

# Dynamic library
$ tml build mylib.tml --crate-type=dylib
build: f:/path/to/build/debug/mylib.dll

# Static library + header in custom directory
$ tml build mylib.tml --crate-type=lib --emit-header --out-dir=examples/ffi
build: examples/ffi/mylib.lib
emit-header: examples/ffi/mylib.h
```

### 2.3 tml run — Execute

```bash
# Build and run
tml run

# With arguments
tml run -- arg1 arg2 --flag

# Release mode
tml run --release

# Watch mode
tml run --watch
```

### 2.4 tml test — Tests

```bash
# All tests
tml test

# Filter
tml test add              # tests containing "add"
tml test --filter "test_*"
tml test --module math

# Parallel
tml test --test-threads=4

# Coverage
tml test --coverage
tml test --coverage --coverage-output=coverage.html

# Memory leak detection (enabled by default)
tml test --no-check-leaks    # Disable leak checking

# Verbose
tml test --verbose

# Timeout (seconds)
tml test --timeout=30

# No color output
tml test --no-color

# Skip build cache
tml test --no-cache
```

Output:
```
 TML Tests v0.1.0

 Running 65 test files...

 + compiler (42 tests)
   ✓ arithmetic.test.tml (12ms)
   ✓ control_flow.test.tml (8ms)
   ✓ functions.test.tml (15ms)
   ...

 Tests 65 passed (65)
 Duration 3.45s

 All tests passed!
```

### 2.5 tml test --bench — Benchmarks

```bash
# Run all benchmarks (discovers *.bench.tml files)
tml test --bench

# Filter by pattern
tml test --bench sorting

# Save baseline for comparison
tml test --bench --save-baseline=baseline.json

# Compare against baseline
tml test --bench --compare=baseline.json

# Release mode benchmarks (more accurate)
tml test --bench --release
```

Benchmark output:
```
 TML Benchmarks v0.1.0

 Running 1 benchmark file...

 + simple
  + bench bench_addition       ... 2 ns/iter (1000 iterations)
  + bench bench_loop           ... 156 ns/iter (10000 iterations)
  + bench bench_multiplication ... 1 ns/iter (5000 iterations)

 Bench Files 1 passed (1)
 Duration    1.23s
```

When comparing against a baseline:
```
  + bench bench_addition ... 2 ns/iter (-15.2%)   # improved (green)
  + bench bench_loop     ... 180 ns/iter (+10.5%) # regressed (red)
  + bench bench_sort     ... 45 ns/iter (~0.3%)   # unchanged (gray)
```

### 2.6 tml check — Quick Verification

```bash
# Check without generating code
tml check

# Include tests
tml check --tests
```

Faster than build, ideal for feedback during development.

### 2.7 tml fmt — Formatting

```bash
# Format all
tml fmt

# Check without modifying
tml fmt --check

# Specific file
tml fmt src/main.tml
```

### 2.8 tml lint — Linter

```bash
# All checks
tml lint

# Auto fix
tml lint --fix

# Levels
tml lint --warn all
tml lint --deny unused
```

Categories:
- `correctness` — probable errors
- `performance` — inefficiencies
- `style` — conventions
- `unused` — dead code
- `security` — vulnerabilities

### 2.9 tml doc — Documentation

```bash
# Generate docs
tml doc

# Open in browser
tml doc --open

# JSON format
tml doc --format json
```

### 2.10 tml repl — REPL

```bash
tml repl

# >>> let x: I32 = 42
# x: I32 = 42
#
# >>> x * 2
# I32 = 84
#
# >>> :type x
# I32
#
# >>> :load src/lib.tml
# Loaded 15 definitions
#
# >>> :help
# Commands:
#   :help    Show help
#   :type    Show type
#   :load    Load file
#   :clear   Clear state
#   :quit    Exit
```

### 2.11 tml ir — Generate IR

```bash
# S-expression
tml ir src/lib.tml

# JSON
tml ir src/lib.tml --format json

# To file
tml ir src/lib.tml --output lib.tml.ir
```

### 2.12 tml cache — Build Cache Management

```bash
# Show cache information
tml cache info
# Output:
#   Cache location: build/debug/.run-cache
#   Object files: 15
#   Executable files: 8
#   Total size: 245.67 MB

# Show detailed cache contents
tml cache info --verbose

# Clean old cache files (7+ days)
tml cache clean

# Clean all cache files
tml cache clean --all

# Clean files older than N days
tml cache clean --days 14

# Cache features:
# - Content-based hashing (cache hit if source unchanged)
# - LRU eviction (removes oldest files when cache > 1GB)
# - Two-level cache: object files + executables
# - 91% speedup for unchanged code
```

### 2.13 tml rlib — RLIB Library Inspection

```bash
# Show library information
tml rlib info mylib.rlib
# Output:
#   TML Library: mylib v1.0.0
#   TML Version: 0.1.0
#   Modules: 1
#   Dependencies: 2

# List public exports
tml rlib exports mylib.rlib
# Output:
#   func add(I32, I32) -> I32
#   func multiply(I32, I32) -> I32
#   struct Point { x: I32, y: I32 }

# Validate RLIB format
tml rlib validate mylib.rlib
# Output:
#   ✓ Valid archive format
#   ✓ Found metadata.json
#   ✓ Valid metadata format
#   ✓ All modules present

# RLIB features:
# - Type information for safe linking
# - Dependency tracking
# - Content-based hashing
# - JSON metadata format
```

### 2.14 tml clean — Cleanup

```bash
tml clean           # remove build/ (FUTURE)
tml clean --all     # remove everything including cache (FUTURE)
```

## 3. Dependencies

### 3.1 tml add

```bash
tml add serde
tml add tokio@1.0
tml add tokio --features "full"
tml add mylib --path ../mylib
tml add mylib --git https://github.com/user/mylib
```

### 3.2 tml remove

```bash
tml remove serde
```

### 3.3 tml update

```bash
tml update          # all
tml update serde    # specific
tml update --dry-run
```

### 3.4 tml tree

```bash
tml tree

# myproject v1.0.0
# ├── serde v1.0.152
# │   └── serde_derive v1.0.152
# └── tokio v1.28.0
#     ├── bytes v1.4.0
#     └── mio v0.8.6
```

## 4. Workspaces

```bash
tml build --workspace
tml build -p core          # specific package
tml test --workspace
```

## 5. Configuration

### 5.1 tml.toml

```toml
[package]
name = "myproject"
version = "1.0.0"
edition = "2025"

[build]
jobs = 8
incremental = true

[test]
threads = 4
timeout = 30

[fmt]
max_width = 100
indent_size = 4

[lint]
deny = ["unused", "deprecated"]
```

### 5.2 Environment Variables

```bash
TML_HOME           # ~/.tml
TML_CACHE_DIR      # package cache
TML_LOG            # log level (error, warn, info, debug)
TML_JOBS           # parallel jobs
```

## 6. Output for LLMs

### 6.1 Structured JSON

```bash
tml build --message-format json
```

```json
{"reason":"compiler-artifact","target":"lib","success":true}
{"reason":"compiler-message","message":{"code":"E001","level":"error","message":"Type mismatch","spans":[{"file":"src/lib.tml","line":42,"column":15,"id":"@let_result"}]}}
{"reason":"build-finished","success":false}
```

### 6.2 Diagnostics for LLM

```bash
tml check --error-format llm
```

Optimized output with:
- Expanded context
- Suggestions as patches
- References by @id
- Type metadata

## 7. IDE Integration

### 7.1 Language Server

```bash
tml lsp
```

Features:
- Auto-complete
- Go to definition
- Find references
- Hover docs
- Diagnostics
- Code actions
- Rename
- Format

### 7.2 VSCode Settings

```json
{
  "tml.server.path": "tml",
  "tml.server.args": ["lsp"],
  "tml.format.enable": true
}
```

## 8. Build System

### 8.1 Build Modes (Crate Types)

TML supports multiple output formats for libraries and executables:

```bash
# Executable (default)
tml build
tml build --crate-type bin

# Static library (C-compatible)
tml build --crate-type staticlib
# Output: libmylib.a (Linux), mylib.lib (Windows)
# Also generates: mylib.h (C header)

# Dynamic library (C-compatible)
tml build --crate-type cdylib
# Output: libmylib.so (Linux), mylib.dll (Windows)
# Also generates: mylib.h (C header)

# TML library (with metadata)
tml build --crate-type rlib
# Output: libmylib.rlib (archive with objects + metadata)
```

**Configuration in tml.toml:**
```toml
[package]
name = "mylib"
version = "1.0.0"

[lib]
crate-type = ["rlib", "cdylib"]  # Multiple outputs

[[bin]]
name = "myapp"
path = "src/main.tml"
```

### 8.2 Build Cache

TML uses a multi-level cache system for fast incremental builds:

```bash
# Build with cache (default — query pipeline + incremental compilation)
tml build

# Rebuild without cache (force full recompilation)
tml build --no-cache

# Use legacy pipeline (bypass query system)
tml build --legacy

# Clean cache
tml cache clean

# Show cache statistics
tml cache info
# Output:
# Cache: build/debug/.run-cache/
# Size: 45.2 MB
# Entries: 127
# Hit rate: 87%
```

**How it works (query-based pipeline):**

1. **Incremental query cache** (Level 1): 128-bit fingerprints for all 8 compilation stages.
   On rebuild, the RED-GREEN system detects unchanged queries and reuses cached LLVM IR.
   - Cache: `build/debug/.incr-cache/incr.bin` + `ir/<hash>.ll`
   - No-op rebuild: < 100ms

2. **Object file cache** (Level 2): Content-based hashing of LLVM IR.
   - Cache: `build/debug/.run-cache/<hash>.obj`
   - Skip LLVM compilation if IR unchanged

3. **Executable cache** (Level 3): Combined hash of all object files.
   - Cache: `build/debug/.run-cache/<hash>.exe`
   - Skip linking if nothing changed

**Cache location:**
- Debug: `build/debug/.incr-cache/` (incremental) + `build/debug/.run-cache/` (objects)
- Release: `build/release/.incr-cache/` + `build/release/.run-cache/`

### 8.3 Object Files and Linking

TML compiles through a fully self-contained pipeline with embedded LLVM and LLD:

```
Pipeline (default — query-based):
  .tml → [QueryContext: 8 memoized stages] → LLVM IR (in-memory)
                                                  ↓
                                          Embedded LLVM (in-process)
                                                  ↓
                                          Object File (.obj/.o)
                                                  ↓
                                          Embedded LLD (in-process)
                                                  ↓
                                    Output (.exe, .dll, .a, .rlib)
```

No intermediate `.ll` files are written to disk. No external tools (clang, linker) are needed.

**Build artifacts:**
```
build/debug/
├── .incr-cache/         # Incremental compilation cache
│   ├── incr.bin         # Fingerprints + dependency edges
│   └── ir/              # Cached LLVM IR strings
│       ├── a1b2c3d4.ll
│       └── a1b2c3d4.libs
├── .run-cache/          # Object file + executable cache
│   ├── abc123.obj       # Cached object files
│   └── def456.exe       # Cached executables
├── deps/                # Compiled dependencies
│   ├── essential.obj
│   └── std.rlib
├── myapp.exe            # Executable
├── libmylib.a           # Static library
├── mylib.dll            # Dynamic library
├── mylib.h              # C header
└── mylib.rlib           # TML library
```

### 8.4 C FFI Export

Export TML functions for use in C/C++:

**TML code (math.tml):**
```tml
// Public functions are automatically exported for FFI
pub func add(a: I32, b: I32) -> I32 {
    return a + b
}

pub func multiply(a: I32, b: I32) -> I32 {
    return a * b
}
```

**Build static library with header:**
```bash
tml build math.tml --crate-type lib --emit-header
# Generates:
#   - build/debug/math.lib (Windows) or libmath.a (Linux)
#   - build/debug/math.h
```

**Build dynamic library with header:**
```bash
tml build math.tml --crate-type dylib --emit-header
# Generates:
#   - build/debug/math.dll (Windows) or libmath.so (Linux)
#   - build/debug/math.h
```

**Generated header (math.h):**
```c
#ifndef TML_MATH_H
#define TML_MATH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// TML library: math
// Auto-generated C header for FFI

int32_t tml_add(int32_t a, int32_t b);
int32_t tml_multiply(int32_t a, int32_t b);

#ifdef __cplusplus
}
#endif

#endif // TML_MATH_H
```

**Use from C:**
```c
#include "math.h"
#include <stdio.h>

int main() {
    int32_t result = tml_add(5, 3);
    printf("%d\n", result);  // 8

    int32_t product = tml_multiply(4, 7);
    printf("%d\n", product);  // 28

    return 0;
}
```

**Compile C program with TML static library:**
```bash
clang my_program.c -o my_program build/debug/math.lib
./my_program
```

**Type Mapping:**
TML types are automatically mapped to C types:
- `I8` → `int8_t`, `I16` → `int16_t`, `I32` → `int32_t`, `I64` → `int64_t`
- `U8` → `uint8_t`, `U16` → `uint16_t`, `U32` → `uint32_t`, `U64` → `uint64_t`
- `F32` → `float`, `F64` → `double`
- `Bool` → `bool` (from `stdbool.h`)
- `Str` → `const char*`
- `ref T` / `Ptr[T]` → `T*`

### 8.5 Performance

**Typical build times:**
- Full build (no cache): 5-10 seconds
- Incremental (1 file changed): <1 second (10x faster)
- Test suite (45 tests): 3 seconds (with cache)

**Cache benefits:**
- ✅ Reuse compiled modules
- ✅ Skip unchanged dependencies
- ✅ Parallel compilation (implemented)
- ✅ Faster CI/CD pipelines

### 8.6 Parallel Compilation

TML supports multi-threaded compilation for faster builds:

```bash
# Auto-detect CPU cores (default)
tml build

# Specify number of threads
tml build -j4             # Use 4 threads
tml build -j8             # Use 8 threads

# Single-threaded build
tml build -j1
```

**How parallel build works:**
1. Parse imports from all source files
2. Build dependency graph
3. Detect circular dependencies (falls back to sequential if found)
4. Compile files with no dependencies first
5. As files complete, unlock dependent files
6. Worker threads process ready queue concurrently

**Build statistics:**
```
Compiling 25 files with 8 threads...
[1/25] Compiling utils.tml
[2/25] Compiling math.tml
...
Build summary:
  Total: 25 files
  Compiled: 20 files
  Cached: 5 files
  Failed: 0 files
  Time: 2.34s
```

### 8.7 Link-Time Optimization (LTO)

LTO enables whole-program optimization for maximum performance:

```bash
# Enable LTO (full)
tml build --lto

# Combine with release mode
tml build --release --lto

# LTO for libraries
tml build --crate-type=dylib --lto
```

**Benefits:**
- Cross-module inlining
- Dead code elimination
- Interprocedural constant propagation
- Better register allocation

**Trade-offs:**
- Longer compile times (especially for large projects)
- Higher memory usage during linking
- Best for release builds

### 8.8 Compiler Phase Timing

Profile compiler performance with detailed phase timing:

```bash
tml build --time
```

**Sample output:**
```
=== Compiler Phase Timings ===
Lexing              :    12.34 ms ( 5.2%)
Parsing             :    23.45 ms (10.1%)
Type Checking       :    45.67 ms (19.6%)
MIR Generation      :    34.56 ms (14.8%)
MIR Optimization    :    28.90 ms (12.4%)
LLVM Codegen        :    56.78 ms (24.4%)
Object Compilation  :    31.23 ms (13.4%)
----------------------------------------
Total               :   232.93 ms
```

### 8.9 Incremental Compilation (Red-Green Query System)

TML uses a demand-driven query system with Red-Green incremental compilation,
inspired by rustc's architecture. All 8 compilation stages are memoized queries
with 128-bit fingerprints persisted to disk between sessions.

```bash
# Normal build (query pipeline + incremental, default)
tml build

# Force full recompilation (skip incremental cache)
tml build --no-cache

# Use legacy sequential pipeline (bypass query system)
tml build --legacy

# Inspect intermediate representations
tml build --emit-mir           # Emit MIR
tml build --emit-ir            # Emit LLVM IR
```

**How incremental compilation works:**
1. On first build, all 8 queries execute and their fingerprints + dependency edges are saved to `build/debug/.incr-cache/incr.bin`
2. On rebuild, the compiler loads the previous session's cache and checks each query:
   - **GREEN**: File unchanged (fingerprint matches) → reuse cached LLVM IR
   - **RED**: File changed → recompute from that point, but downstream may still be GREEN
3. The CodegenUnit query's LLVM IR is cached to disk as `build/debug/.incr-cache/ir/<hash>.ll`

**Cache invalidation triggers:**
- Source file content changed (fingerprint mismatch)
- Build options changed (optimization level, debug info, target triple, defines, coverage)
- Library environment changed (`.tml.meta` files modified)
- Cache format version mismatch

**Cache location:**
- `build/debug/.incr-cache/` — Incremental query cache (fingerprints + LLVM IR)
- `build/debug/.run-cache/` — Object file and executable cache

### 8.10 Conditional Compilation

TML supports C-style preprocessor directives for conditional compilation:

```bash
# Define symbols via CLI
tml build file.tml -DDEBUG              # Define DEBUG symbol
tml build file.tml -DVERSION=1.0        # Define with value
tml build file.tml --define=FEATURE     # Alternative syntax

# Multiple defines
tml build file.tml -DDEBUG -DFEATURE_X -DLOG_LEVEL=3

# Cross-compilation
tml build file.tml --target=x86_64-unknown-linux-gnu
tml build file.tml --target=aarch64-apple-darwin
tml build file.tml --target=x86_64-pc-windows-msvc
```

**Build mode symbols:**
- `--debug` / `-g` → Defines `DEBUG` symbol
- `--release` → Defines `RELEASE` symbol
- Test mode → Defines `TEST` symbol

**Predefined symbols:**
The compiler automatically defines symbols based on the host or target:

| Category | Symbols |
|----------|---------|
| OS | `WINDOWS`, `LINUX`, `MACOS`, `ANDROID`, `IOS`, `FREEBSD`, `UNIX`, `POSIX` |
| Arch | `X86_64`, `X86`, `ARM64`, `ARM`, `WASM32`, `RISCV64` |
| Pointer | `PTR_32`, `PTR_64` |
| Endian | `LITTLE_ENDIAN`, `BIG_ENDIAN` |
| Env | `MSVC`, `GNU`, `MUSL` |

**Example TML code:**
```tml
#if WINDOWS
func get_home() -> Str {
    return env::var("USERPROFILE")
}
#elif UNIX
func get_home() -> Str {
    return env::var("HOME")
}
#endif

#ifdef DEBUG
func log(msg: Str) {
    print("[DEBUG] {msg}\n")
}
#else
func log(msg: Str) { }
#endif
```

See [02-LEXICAL.md](./02-LEXICAL.md#10-preprocessor-directives) for full preprocessor documentation.

### 8.11 Code Generation Backends

TML supports multiple code generation backends. The backend is selected via the `--backend` flag.

| Backend | Flag | Status | Description |
|---------|------|--------|-------------|
| **LLVM** | `--backend=llvm` | ✅ Default | Full-featured backend with optimizations, LTO, debug info. Production-ready. |
| **Cranelift** | `--backend=cranelift` | 🧪 Experimental | Lightweight backend focused on fast compile times. **In development — not ready for production use.** |

```bash
# Default (LLVM)
tml build main.tml

# Explicit LLVM
tml build main.tml --backend=llvm

# Cranelift (experimental, in development)
tml build main.tml --backend=cranelift
```

**Cranelift Backend (Experimental)**

The Cranelift backend is under active development as an alternative to LLVM. It aims to provide:
- Faster compilation times (especially for debug builds)
- Lower memory usage during compilation
- Simpler integration (no LLVM dependency)

**Current limitations** (will be addressed as development progresses):
- No optimization passes (debug-quality code only)
- Limited target support (x86_64 only initially)
- No LTO support
- No debug info emission
- Incomplete feature coverage

> **Warning:** The Cranelift backend is experimental and actively in development. Use LLVM for all production builds.

### 8.12 Borrow Checker Selection

TML supports two borrow checking algorithms:

| Checker | Flag | Status | Description |
|---------|------|--------|-------------|
| **NLL** | *(default)* | ✅ Default | Non-Lexical Lifetimes checker. Single forward pass over AST. Conservative but fast. |
| **Polonius** | `--polonius` | ✅ Complete | Datalog-style constraint solver. Strictly more permissive than NLL. |

```bash
# Default (NLL)
tml build main.tml

# Polonius (more permissive)
tml build main.tml --polonius

# Polonius with test runner
tml test tests/ --polonius
```

**Polonius Borrow Checker**

Polonius uses a constraint-based algorithm that tracks **origins** (where references come from) and **loans** (specific borrow operations). It propagates loan liveness through the control flow graph and only reports errors when an invalidated loan is still reachable through a live origin.

Polonius accepts all programs NLL accepts, plus additional safe programs where borrows are conditionally taken across branches. Both checkers produce identical `BorrowError` output.

See [06-MEMORY.md](./06-MEMORY.md#1251-polonius-borrow-checker-alternative) for detailed algorithm description and examples.

---

*Previous: [08-IR.md](./08-IR.md)*
*Next: [10-TESTING.md](./10-TESTING.md) — Testing Framework*
