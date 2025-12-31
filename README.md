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

// Closures use 'do' keyword
let double = do(x: I32) x * 2
let sum = do(a: I32, b: I32) { return a + b }

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
│   │   ├── codegen/   # LLVM IR generation
│   │   ├── parser/    # LL(1) parser
│   │   ├── types/     # Type checker
│   │   └── cli/       # Command-line interface
│   └── include/       # Headers
├── lib/               # TML standard libraries
│   ├── core/          # Core types (Maybe, Outcome, Iterator, Array)
│   └── std/           # Standard library (File, collections)
├── docs/              # Language specification
└── scripts/           # Build scripts
```

## Current Status

**Bootstrap Compiler** - Core features working

| Component | Status |
|-----------|--------|
| Lexer | Complete |
| Parser (LL(1)) | Complete |
| Type Checker | Complete (generics, closures, where clauses) |
| LLVM Backend | Complete (via text IR) |
| Arrays | Complete (fixed-size with full method suite) |
| Maybe/Outcome | Complete |
| Pattern Matching | Complete |
| Closures | Complete |
| FFI | Complete (@extern, @link) |
| Test Framework | Complete (@test) |

### Recent Features (Dec 2024)

- Fixed-size arrays with methods: `len`, `is_empty`, `get`, `first`, `last`, `map`, `eq`, `cmp`
- Array element modification via index assignment
- Maybe type with pattern matching for `Just(v)` and `Nothing`
- Null literal support
- Bitwise operators: `shl`, `shr`, `xor`
- ASCII character utilities

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
