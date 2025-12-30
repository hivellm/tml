# TML Compiler

The TML (To Machine Language) bootstrap compiler, written in C++20.

## Building

### Requirements

- CMake 3.20+
- C++20 compatible compiler (GCC 12+, Clang 15+, MSVC 19.30+)
- LLVM 17+ (required for code generation)
- Clang (for linking generated code)

### Build Steps

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --config Debug

# Run tests
./build/Debug/tml.exe test
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `TML_BUILD_TESTS` | ON | Build test suite |
| `TML_ENABLE_ASAN` | OFF | Enable AddressSanitizer |
| `TML_ENABLE_UBSAN` | OFF | Enable UndefinedBehaviorSanitizer |

## Usage

```bash
# Check a file
./tml check file.tml

# Build a file
./tml build file.tml

# Run a file
./tml run file.tml

# Run all tests
./tml test

# Debug a file
./tml debug file.tml
```

## Project Structure

```
packages/compiler/
├── include/tml/        # Public headers
│   ├── common.hpp      # Common types and utilities
│   ├── lexer/          # Lexer headers
│   ├── parser/         # Parser headers
│   ├── types/          # Type system headers
│   ├── borrow/         # Borrow checker headers
│   ├── ir/             # IR headers
│   └── codegen/        # Codegen headers
├── src/
│   ├── lexer/          # Tokenizer
│   ├── parser/         # AST generation
│   ├── types/          # Type checker with module system
│   ├── borrow/         # Borrow checker
│   ├── ir/             # Intermediate representation
│   ├── codegen/        # LLVM backend
│   ├── cli/            # Command line interface
│   └── main.cpp        # Entry point
├── tests/              # Test files
│   └── tml/
│       ├── compiler/   # Compiler tests
│       └── runtime/    # Runtime tests
├── runtime/            # C runtime library
└── CMakeLists.txt      # Build configuration
```

## Development Status

| Component | Status |
|-----------|--------|
| Lexer | ✅ Complete |
| Parser | ✅ Complete |
| Type Checker | ✅ Complete |
| Module System | ✅ Complete |
| Pattern Matching | ✅ Complete |
| Enum Support | ✅ Complete |
| Trait Objects | ✅ Complete |
| Borrow Checker | 🟡 Basic |
| IR Generator | ✅ Complete |
| LLVM Backend | ✅ Complete |
| CLI | ✅ Complete |
| Test Framework | ✅ Complete |

## Features

### Language Features
- ✅ Basic types (I32, I64, Bool, Str, F64, etc.)
- ✅ Functions with type parameters
- ✅ Structs with generics (monomorphization)
- ✅ Enums (simple and with data variants)
- ✅ Pattern matching (when expressions)
- ✅ Trait objects (`dyn Behavior`) with vtables
- ✅ Closures (basic, without capture)
- ✅ Operators (arithmetic, comparison, logical, bitwise)
- ✅ Control flow (if/else, loop, for, while)
- ✅ Module system (use declarations)
- ✅ Time API (Instant::now(), Duration)

### Compiler Features
- ✅ Full lexical analysis
- ✅ Complete parser (all constructs)
- ✅ Type checking with inference
- ✅ Module registry and imports
- ✅ LLVM IR code generation
- ✅ Enum codegen (struct-based tagged unions)
- ✅ Pattern matching codegen
- ✅ Trait object vtable generation
- ✅ Test framework integration (@test, @bench)
- ✅ Parallel test execution

### Test Results

Current status: **34/34 tests passing (100%)**

All compiler and test framework tests pass with polymorphic assertions.

## Module System

The compiler supports a module system with `use` declarations:

```tml
use test  // Import test module

@test
func my_test() -> I32 {
    assert_eq(2 + 2, 4, "math works")
    return 0
}
```

Modules are registered in the `ModuleRegistry` and resolved during type checking.

## Recent Updates

### v0.5.0 (2025-12-24)
- **Trait Objects** - `dyn Behavior` syntax for dynamic dispatch
- Vtable generation for behavior implementations
- Method resolution through generated vtables

### v0.4.0 (2025-12-23)
- **Build System** - Cross-platform build scripts
- Target triple-based build directories (like Rust)
- Linux/GCC compatibility fixes
- Vitest-like test output with colors

### v0.3.0 (2025-12-23)
- Full module system with `use test` support
- Fixed enum pattern matching in `when` expressions
- Parallel test execution with thread pool
- Test timeout support (default 20s)
- Benchmarking with `@bench` decorator

### v0.2.0 (2025-12-23)
- Complete test framework with @test decorator
- Auto-generated test runner
- Type-specific assertion functions
- Test discovery and execution

## Known Issues

- **I64 comparisons** - Type mismatch in LLVM IR (blocks string operations)
- **Pointer references** - `mut ref I32` codegen issue (blocks memory/atomic operations)
- **Closure capture** - Basic closures work, environment capture not implemented

## License

Apache 2.0
