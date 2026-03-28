# Proposal: C-Style Preprocessor Directives for Conditional Compilation

## Status
- **Created**: 2025-12-31
- **Status**: Draft
- **Priority**: High

## Why

TML needs preprocessor directives similar to C/C++/C# for conditional compilation. This enables:

1. **Cross-platform code** - Platform-specific implementations (Windows, Linux, macOS)
2. **Architecture optimization** - SIMD/intrinsics on specific architectures (x86_64, aarch64)
3. **Feature flags** - Conditionally include/exclude code based on enabled features
4. **Debug/Release builds** - Include debug-only code, assertions, logging
5. **LLM-generated code** - Allow LLMs to generate platform-aware code naturally

### Inspiration

- **C/C++**: `#if`, `#ifdef`, `#ifndef`, `#else`, `#elif`, `#endif`, `#define`
- **C#**: `#if`, `#elif`, `#else`, `#endif`, `#define`, `#undef`

## Proposed Syntax

### Basic Directives

```tml
#define DEBUG
#define FEATURE_ASYNC

#if DEBUG
func debug_log(msg: String) {
    println("DEBUG: {msg}")
}
#endif

#ifdef FEATURE_ASYNC
mod async_runtime {
    // async implementation
}
#endif

#ifndef WINDOWS
func unix_specific() {
    // POSIX implementation
}
#endif
```

### Predefined Symbols

The compiler automatically defines symbols based on the target:

```tml
// Operating System (one is always defined)
#if WINDOWS
    // Windows-specific code
#elif LINUX
    // Linux-specific code
#elif MACOS
    // macOS-specific code
#elif ANDROID
    // Android-specific code
#elif IOS
    // iOS-specific code
#elif FREEBSD
    // FreeBSD-specific code
#endif

// OS Families
#ifdef UNIX          // Linux, macOS, BSD, etc.
#ifdef POSIX         // POSIX-compliant systems

// Architecture (one is always defined)
#if X86_64
    // x86-64 specific
#elif X86
    // 32-bit x86
#elif ARM64
    // ARM 64-bit (aarch64)
#elif ARM
    // ARM 32-bit
#elif WASM32
    // WebAssembly
#elif RISCV64
    // RISC-V 64-bit
#endif

// Pointer width
#ifdef PTR_32        // 32-bit pointers
#ifdef PTR_64        // 64-bit pointers

// Endianness
#ifdef LITTLE_ENDIAN
#ifdef BIG_ENDIAN

// Build mode (one is always defined)
#ifdef DEBUG
#ifdef RELEASE
#ifdef TEST          // When running tests

// Environment
#ifdef MSVC          // Microsoft Visual C++
#ifdef GNU           // GNU/GCC
#ifdef MUSL          // musl libc
```

### Logical Operators

```tml
// AND - all conditions must be true
#if WINDOWS && X86_64
    func windows_x64_impl() { }
#endif

// OR - any condition can be true
#if LINUX || MACOS || FREEBSD
    func unix_like_impl() { }
#endif

// NOT - negation
#if !WINDOWS
    func non_windows_impl() { }
#endif

// Complex expressions
#if (WINDOWS && X86_64) || (LINUX && ARM64)
    func platform_optimized() { }
#endif

// Defined check with expression
#if defined(FEATURE_A) && !defined(FEATURE_B)
    // code
#endif
```

### User-Defined Symbols

```tml
// Define a symbol (no value)
#define MY_FEATURE

// Define with value (for future use with #if VALUE == X)
#define VERSION 2

// Undefine a symbol
#undef MY_FEATURE

// Conditional definition
#ifndef MY_FEATURE
#define MY_FEATURE
#endif
```

### Elif Chains

```tml
#if WINDOWS
    const PATH_SEP: Char = '\\'
#elif UNIX
    const PATH_SEP: Char = '/'
#else
    const PATH_SEP: Char = '/'  // Default fallback
#endif
```

### Nested Conditionals

```tml
#if WINDOWS
    #if X86_64
        func win64_asm() { }
    #elif X86
        func win32_asm() { }
    #endif
#elif LINUX
    #if ARM64
        func linux_arm64() { }
    #endif
#endif
```

