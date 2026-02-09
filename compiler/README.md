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
# From project root (recommended)
scripts/build.bat              # Windows - Debug build
scripts/build.bat release      # Windows - Release build
scripts/build.bat --no-tests   # Skip tests

# Run tests
scripts/test.bat
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

# Debug commands
./tml debug lex file.tml    # Show tokens
./tml debug parse file.tml  # Show AST
./tml debug check file.tml  # Show type info
```

## Project Structure

```
compiler/
├── include/            # Header files
│   ├── common.hpp      # Common types and utilities
│   ├── lexer/          # Lexer headers
│   ├── parser/         # Parser headers (AST, OOP)
│   ├── types/          # Type system headers
│   ├── borrow/         # Borrow checker headers
│   ├── hir/            # High-level IR
│   ├── mir/            # Mid-level IR (SSA, passes)
│   ├── codegen/        # LLVM codegen headers
│   └── query/          # Query system headers (QueryContext, cache, keys, incremental)
├── src/
│   ├── lexer/          # Tokenizer
│   ├── parser/         # AST generation
│   ├── preprocessor/   # Conditional compilation
│   ├── types/          # Type checker with module system
│   ├── borrow/         # Borrow checker
│   ├── hir/            # HIR generation
│   ├── mir/            # MIR passes (devirtualization, etc.)
│   ├── codegen/        # LLVM IR backend
│   │   └── core/       # Core codegen (classes, generics)
│   ├── query/          # Query system (demand-driven compilation)
│   ├── backend/        # LLVM backend + LLD linker (in-process)
│   ├── cli/            # Command line interface
│   │   ├── commands/   # CLI commands (build, test, etc.)
│   │   ├── builder/    # Build system
│   │   └── tester/     # Test runner
│   └── main.cpp        # Entry point
├── tests/              # C++ unit tests (GoogleTest)
└── runtime/            # C runtime library (essential.c)
```

## Development Status

| Component | Status |
|-----------|--------|
| Lexer | Complete |
| Parser | Complete |
| Type Checker | Complete |
| Module System | Complete |
| Pattern Matching | Complete |
| Enum Support | Complete |
| Trait Objects | Complete |
| **OOP (Classes/Interfaces)** | Complete |
| **@value Classes** | Complete |
| **@pool Classes** | Validation Complete |
| Borrow Checker | Basic |
| HIR Generator | Complete |
| MIR Passes | Complete |
| **LLVM Backend** | Complete (embedded, in-process) |
| **LLD Linker** | Complete (embedded COFF/ELF/MachO) |
| **Query System** | Complete (demand-driven, 8 stages) |
| **Incremental Compilation** | Complete (red-green, cross-session) |
| CLI | Complete |
| Test Framework | Complete |

## Features

### Language Features
- Basic types (I8-I128, U8-U128, F32, F64, Bool, Char, Str)
- Functions with type parameters
- Structs with generics (monomorphization)
- Enums (simple and with data variants)
- Pattern matching (when expressions)
- Trait objects (`dyn Behavior`) with vtables
- Closures with capture
- Operators (arithmetic, comparison, logical, bitwise)
- Control flow (if/else, loop, for, while)
- Module system (use declarations)
- Async/await support
- **C#-style OOP**:
  - Classes with single inheritance (`extends`)
  - Interfaces with multiple implementation (`implements`)
  - Abstract and sealed classes
  - Virtual, override, and abstract methods
  - Constructors with base calls
  - Properties (get/set)
  - Member visibility (public, private, protected, internal)
  - Namespaces
- **@value classes** (no vtable, direct dispatch)
- **@pool classes** (object pooling - validation only)

### Compiler Features
- Full lexical analysis
- Complete parser (all constructs including OOP)
- Type checking with inference
- Module registry and imports
- **Class Hierarchy Analysis (CHA)**
- **Devirtualization pass**
- **Dead method elimination**
- **Escape analysis**
- **Demand-driven query system** (like rustc's TyCtxt) with 8 memoized stages
- **Red-green incremental compilation** with cross-session persistence
- **Embedded LLVM backend** (~55 static libs, in-process IR→obj, no clang subprocess)
- **Embedded LLD linker** (in-process COFF/ELF/MachO, no linker subprocess)
- LLVM IR code generation
- Enum codegen (struct-based tagged unions)
- Pattern matching codegen
- Trait object vtable generation
- Class vtable generation
- Interface vtable generation
- Test framework integration (@test, @bench)
- Parallel test execution
- Code coverage instrumentation
- Debug info (DWARF)

### MIR Optimization Passes
- Devirtualization (final methods, sealed classes)
- Virtual call inlining
- Dead method elimination
- Escape analysis (stack promotion)
- Vtable deduplication
- Trivial destructor detection

## OOP Features

### Classes and Interfaces

```tml
interface IDrawable {
    func draw(this) -> Unit
}

