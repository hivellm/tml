# TML v1.0 — Toolchain CLI

## 1. Overview

`tml` is the unified command-line tool for TML.

```bash
tml --version
# tml 1.0.0 (abc1234 2024-01-15)

tml --help
# TML Toolchain
#
# USAGE:
#     tml <COMMAND> [OPTIONS]
#
# COMMANDS:
#     new         Create a new project
#     build       Compile the project
#     run         Build and execute
#     test        Run tests
#     check       Check for errors without compiling
#     fmt         Format source code
#     lint        Run linter
#     doc         Generate documentation
#     repl        Interactive REPL
#     ir          Generate IR
#     clean       Remove build artifacts
```

## 2. Main Commands

### 2.1 tml new — Create Project

```bash
tml new myproject

# Structure created:
# myproject/
# ├── tml.toml
# ├── src/
# │   └── lib.tml
# └── tests/
#     └── lib_test.tml

# Options
tml new myproject --bin      # executable (src/main.tml)
tml new myproject --lib      # library (default)
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
tml build --crate-type=rlib      # TML library format (future)

# C header generation for FFI
tml build --emit-header          # Generate .h file from public functions

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
tml test --jobs 4

# Coverage
tml test --coverage
tml test --coverage --format html

# Verbose
tml test --verbose
```

Output:
```
   Running tests for myproject v1.0.0

running 12 tests
test math::test_add ... ok (0.1ms)
test math::test_subtract ... ok (0.1ms)
test math::test_multiply ... ok (0.2ms)

test result: ok. 12 passed; 0 failed
   finished in 0.32s
```

### 2.5 tml check — Quick Verification

```bash
# Check without generating code
tml check

# Include tests
tml check --tests
```

Faster than build, ideal for feedback during development.

### 2.6 tml fmt — Formatting

```bash
# Format all
tml fmt

# Check without modifying
tml fmt --check

# Specific file
tml fmt src/main.tml
```

### 2.7 tml lint — Linter

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

### 2.8 tml doc — Documentation

```bash
# Generate docs
tml doc

# Open in browser
tml doc --open

# JSON format
tml doc --format json
```

### 2.9 tml repl — REPL

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

### 2.10 tml ir — Generate IR

```bash
# S-expression
tml ir src/lib.tml

# JSON
tml ir src/lib.tml --format json

# To file
tml ir src/lib.tml --output lib.tml.ir
```

### 2.11 tml clean — Cleanup

```bash
tml clean           # remove target/
tml clean --all     # remove everything including cache
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
edition = "2024"

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

TML uses incremental compilation with hash-based caching:

```bash
# Build with cache (default)
tml build

# Rebuild without cache
tml build --no-cache

# Clean cache
tml cache clean

# Show cache statistics
tml cache info
# Output:
# Cache: build/debug/.cache/
# Size: 45.2 MB
# Entries: 127
# Hit rate: 87%
# Last cleaned: 2025-01-15
```

**How it works:**
1. Hash = `hash(source_content + compiler_version + flags)`
2. Check if `build/debug/.cache/{hash}.o` exists
3. If exists → reuse (cache hit, ~10x faster)
4. If not → compile and cache

**Cache location:**
- Debug: `build/debug/.cache/`
- Release: `build/release/.cache/`
- Shared deps: `build/debug/deps/`

### 8.3 Object Files and Linking

TML compiles through object files for faster incremental builds:

```
Pipeline:
  .tml → Parse → Type Check → LLVM IR (.ll)
                                  ↓
                          Object File (.o)
                                  ↓
                         Link (based on mode)
                                  ↓
                    Output (.exe, .dll, .a, .rlib)
```

**Build artifacts:**
```
build/debug/
├── .cache/              # Object file cache
│   ├── abc123.o        # Cached object
│   ├── abc123.meta     # Metadata
│   └── index.json      # Cache index
├── deps/               # Compiled dependencies
│   ├── essential.obj
│   └── std.rlib
├── myapp.exe           # Executable
├── libmylib.a          # Static library
├── mylib.dll           # Dynamic library
├── mylib.h             # C header
└── mylib.rlib          # TML library
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
- ✅ Parallel compilation (future)
- ✅ Faster CI/CD pipelines

---

*Previous: [08-IR.md](./08-IR.md)*
*Next: [10-TESTING.md](./10-TESTING.md) — Testing Framework*