### CLI Integration

```bash
# Define custom symbols
tml build file.tml -D MY_FEATURE
tml build file.tml -D VERSION=2
tml build file.tml --define MY_FEATURE
tml build file.tml --define VERSION=2

# Multiple defines
tml build file.tml -D FEATURE_A -D FEATURE_B -D DEBUG_LEVEL=3

# Cross-compilation (sets appropriate OS/ARCH symbols)
tml build file.tml --target x86_64-unknown-linux-gnu
tml build file.tml --target aarch64-apple-darwin

# Build modes (sets DEBUG or RELEASE)
tml build file.tml --debug    # Defines DEBUG
tml build file.tml --release  # Defines RELEASE
```

### Error Directive

```tml
#if !WINDOWS && !LINUX && !MACOS
#error "Unsupported platform"
#endif

#ifndef REQUIRED_FEATURE
#error "REQUIRED_FEATURE must be defined"
#endif
```

### Warning Directive

```tml
#ifdef DEPRECATED_API
#warning "Using deprecated API, will be removed in v2.0"
#endif
```

## Implementation

### Lexer Phase

The preprocessor runs BEFORE lexing, as a separate pass:

1. **Preprocessor Pass**: Scans source, evaluates directives, outputs filtered source
2. **Lexer Pass**: Tokenizes the filtered source
3. **Parser Pass**: Parses tokens into AST

### Preprocessor State

```cpp
struct PreprocessorState {
    std::set<std::string> defined_symbols;
    std::stack<bool> condition_stack;  // For nested #if/#endif
    bool currently_active;             // Is current code block active?
};
```

### Directive Handling

```cpp
// #if SYMBOL
// #if SYMBOL && OTHER
// #if !SYMBOL
// #if defined(SYMBOL)
void handle_if(const std::string& expr);

// #ifdef SYMBOL (shorthand for #if defined(SYMBOL))
void handle_ifdef(const std::string& symbol);

// #ifndef SYMBOL (shorthand for #if !defined(SYMBOL))
void handle_ifndef(const std::string& symbol);

// #elif EXPR
void handle_elif(const std::string& expr);

// #else
void handle_else();

// #endif
void handle_endif();

// #define SYMBOL [VALUE]
void handle_define(const std::string& symbol, std::optional<std::string> value);

// #undef SYMBOL
void handle_undef(const std::string& symbol);

// #error "message"
void handle_error(const std::string& message);

// #warning "message"
void handle_warning(const std::string& message);
```

## What Changes

### New Files

- `compiler/src/preprocessor/preprocessor.hpp` - Preprocessor interface
- `compiler/src/preprocessor/preprocessor.cpp` - Main preprocessor logic
- `compiler/src/preprocessor/directive_parser.cpp` - Parse directive expressions
- `compiler/src/preprocessor/target_symbols.cpp` - Generate target-based symbols

### Modified Files

- `compiler/src/cli/dispatcher.cpp` - Add `-D`/`--define` flags
- `compiler/src/cli/cmd_build.cpp` - Run preprocessor before compilation
- `compiler/src/lexer/lexer_core.cpp` - Skip preprocessor directives (already handled)

## Impact

- **Breaking change**: NO (new feature)
- **User benefit**: Cross-platform code, feature toggles, conditional compilation

## Success Criteria

1. `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif` work correctly
2. `#define` and `#undef` work correctly
3. Predefined symbols (WINDOWS, LINUX, X86_64, etc.) are set automatically
4. `-D` CLI flag works for custom symbols
5. Logical operators (`&&`, `||`, `!`) evaluate correctly
6. `defined()` function works in expressions
7. Nested conditionals work correctly
8. `#error` and `#warning` emit appropriate messages
9. Clear error messages for malformed directives
10. Line numbers in errors map to original source correctly

## References

- C preprocessor: https://en.cppreference.com/w/cpp/preprocessor
- C# preprocessor: https://docs.microsoft.com/en-us/dotnet/csharp/language-reference/preprocessor-directives
- Rust cfg (alternative approach): https://doc.rust-lang.org/reference/conditional-compilation.html
