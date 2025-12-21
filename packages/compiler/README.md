# TML Compiler

The TML (To Machine Language) bootstrap compiler, written in C++20.

## Building

### Requirements

- CMake 3.20+
- C++20 compatible compiler (GCC 12+, Clang 15+, MSVC 19.30+)
- LLVM 17+ (optional, for code generation)

### Build Steps

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `TML_BUILD_TESTS` | ON | Build test suite |
| `TML_ENABLE_ASAN` | OFF | Enable AddressSanitizer |
| `TML_ENABLE_UBSAN` | OFF | Enable UndefinedBehaviorSanitizer |

## Usage

```bash
# Show help
./tml --help

# Show version
./tml --version

# Lexer demo (for development)
./tml lex "func add(a: I32, b: I32) -> I32 { return a + b }"
```

## Project Structure

```
packages/compiler/
├── include/tml/        # Public headers
│   ├── common.hpp      # Common types and utilities
│   └── lexer/          # Lexer headers
├── src/
│   ├── lexer/          # Tokenizer
│   ├── parser/         # AST generation
│   ├── types/          # Type checker
│   ├── borrow/         # Borrow checker
│   ├── ir/             # Intermediate representation
│   ├── codegen/        # LLVM backend
│   ├── cli/            # Command line interface
│   └── main.cpp        # Entry point
├── tests/              # Unit tests
├── docs/               # Documentation
└── CMakeLists.txt      # Build configuration
```

## Development Status

| Component | Status |
|-----------|--------|
| Lexer | ✅ Implemented |
| Parser | 🚧 In Progress |
| Type Checker | 📝 Planned |
| Borrow Checker | 📝 Planned |
| IR Generator | 📝 Planned |
| LLVM Backend | 📝 Planned |
| CLI | 🚧 Basic |

## License

MIT
