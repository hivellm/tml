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
| Borrow Checker | ✅ Basic |
| IR Generator | ✅ Complete |
| LLVM Backend | ✅ Complete |
| CLI | ✅ Complete |
| Test Framework | ✅ Complete |

## Features

### Language Features
- ✅ Basic types (I32, I64, Bool, Str, etc.)
- ✅ Functions with type parameters
- ✅ Structs with generics
- ✅ Enums (simple and with data)
- ✅ Pattern matching (when expressions)
- ✅ Closures (basic, without capture)
- ✅ Operators (arithmetic, comparison, logical)
- ✅ Control flow (if/else, loop, for, while)
- ✅ Module system (use declarations)

### Compiler Features
- ✅ Full lexical analysis
- ✅ Complete parser (all constructs)
- ✅ Type checking with inference
- ✅ Module registry and imports
- ✅ LLVM IR code generation
- ✅ Enum codegen (struct-based)
- ✅ Pattern matching codegen
- ✅ Test framework integration

### Test Results

Current status: **9/10 tests passing (90%)**

```
✅ basics.test.tml
✅ closures.test.tml
✅ features.test.tml
✅ patterns.test.tml
✅ enums.test.tml
✅ enums_comparison.test.tml
✅ structs.test.tml
✅ demo_assertions.test.tml
✅ simple_demo.test.tml
❌ collections.test.tml (known runtime bug)
```

## Module System

The compiler supports a module system with `use` declarations:

```tml
use test  // Import test module

@test
func my_test() -> I32 {
    assert_eq_i32(2 + 2, 4, "math works")
    return 0
}
```

Modules are registered in the `ModuleRegistry` and resolved during type checking.

## Recent Updates

### v0.3.0 (2025-12-23)
- Implemented full module system with `use test` support
- Fixed enum pattern matching in `when` expressions
- Added proper enum value creation and comparison
- Removed global assertion functions (now module-scoped)
- All tests updated to use `use test`

### v0.2.0 (2025-12-23)
- Complete test framework with @test decorator
- Auto-generated test runner
- Type-specific assertion functions
- Test discovery and execution

## License

MIT