abstract class Shape implements IDrawable {
    protected x: I32
    protected y: I32

    abstract func area(this) -> F64
}

class Circle extends Shape {
    private radius: F64

    func new(x: I32, y: I32, r: F64) : base(x, y) {
        this.radius = r
    }

    override func area(this) -> F64 {
        3.14159 * this.radius * this.radius
    }

    override func draw(this) -> Unit {
        print("Drawing circle at ({this.x}, {this.y})")
    }
}
```

### Value Classes

```tml
@value
class Point {
    private x: I32
    private y: I32

    func new(x: I32, y: I32) {
        this.x = x
        this.y = y
    }

    func distance(this, other: ref Point) -> F64 {
        // Direct dispatch, no vtable overhead
        let dx = (this.x - other.x) as F64
        let dy = (this.y - other.y) as F64
        (dx * dx + dy * dy).sqrt()
    }
}
```

Value classes:
- No vtable pointer (smaller memory footprint)
- Direct method dispatch (faster calls)
- Cannot have virtual methods
- Can only extend other @value classes
- Can implement interfaces

## Module System

The compiler supports a module system with `use` declarations:

```tml
use core::io
use test

@test
func my_test() -> I32 {
    assert_eq(2 + 2, 4, "math works")
    0
}
```

## Recent Updates

### v0.7.0 (2026-02)
- **Query-based build pipeline** - Default `tml build` uses demand-driven queries (like rustc)
- **Red-green incremental compilation** - Cross-session persistence, GREEN path skips entire pipeline
- **Embedded LLVM backend** - ~55 static libs linked, in-process IR→obj (50x faster)
- **Embedded LLD linker** - In-process linking (COFF/ELF/MachO, no subprocess)
- **Query system foundation** - 8 memoized stages with dependency tracking and cycle detection
- **128-bit fingerprinting** - CRC32C-based fingerprints for incremental cache validation
- `--legacy` flag to fall back to traditional pipeline
- All 3,632 tests pass across 363 test files

### v0.6.0 (2026-01)
- **@value classes** - Value semantics with no vtable
- **@pool classes** - Object pooling directive (validation)
- Direct dispatch for value class methods
- Value class codegen (no vtable pointer)

### v0.5.5 (2025-12)
- **C#-style OOP** - Classes, interfaces, inheritance
- Virtual dispatch with vtables
- Interface vtables for multiple implementation
- Namespace support
- Memory leak detection system

### v0.5.0 (2025-12)
- **Trait Objects** - `dyn Behavior` syntax for dynamic dispatch
- Vtable generation for behavior implementations
- Method resolution through generated vtables

### v0.4.0 (2025-12)
- **MIR Passes** - Devirtualization, escape analysis
- Dead method elimination
- Stack promotion for non-escaping objects

## Test Status

Run tests with:
```bash
# All C++ unit tests
./build/debug/tml_tests.exe

# Specific test suite
./build/debug/tml_tests.exe --gtest_filter="*ValueClass*"

# TML integration tests
./build/debug/tml.exe test
```

## License

Apache 2.0
