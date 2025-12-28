# TML Language Support for Visual Studio Code

Syntax highlighting and language support for TML (To Machine Language).

## Features

- **Syntax Highlighting**: Full syntax highlighting for TML language
  - Keywords (func, type, behavior, impl, dyn, etc.)
  - Types (I32, U64, F64, Str, Bool, Instant, Duration, etc.)
  - Builtin enums: `Maybe[T]` (Just/Nothing), `Outcome[T,E]` (Ok/Err), `Ordering`
  - Operators (and, or, not, +, -, *, /, etc.)
  - Literals (integers, floats, strings, characters)
  - Comments (line, block, doc, AI comments)
  - Directives (@test, @bench, @stable, @deprecated, @extern, @link, etc.)
  - Builtin methods (hash, duplicate, to_string, is_just, unwrap, etc.)
  - Static type methods (Type::from, Type::default)

- **Language Configuration**:
  - Auto-closing brackets, quotes, and parentheses
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

### Directives
```tml
@test
func test_addition() {
    assert_eq(2 + 2, 4, "Math works!")
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

None at this time.

## Release Notes

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

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

Apache 2.0

## About TML

TML (To Machine Language) is a programming language specification designed specifically for LLM code generation and analysis. Learn more at the [TML repository](https://github.com/yourusername/tml).
