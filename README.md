# TML — To Machine Language

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)](https://cmake.org/)

**TML is a programming language designed specifically for Large Language Models (LLMs).** It eliminates parsing ambiguities, provides stable IDs for refactoring, and uses formal contracts to make code generation and analysis deterministic.

## 🎯 Problem Statement

All existing programming languages were designed for **humans**:
- Syntax optimized for manual reading/writing
- Ambiguities resolved by human context
- Syntactic sugar for typing convenience
- Error messages for human developers

**LLMs are not humans.** They need:
- **Deterministic parsing** — no ambiguities
- **Unique tokens** — each symbol with one meaning
- **Explicit structure** — no contextual inferences
- **Stable IDs** — references that survive refactoring

## ✨ Solution: TML

**TML (To Machine Language)** is a language where:

| Principle | Implementation |
|-----------|----------------|
| One token = one meaning | `<` is always comparison, `[` is always generic/array |
| LL(1) parsing | Lookahead of 1 token determines the production |
| Explicit > implicit | `return` mandatory, `then` mandatory |
| Stable IDs | Functions and types have immutable `@id` |
| No macros | Code is code, no meta-programming |

## 🚀 Key Features

### 🔍 Deterministic Parsing
- **LL(1) grammar** — single token lookahead determines production
- **No ambiguities** — each token has exactly one meaning
- **No macros** — code is code, no meta-programming

### 🏷️ Stable IDs
Each definition has a unique ID that survives renames:
```tml
func calculate@a1b2c3d4(x: I32) -> I32 {
    return x * 2
}
```
LLMs can reference `@a1b2c3d4` without depending on the name.

### 🎭 Effects & Capabilities
Explicit declaration of what code can and does:
```tml
module Database {
    caps: [io.network, io.file]

    func query(sql: String) -> Result[Rows, Error]
    effects: [db.read] {
        // ...
    }
}
```

### 📋 Formal Contracts
Pre and post-conditions:
```tml
func sqrt(x: F64) -> F64
pre: x >= 0.0
post(result): result >= 0.0 and result * result == x {
    // ...
}
```

### 🏗️ Rust-Inspired, LLM-Optimized
Learns from Rust but changes syntax for determinism:

```tml
// TML (explicit, deterministic)
func first[T: Clone](items: &List[T]) -> Option[T] {
    return items.first().clone()
}

// Rust (ambiguous parsing)
fn first<T: Clone>(items: &[T]) -> Option<T> {
    items.first().cloned()
}
```

## 📖 Language Examples

### Hello World
```tml
module hello

public func main() {
    println("Hello, TML!")
}
```

### Variables & Types
```tml
module variables

public func main() {
    let x = 42              // Immutable with inference
    let count: I32 = 100    // Explicit type
    var counter = 0         // Mutable
    const MAX_SIZE = 1024   // Compile-time constant

    println("counter = " + counter.to_string())
}
```

### Functions & Generics
```tml
module collections

func first[T: Clone](items: &List[T]) -> Option[T] {
    return items.first().clone()
}

func map[T, U](items: List[T], f: do(T) -> U) -> List[U] {
    var result = List.of[U]()
    loop item in items {
        result.append(f(item))
    }
    return result
}
```

### Pattern Matching
```tml
func process(value: Result[I32, String]) -> String {
    when value {
        Ok(num) -> "Number: " + num.to_string(),
        Err(msg) -> "Error: " + msg,
    }
}
```

### Error Handling
```tml
func read_file(path: String) -> Result[String, Error] {
    let file = File.open(path)!      // ! propagates errors
    let content = file.read_string()!
    return Ok(content)
}
```

## 🏛️ Architecture

### Two-Phase Bootstrap

**Phase 1: Bootstrap Compiler (C++)**
- Cross-platform native executables via LLVM
- Implements full language specification
- Frozen after Phase 2 completion

**Phase 2: Native Compiler (TML)**
- Self-hosted in TML
- Compiled by bootstrap compiler
- Ongoing development and maintenance

### Compiler Pipeline

```
Source (.tml)
    │
    ▼
┌─────────────┐
│   Lexer     │  → Token stream
└─────────────┘
    │
    ▼
┌─────────────┐
│   Parser    │  → AST (LL(1))
└─────────────┘
    │
    ▼
┌─────────────┐
│  Resolver   │  → AST (names resolved)
└─────────────┘
    │
    ▼
┌─────────────┐
│ Type Check  │  → TAST (typed AST)
└─────────────┘
    │
    ▼
┌─────────────┐
│Borrow Check │  → TAST (ownership verified)
└─────────────┘
    │
    ▼
┌─────────────┐
│Effect Check │  → TAST (effects verified)
└─────────────┘
    │
    ▼
┌─────────────┐
│  Lowering   │  → TML-IR (canonical)
└─────────────┘
    │
    ▼
┌─────────────┐
│   Codegen   │  → Native executable
└─────────────┘
```

## 🛠️ Current Status

**Bootstrap Phase** - Active Development

| Component | Status | Progress |
|-----------|--------|----------|
| Lexer | 🟡 In Progress | 0% (0/24 tasks) |
| Parser | 🟡 Planned | 0% (0/32 tasks) |
| Type Checker | 🟡 Planned | 0% |
| Borrow Checker | 🟡 Planned | 0% |
| LLVM Backend | 🟡 Planned | 0% |
| Standard Library | 🟡 Planned | 0% |

### Development Tasks
- ✅ Project setup and architecture
- 🔄 Bootstrap lexer implementation
- ⏳ Bootstrap parser implementation
- ⏳ Self-hosting compiler
- ⏳ Standard library development

## 📋 Prerequisites

- **C++20 compiler** (GCC 11+, Clang 15+, MSVC 19.30+)
- **CMake 3.20+**
- **LLVM 15+** (for code generation)
- **Git**

### Supported Platforms
- Linux (x86_64, aarch64)
- macOS (x86_64, aarch64)
- Windows (x86_64)

## 🚀 Quick Start

### Build Bootstrap Compiler

```bash
# Clone repository
git clone https://github.com/your-org/tml.git
cd tml

# Configure and build
cmake -B build -S packages/compiler
cmake --build build

# Run tests (when available)
ctest --test-dir build
```

### Hello World Example

```tml
// Save as hello.tml
module hello

public func main() {
    println("Hello, TML!")
}
```

```bash
# Compile (future command)
tmlc hello.tml -o hello

# Run
./hello
# Output: Hello, TML!
```

## 📚 Documentation

### Language Specification
- **[01-OVERVIEW.md](docs/specs/01-OVERVIEW.md)** - Language overview and philosophy
- **[02-LEXICAL.md](docs/specs/02-LEXICAL.md)** - Lexical specification
- **[03-GRAMMAR.md](docs/specs/03-GRAMMAR.md)** - Grammar and syntax
- **[04-TYPES.md](docs/specs/04-TYPES.md)** - Type system
- **[16-COMPILER-ARCHITECTURE.md](docs/specs/16-COMPILER-ARCHITECTURE.md)** - Compiler design

### Examples
Complete examples in **[docs/examples/](docs/examples/)**:
- Hello World and basic syntax
- Variables, functions, and types
- Control flow and pattern matching
- Error handling and collections
- Memory management and concurrency

### Package Documentation
Standard library packages in **[docs/packages/](docs/packages/)**:
- **FS** - File system operations
- **NET** - Networking and HTTP
- **COLLECTIONS** - Data structures
- **CRYPTO** - Cryptographic operations

## 🤝 Contributing

### Development Workflow

1. **Check existing tasks**: `rulebook task list`
2. **Create new task**: `rulebook task create <task-id>`
3. **Write proposal**: Edit `rulebook/tasks/<task-id>/proposal.md`
4. **Plan implementation**: Edit `rulebook/tasks/<task-id>/tasks.md`
5. **Validate**: `rulebook task validate <task-id>`
6. **Implement** following task checklist
7. **Test** and verify coverage ≥95%
8. **Archive**: `rulebook task archive <task-id>`

### Code Quality Requirements

- **C++20** with modern idioms (RAII, smart pointers, ranges)
- **95%+ test coverage** required
- **No warnings** allowed (`-Werror`)
- **Address/UB sanitizers** in CI/CD
- **clang-format** for consistent style

### Testing

```bash
# Build and test
cmake -B build -S packages/compiler
cmake --build build
ctest --test-dir build --output-on-failure

# Coverage (when available)
cmake -B build-cov -S packages/compiler -DCMAKE_BUILD_TYPE=Coverage
cmake --build build-cov
ctest --test-dir build-cov
```

## 🎯 Use Cases

### 🤖 LLM Code Generation
- Deterministic parsing allows immediate validation
- Stable IDs allow surgical patches
- No ambiguity reduces generation errors

### 🔍 LLM Code Analysis
- Explicit caps/effects allow reasoning about behavior
- Formalizable contracts for verification
- Canonical IR allows semantic diff

### 🔄 Automatic Refactoring
- IDs survive renames without breaking references
- Transformations preserve semantics via IR
- Patches applicable without context

## 📄 License

**MIT License** - see [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **Rust** - Inspiration for ownership, borrowing, and effects
- **Swift** - Pattern matching and error handling patterns
- **Haskell** - Type system foundations
- **LLVM** - Code generation backend

---

**TML is designed to be the language LLMs write best.** By removing ambiguities and adding explicit structure, we create a language where AI can generate, analyze, and refactor code with confidence.

Ready to build the future of AI-assisted programming? 🚀
