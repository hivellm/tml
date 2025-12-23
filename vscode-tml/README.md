# TML Language Support for Visual Studio Code

Syntax highlighting and language support for TML (To Machine Language).

## Features

- **Syntax Highlighting**: Full syntax highlighting for TML language
  - Keywords (func, type, behavior, extend, etc.)
  - Types (I32, U64, F64, String, Bool, etc.)
  - Operators (and, or, not, +, -, *, /, etc.)
  - Literals (integers, floats, strings, characters)
  - Comments (line, block, doc, AI comments)
  - Directives (@test, @when, @auto, etc.)

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

### 0.1.0

- Initial release
- Full syntax highlighting support
- Language configuration (brackets, comments, folding)

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

MIT

## About TML

TML (To Machine Language) is a programming language specification designed specifically for LLM code generation and analysis. Learn more at the [TML repository](https://github.com/yourusername/tml).
