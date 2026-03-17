# TML Language Support for Visual Studio Code

Syntax highlighting, autocompletion, and language support for TML (To Machine Language).

## Features

- **Syntax Highlighting**: Full syntax highlighting for TML language
  - Keywords (func, type, behavior, impl, dyn, class, interface, etc.)
  - Types (I32, U64, F64, Str, Bool, Text, Instant, Duration, etc.)
  - Collection types: `Vec[T]`, `HashMap[K,V]`, `HashSet[T]`, `BTreeMap[K,V]`, `BTreeSet[T]`, `Deque[T]`, `Buffer`
  - Wrapper types: `Maybe[T]`, `Outcome[T,E]`, `Heap[T]`, `Shared[T]`, `Sync[T]`
  - Builtin enums: `Maybe[T]` (Just/Nothing), `Outcome[T,E]` (Ok/Err), `Ordering`
  - Operators (and, or, not, +, -, *, /, etc.)
  - Literals (integers, floats, strings, characters)
  - Comments (line, block, doc, AI comments)
  - Directives (@test, @bench, @derive, @simd, @should_panic, @extern, @link, etc.)
  - Type-specific atomic intrinsics (`atomic_load_i32`, `atomic_store_i64`, etc.)
  - Modern memory intrinsics (`ptr_read`, `ptr_write`, `copy_nonoverlapping`, etc.)
  - Compile-time constants (`__FILE__`, `__DIRNAME__`, `__LINE__`)
  - Builtin methods (hash, duplicate, to_string, is_just, unwrap, etc.)
  - Static type methods (Type::from, Type::default)
  - Extended assertion functions (assert_eq, assert_ne, assert_true, assert_lt, assert_in_range, etc.)
  - Preprocessor directives (#if, #ifdef, #elif, #endif) with platform symbols
  - Markdown code blocks (```tml)

- **IntelliSense / Autocompletion**:
  - All 57 TML keywords (including reserved OOP and effects keywords)
  - Primitive and collection types (Vec, HashMap, HashSet, BTreeMap, BTreeSet, Deque, Buffer)
  - Wrapper types (Maybe, Outcome, Heap, Shared, Sync, Text)
  - Builtin functions (print, println, panic, assert, assert_eq, assert_ne, assert_true, etc.)
  - 40+ module completions for `use` statements (std::json, std::http, std::crypto, std::stream, std::sqlite, std::aio, etc.)
  - Code snippets for common patterns (func, type, behavior, when, etc.)
  - Import statement completion (`use std::json::*`, `use std::http::*`)

- **Hover Information**:
  - Keyword descriptions
  - Type documentation
  - Function signatures
  - Variant documentation
  - Module exports

- **Semantic Highlighting** (via LSP):
  - Function declarations and calls
  - Type declarations and references
  - Decorators (@test, @bench, etc.)

- **Build Integration**:
  - `TML: Build` command (Ctrl+Shift+B)
  - `TML: Run` command (F5)
  - `TML: Test` command
  - `TML: Clean` command
  - Problem matchers for compiler errors
  - Context menu integration

- **Diagnostics & Error Reporting**:
  - Real-time syntax validation using the TML compiler
  - Errors and warnings displayed in the Problems panel
  - Configurable via `tml.enableDiagnostics` setting
  - Debounced validation for better performance
  - Support for compiler JSON error format

- **Language Configuration**:
  - Auto-closing brackets, quotes, backticks, and parentheses
  - Comment toggling (Ctrl+/)
  - Block commenting (Ctrl+Shift+A)
  - Bracket matching
  - Code folding

## Supported File Extensions

- `.tml` - TML source files

## Syntax Examples

### Keywords and Control Flow
```tml
func fibonacci(n: I32) -> I32 {
    if n <= 1 then
        return n
    else
        return fibonacci(n - 1) + fibonacci(n - 2)
}
```

### Pattern Matching
```tml
when result {
    Ok(value) -> process(value),
    Err(error) -> handle_error(error),
}
```

### Maybe and Outcome Types
```tml
// Maybe[T] - optional values
let maybe_value: Maybe[I32] = Just(42)
if maybe_value.is_just() {
    let x: I32 = maybe_value.unwrap()
}
let safe_value: I32 = maybe_value.unwrap_or(0)

// Outcome[T, E] - result types
let result: Outcome[I32, Str] = Ok(100)
if result.is_ok() {
    let value: I32 = result.unwrap()
}
```

### Type Conversion and Methods
```tml
// Static type methods
let x: I32 = I32::from(42u8)
let default_val: I32 = I32::default()

// Instance methods
let hash_val: I64 = x.hash()
let copy: I32 = x.duplicate()
let s: Str = x.to_string()
```

### Types and Generics
```tml
type Point {
    x: F64,
    y: F64,
}

func first[T](list: List[T]) -> Maybe[T] {
    return list.get(0)
}
```

### Behaviors (Traits)
```tml
behavior Display {
    func to_string(this) -> Str
}

impl Display for Point {
    func to_string(this) -> Str {
        return `({this.x}, {this.y})`
    }
}
```

### Directives
```tml
@test
func test_addition() -> I32 {
    assert_eq(2 + 2, 4, "Math works!")
    return 0
}

@when(os: linux)
func linux_only() {
    // Linux-specific code
}
```

### Lowlevel and Pointers
```tml
func pointer_example() {
    let mut x: I32 = 42

    lowlevel {
        let p: *I32 = &x      // Pointer type (*T)
        let val: I32 = p.read()
        p.write(100)
    }
}
```

### HTTP Framework
```tml
use std::http::*

func main() {
    let app = App::new()
    app.get("/hello", do(req, res) {
        res.send("Hello, world!")
    })
    app.listen(3000)
}
```

### OOP — Classes and Interfaces (RESERVED / PROPOSED)

> **Note**: The keywords `class`, `interface`, `extends`, `implements`, `override`, `virtual`, `abstract`, and `sealed` are reserved in the TML grammar and receive syntax highlighting, but the OOP object model they represent is **not yet implemented** in the compiler. The following example shows proposed syntax for a future release.

```tml
interface Drawable {
    func draw(this)
}

class Shape {
    color: Color

    virtual func area(this) -> F64 {
        return 0.0
    }
}

class Circle extends Shape implements Drawable {
    radius: F64

    override func area(this) -> F64 {
        return 3.14159 * this.radius * this.radius
    }

    func draw(this) {
        // Draw the circle
    }
}
```

### Effects and Capabilities (RESERVED / PROPOSED)

> **Note**: The `with` keyword and effect annotations (`pure`, `io`, `async`, `throws`, etc.) are reserved and highlighted by the extension, but the effect system is **not yet implemented** in the compiler. The following example shows proposed syntax for a future release.

```tml
// Proposed: effects declare side effects
func read_file(path: Str) -> Str with io {
    // Function may perform I/O
}

func pure_add(a: I32, b: I32) -> I32 with pure {
    return a + b
}
```

## Standard Library Modules

The TML standard library ships 40+ modules. The extension provides `use` statement completions for all of them:

| Namespace | Modules |
|-----------|---------|
| `std::` | json, http, stream, sqlite, aio, buffer, crypto, compress, random, search, regex, datetime, os, url, uuid, semver, glob, text, process, path, log |
| `core::` | str, fmt, iter, slice, alloc, error, encoding |
| `net::` | tcp, udp, tls |

## Installation

### From Source

1. Clone this repository
2. Open the `vscode-tml` directory in VS Code
3. Press F5 to launch the extension in a new Extension Development Host window
4. Open a `.tml` file to see syntax highlighting

### From VSIX (when published)

1. Download the `.vsix` file
2. In VS Code, go to Extensions
3. Click "..." menu → "Install from VSIX..."
4. Select the downloaded file

## Requirements

- Visual Studio Code 1.75.0 or higher

## Known Issues

- Syntax validation requires the TML compiler (`tml`) to be installed and available in PATH
- Diagnostics are disabled if the compiler is not found
- OOP and effects syntax (class, interface, with) is highlighted but not yet implemented in the compiler

## Release Notes

### 0.18.0

- **Type-Specific Atomic Intrinsics** - Highlighting for `atomic_load_i32`, `atomic_store_i64`, `atomic_cas_i32`, etc.
- **Fence & Memory Intrinsics** - `atomic_fence`, `ptr_read`, `ptr_write`, `copy_nonoverlapping`
- **Extended Assertions** - 10 new builtins: `assert_true`, `assert_false`, `assert_lt`, `assert_gt`, `assert_lte`, `assert_gte`, `assert_in_range`, `assert_str_len`, `assert_str_empty`, `assert_str_not_empty`
- **Compile-Time Constants** - `__FILE__`, `__DIRNAME__`, `__LINE__`
- **Backtick Auto-Closing** - Template literal backticks now auto-close
- **40+ Module Completions** - std::json, std::http, std::stream, std::sqlite, std::aio, std::buffer, std::crypto, std::compress, std::random, std::search, std::regex, std::datetime, std::os, std::url, std::uuid, std::semver, std::glob, and more
- **Collection & Wrapper Types** - Vec, HashMap, HashSet, BTreeMap, BTreeSet, Deque, Buffer, Text
- **New Directives** - `@derive`, `@simd`, `@should_panic`
- **Extended Preprocessor Symbols** - ANDROID, IOS, FREEBSD, UNIX, POSIX, WASM32, RISCV64, PTR_32, PTR_64, and more

### 0.13.0

- **Syntax Validation** - Real-time error reporting using the TML compiler
- **Diagnostics Integration** - Errors and warnings in the Problems panel
- **Configurable Validation** - Enable/disable with `tml.enableDiagnostics` setting

### 0.12.0

- **Import Statement Completion** - Smart completions for `use` statements
- **Effect & Capability Support** - Completions, hover, and highlighting for reserved effect keywords
- **Contract Support** - Completions and documentation for requires, ensures, invariant
- **Module Hover** - See module documentation and exports on hover

### 0.11.0

- **Build Integration** - Commands for build, run, test, clean with keybindings
- **Semantic Highlighting** - Enhanced syntax highlighting via LSP
- **Problem Matchers** - Compiler error parsing in Problems panel
- **Context Menu** - Build and Run options in editor context menu

### 0.10.0

- **Language Server Protocol (LSP)** - Full LSP implementation
- **IntelliSense** - Autocompletion for keywords, types, builtins, and snippets
- **Hover Information** - Documentation on hover for all TML constructs
- **Markdown Support** - Syntax highlighting in ```tml code blocks
- **OOP keyword support** - Class, interface, extends, implements keywords highlighted (reserved syntax)

### 0.6.0

- Added `Maybe[T]` and `Outcome[T,E]` builtin enum support
- Added type conversion methods (`Type::from()`, `Type::default()`)
- Added primitive methods (`hash()`, `duplicate()`, `to_string()`, `cmp()`)
- Added Maybe methods (`is_just()`, `is_nothing()`, `unwrap()`, `unwrap_or()`, `expect()`)
- Added Outcome methods (`is_ok()`, `is_err()`, `unwrap()`, `unwrap_err()`, `expect()`)
- Added FFI decorators (`@extern`, `@link`)

### 0.5.0

- Added FFI decorator support (`@extern(abi)`, `@link(library)`)
- Supported ABIs: "c", "c++", "stdcall", "fastcall", "thiscall"

### 0.4.0

- Added concurrency primitives (atomic operations, fences, spinlocks)
- Added memory builtins (alloc, dealloc, mem_copy, etc.)
- Added thread primitives (thread_yield, thread_id)

### 0.3.0

- Added pointer type syntax highlighting (`*I32`, `*Bool`, etc.)
- Added lowlevel block documentation
- Improved support for low-level programming features

### 0.2.0

- Added `dyn` keyword for trait objects
- Added `@bench`, `@stable`, `@unstable` directives
- Added `Str`, `Never` primitive types
- Added `Instant`, `Duration` time types
- Added `Just`, `Nothing`, `Ok`, `Err` enum variant highlighting
- Updated license to Apache 2.0

### 0.1.0

- Initial release
- Full syntax highlighting support
- Language configuration (brackets, comments, folding)

## Development

### Building from Source

```bash
cd vscode-tml
pnpm install
pnpm compile
```

### Running Tests

The integration tests require a GUI environment (they launch a VS Code instance):

```bash
# Compile and lint
pnpm compile && pnpm lint

# Run full integration tests (requires display/GUI)
pnpm test

# Package the extension
pnpm package
```

**Note**: Tests will fail in headless environments (WSL without X server, CI without display). Use `xvfb-run` on Linux CI environments or run tests on Windows/macOS with a display.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

Apache 2.0

## About TML

TML (To Machine Language) is a compiled, statically typed programming language with an LLVM backend, designed specifically for LLM code generation and analysis. It features ownership and borrowing (without lifetime annotations), a behavior system (similar to traits), and 40+ standard library modules. Learn more at the [TML repository](https://github.com/yourusername/tml).
