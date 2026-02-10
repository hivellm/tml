# TML — To Machine Language

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)](https://cmake.org/)

**TML is a programming language designed specifically for Large Language Models (LLMs).** It eliminates parsing ambiguities, provides stable IDs for refactoring, and uses self-documenting syntax to make code generation and analysis deterministic.

## Quick Example

```tml
func main() -> I32 {
    let numbers: [I32; 5] = [1, 2, 3, 4, 5]
    let doubled = numbers.map(do(x) x * 2)

    when doubled.get(0) {
        Just(v) => println("First: " + (*v).to_string()),
        Nothing => println("Empty array")
    }

    return 0
}
```

## Why TML?

All existing programming languages were designed for **humans**. LLMs need:

- **Deterministic parsing** — no ambiguities
- **Unique tokens** — each symbol with one meaning
- **Self-documenting syntax** — keywords that explain themselves
- **Explicit structure** — no contextual inferences

## Key Design Decisions

TML uses Rust-inspired semantics with self-documenting syntax:

| Rust | TML | Why |
|------|-----|-----|
| `<T>` | `[T]` | `<` conflicts with comparison |
| `\|x\| expr` | `do(x) expr` | `\|` conflicts with OR |
| `&&` `\|\|` `!` | `and` `or` `not` | Keywords are clearer |
| `fn` | `func` | More explicit |
| `match` | `when` | More intuitive |
| `for`/`while`/`loop` | `loop` unified | Single construct |
| `&T` / `&mut T` | `ref T` / `mut ref T` | Words over symbols |
| `trait` | `behavior` | Self-documenting |
| `Option[T]` | `Maybe[T]` | Intent is clear |
| `Some(x)` / `None` | `Just(x)` / `Nothing` | Self-documenting |
| `Result[T,E]` | `Outcome[T,E]` | Describes what it is |
| `unsafe` | `lowlevel` | Less scary, accurate |
| `.clone()` | `.duplicate()` | No confusion with Git |

## Language Features

### Types

```tml
// Primitives: I8-I128, U8-U128, F32, F64, Bool, Char, Str
let x: I32 = 42
let pi: F64 = 3.14159
let flag: Bool = true
let name: Str = "TML"

// Mutable variables use 'mut'
let mut counter: I32 = 0
counter = counter + 1
```

### Fixed-Size Arrays

```tml
// Array literal with explicit type
let arr: [I32; 3] = [1, 2, 3]

// Access elements
let first = arr[0]          // Direct index (panics if out of bounds)
let safe = arr.get(0)       // Returns Maybe[ref I32]

// Array methods
arr.len()                   // 3
arr.is_empty()              // false
arr.first()                 // Maybe[ref I32]
arr.last()                  // Maybe[ref I32]
arr.map(do(x) x * 2)        // [2, 4, 6]

// Modify mutable arrays
let mut nums: [I32; 3] = [1, 2, 3]
nums[0] = 10                // Direct modification
```

### Maybe and Outcome Types

```tml
// Maybe[T] - optional values (like Rust's Option)
let maybe_val: Maybe[I32] = Just(42)
let nothing: Maybe[I32] = Nothing

when maybe_val {
    Just(v) => println("Got: " + v.to_string()),
    Nothing => println("Nothing here")
}

// Outcome[T, E] - error handling (like Rust's Result)
func divide(a: I32, b: I32) -> Outcome[I32, Str] {
    if b == 0 {
        return Err("division by zero")
    }
    return Ok(a / b)
}

// Combinators
let result = divide(10, 2)
    .map(do(n) n * 2)
    .unwrap_or(0)
```

### Pattern Matching

```tml
// 'when' expression for pattern matching
func describe(value: Maybe[I32]) -> Str {
    when value {
        Just(n) => {
            if n > 0 { return "positive" }
            if n < 0 { return "negative" }
            return "zero"
        },
        Nothing => return "nothing"
    }
}

// Works with Outcome too
when read_file("data.txt") {
    Ok(content) => process(content),
    Err(e) => println("Error: " + e)
}
```

### Functions and Closures

```tml
// Regular function
func add(a: I32, b: I32) -> I32 {
    return a + b
}

// Closures use 'do' keyword (type annotation required for closure bindings)
let double: func(I32) -> I32 = do(x: I32) x * 2
let sum: func(I32, I32) -> I32 = do(a: I32, b: I32) { return a + b }

// Higher-order functions
func apply_twice[T](value: T, f: func(T) -> T) -> T {
    return f(f(value))
}

let result = apply_twice(5, do(x) x * 2)  // 20
```

### Generics

```tml
// Generic function
func first[T](items: ref [T; 3]) -> Maybe[ref T] {
    return items.get(0)
}

// Generic struct
type Pair[T, U] {
    first: T,
    second: U
}

// With constraints
func print_all[T: Display](items: ref [T; 3]) {
    loop i in 0 through 2 {
        println(items[i].to_string())
    }
}
```

### Behaviors (Traits)

```tml
// Define a behavior
pub behavior Display {
    pub func to_string(this) -> Str
}

pub behavior Iterator {
    type Item
    pub func next(mut this) -> Maybe[This::Item]
}

// Implement for a type
impl Display for Point {
    pub func to_string(this) -> Str {
        return "(" + this.x.to_string() + ", " + this.y.to_string() + ")"
    }
}
```

### Enums

```tml
// Simple enum
type Color {
    Red,
    Green,
    Blue
}

// Enum with data
type Message {
    Text(Str),
    Number(I32),
    Quit
}

when msg {
    Text(s) => println(s),
    Number(n) => println(n.to_string()),
    Quit => return
}
```

### Error Propagation with `!`

```tml
// The ! operator propagates errors early (like Rust's ?)
func read_config() -> Outcome[Config, Str] {
    let content = read_file("config.json")!  // Returns Err early if fails
    let parsed = parse_json(content)!        // Chain multiple fallible ops
    return Ok(parsed)
}

// Works with Maybe too
func get_first_user() -> Maybe[User] {
    let users = load_users()!     // Returns Nothing if Nothing
    return users.first()
}
```

### Drop and RAII

```tml
// Types implementing Drop get automatic cleanup
pub behavior Drop {
    pub func drop(mut this)
}

type FileHandle {
    fd: I32
}

impl Drop for FileHandle {
    pub func drop(mut this) {
        close_fd(this.fd)  // Called automatically when out of scope
    }
}

func process_file() {
    let file = open("data.txt")
    // ... use file ...
}  // file.drop() called automatically here
```

### Slices

```tml
// Slices are fat pointers: { data_ptr, len }
func sum_slice(s: Slice[I32]) -> I64 {
    let mut total: I64 = 0
    let mut i: I64 = 0
    loop {
        if i >= s.len() { return total }
        total = total + s.get(i).unwrap() as I64
        i = i + 1
    }
    return total
}

// Create from array
let arr: [I32; 5] = [1, 2, 3, 4, 5]
let slice = arr.as_slice()
println(sum_slice(slice).to_string())  // 15
```

### Trait Objects (dyn Behavior)

```tml
behavior Shape {
    func area(this) -> F64
    func name(this) -> Str
}

type Circle { radius: F64 }
type Rectangle { width: F64, height: F64 }

impl Shape for Circle {
    func area(this) -> F64 { 3.14159 * this.radius * this.radius }
    func name(this) -> Str { "Circle" }
}

impl Shape for Rectangle {
    func area(this) -> F64 { this.width * this.height }
    func name(this) -> Str { "Rectangle" }
}

// Dynamic dispatch via dyn
func print_shape(s: dyn Shape) {
    println(s.name() + " area: " + s.area().to_string())
}

let c = Circle { radius: 2.0 }
let r = Rectangle { width: 3.0, height: 4.0 }
print_shape(c)  // Circle area: 12.566...
print_shape(r)  // Rectangle area: 12.0
```

### FFI (Foreign Function Interface)

```tml
// Call C functions
@extern("c")
func puts(s: Str) -> I32

// Link external libraries (Windows)
@link("user32")
@extern("stdcall")
func MessageBoxA(hwnd: I32, text: Str, caption: Str, utype: I32) -> I32

func main() -> I32 {
    puts("Hello from C!")
    MessageBoxA(0, "Hello!", "TML", 0)
    return 0
}
```

### Testing

```tml
@test
func test_array_access() -> I32 {
    let arr: [I32; 3] = [10, 20, 30]
    assert(arr[0] == 10, "first element should be 10")
    assert(arr.len() == 3, "length should be 3")
    return 0
}

@test
func test_maybe_unwrap() -> I32 {
    let x: Maybe[I32] = Just(42)
    assert(x.unwrap() == 42, "should unwrap to 42")
    return 0
}
```

## Build and Run

### Prerequisites

- **C++20 compiler** (GCC 11+, Clang 15+, MSVC 19.30+)
- **CMake 3.20+**
- **LLVM 15+**

### Optional Dependencies

TML's standard library has optional modules that require external C libraries. These are **statically linked** into the compiler runtime — no DLLs need to be copied or distributed.

| Module | Requires | Purpose |
|--------|----------|---------|
| `std::crypto` | OpenSSL 3.0+ | X.509 certificates, key management, signatures |
| `std::zlib` | zlib, brotli, zstd | Compression (deflate, gzip, brotli, zstd) |

If not installed, the compiler still works — programs using these modules will get stub implementations or compile-time errors.

#### Install via vcpkg (recommended, all platforms)

The project includes a `vcpkg.json` manifest. Install all dependencies with a single command:

```bash
# Windows
cd tml
vcpkg install --x-install-root=vcpkg_installed --triplet=x64-windows

# Linux
cd tml
vcpkg install --x-install-root=vcpkg_installed --triplet=x64-linux

# macOS (Intel)
cd tml
vcpkg install --x-install-root=vcpkg_installed --triplet=x64-osx

# macOS (Apple Silicon)
cd tml
vcpkg install --x-install-root=vcpkg_installed --triplet=arm64-osx
```

This installs OpenSSL, zlib, brotli, and zstd to `tml/vcpkg_installed/<triplet>/`. The build system auto-detects them.

> **Don't have vcpkg?** Install it from [github.com/microsoft/vcpkg](https://github.com/microsoft/vcpkg):
> ```bash
> git clone https://github.com/microsoft/vcpkg.git
> cd vcpkg && bootstrap-vcpkg.bat   # Windows
> cd vcpkg && ./bootstrap-vcpkg.sh  # Linux/macOS
> # Add vcpkg to your PATH
> ```

#### Alternative: System package managers (Linux/macOS)

```bash
# Linux (apt/Ubuntu/Debian)
sudo apt install libssl-dev zlib1g-dev libbrotli-dev libzstd-dev

# Linux (dnf/Fedora)
sudo dnf install openssl-devel zlib-devel brotli-devel libzstd-devel

# macOS (brew)
brew install openssl@3 zlib brotli zstd
```

The build system auto-detects system-installed libraries via `find_package` and standard include paths.

#### Alternative: Standalone OpenSSL (Windows only)

Download from [slproweb.com/products/Win32OpenSSL.html](https://slproweb.com/products/Win32OpenSSL.html) and install to `C:\Program Files\OpenSSL-Win64`. The build system checks this path automatically.

### Build

```bash
# Windows
scripts\build.bat              # Debug build
scripts\build.bat release      # Release build
scripts\build.bat --clean      # Clean build

# Linux/Mac
./scripts/build.sh debug
./scripts/build.sh release
```

### Run

```bash
# Run directly
./build/debug/tml run hello.tml

# Compile to executable
./build/debug/tml build hello.tml -o hello
./hello

# Run tests
./build/debug/tml test mymodule.tml

# Emit LLVM IR for debugging
./build/debug/tml build hello.tml --emit-ir
```

## Project Structure

```
tml/
├── compiler/           # C++ compiler implementation
│   ├── src/           # Source files
│   │   ├── lexer/     # Tokenizer
│   │   ├── parser/    # LL(1) parser
│   │   ├── types/     # Type checker
│   │   ├── borrow/    # Borrow checker
│   │   ├── hir/       # High-level IR (type-resolved, desugared)
│   │   ├── mir/       # Mid-level IR (SSA form)
│   │   ├── codegen/   # LLVM IR generation
│   │   ├── query/     # Query system (demand-driven compilation)
│   │   ├── backend/   # LLVM backend + LLD linker (in-process)
│   │   └── cli/       # Command-line interface
│   └── include/       # Headers
├── lib/               # TML standard libraries
│   ├── core/          # Core types (Maybe, Outcome, Iterator, Array)
│   └── std/           # Standard library (File, collections)
├── docs/              # Language specification
└── scripts/           # Build scripts
```

## Compiler Pipeline

The TML compiler uses a **demand-driven query system** (like rustc) with **red-green incremental compilation**:

```
Source (.tml) → [QueryContext] → ReadSource → Tokenize → Parse → Typecheck
             → Borrowcheck → HirLower → MirBuild → CodegenUnit
             → [Embedded LLVM] → .obj → [Embedded LLD] → .exe
```

| Stage | Description |
|-------|-------------|
| ReadSource | Read and preprocess source file |
| Tokenize | Tokenize preprocessed source |
| Parse | Build AST using LL(1) grammar |
| Typecheck | Type inference, generic resolution, module imports |
| Borrowcheck | Ownership and lifetime verification |
| HirLower | Lower AST to High-level IR (type-resolved, desugared) |
| MirBuild | Build Mid-level IR (SSA form, optimization passes) |
| CodegenUnit | Generate LLVM IR from MIR |
| **LLVM** | In-process IR → .obj compilation (embedded, no clang subprocess) |
| **LLD** | In-process linking (embedded, no linker subprocess) |

Each stage is a **memoized query** with dependency tracking. On rebuild, unchanged queries are marked **GREEN** and skipped entirely — cached LLVM IR is loaded from disk.

## Current Status

**Bootstrap Compiler** - Core features working, query-based pipeline with incremental compilation

| Component | Status |
|-----------|--------|
| Lexer | Complete |
| Parser (LL(1)) | Complete |
| Type Checker | Complete (generics, closures, where clauses) |
| Borrow Checker | Complete (integrated with Rust-style diagnostics) |
| HIR | Complete (type-resolved, desugared AST) |
| MIR | Complete (SSA form, optimization passes) |
| LLVM Backend | Complete (embedded, in-process compilation) |
| LLD Linker | Complete (embedded, in-process COFF/ELF/MachO) |
| Query System | Complete (demand-driven, 8 memoized stages) |
| Incremental Compilation | Complete (red-green, cross-session persistence) |
| Arrays | Complete (fixed-size with full method suite) |
| Slices | Complete (Slice[T], MutSlice[T] fat pointers) |
| Maybe/Outcome | Complete |
| Pattern Matching | Complete |
| Closures | Complete (captures, HOF, iterators) |
| Drop/RAII | Complete (automatic resource cleanup) |
| Trait Objects | Complete (dyn Behavior, multiple methods) |
| Error Propagation | Complete (`!` operator) |
| FFI | Complete (@extern, @link) |
| Test Framework | Complete (@test, 363 test files, 3632 tests) |

### Recent Features (Feb 2026)

- **Query-Based Build Pipeline (Default)** - Demand-driven compilation using memoized queries (like rustc's TyCtxt)
- **Red-Green Incremental Compilation** - Cross-session persistence of fingerprints; no-op rebuild skips entire pipeline
- **Embedded LLVM Backend** - ~55 LLVM static libraries linked directly; in-process IR→obj (50x faster)
- **Embedded LLD Linker** - In-process linking for Windows (COFF), Linux (ELF), macOS (MachO)

### Features (Jan 2026)

- **HashMap String Keys** - Fixed string key hashing to use content-based `str_hash()` instead of pointer addresses
- **HIR (High-level IR)** - New compiler IR layer between type-checked AST and MIR for type-resolved desugaring and monomorphization
- **Core Library Tests** - Comprehensive tests for iter, slice, num, range, async_iter, marker modules
- **Numeric Properties** - Zero, One, Bounded behaviors with full test coverage
- **Range Types** - Range, RangeFrom, RangeTo, RangeFull types with iterator support
- **ASCII Module** - Complete `core::ascii::AsciiChar` type with classification methods

### Features (Dec 2025 - Jan 2026)

- **Borrow Checker Integration** - Memory safety enforced at compile time with Rust-style diagnostics
- **Drop/RAII** - Automatic resource cleanup when values go out of scope
- **Dynamic Slices** - `Slice[T]` and `MutSlice[T]` fat pointer types with safe access
- **Error Propagation** - `!` operator for early return on `Outcome[T,E]` and `Maybe[T]`
- **Trait Objects** - `dyn Behavior` for dynamic dispatch with multiple methods
- **Closures** - Full closure support with captures, higher-order functions
- **Iterator Methods** - `fold`, `all`, `any`, `find`, `position`, `for_each`, `count`, `last`, `nth`
- Fixed-size arrays with methods: `len`, `is_empty`, `get`, `first`, `last`, `map`, `eq`, `cmp`

## Documentation

- [docs/INDEX.md](docs/INDEX.md) - Documentation overview
- [docs/01-OVERVIEW.md](docs/01-OVERVIEW.md) - Language philosophy
- [docs/03-GRAMMAR.md](docs/03-GRAMMAR.md) - LL(1) grammar specification
- [docs/04-TYPES.md](docs/04-TYPES.md) - Type system

## License

**Apache License 2.0** - see [LICENSE](LICENSE) file.

## Acknowledgments

- **Rust** - Ownership, borrowing, and pattern matching inspiration
- **LLVM** - Code generation backend
