# Tasks: C-Style Preprocessor Directives

## Progress: 0% (0/60 tasks complete)

## Phase 1: Preprocessor Infrastructure

### 1.1 Core Files
- [ ] 1.1.1 Create `compiler/src/preprocessor/` directory
- [ ] 1.1.2 Create `preprocessor.hpp` with main interface
- [ ] 1.1.3 Create `PreprocessorState` struct (defined symbols, condition stack)
- [ ] 1.1.4 Create `PreprocessorResult` struct (filtered source, line mappings, errors)
- [ ] 1.1.5 Implement basic preprocessor skeleton that passes through source unchanged
- [ ] 1.1.6 Add preprocessor to CMakeLists.txt

## Phase 2: Target Symbol Detection

### 2.1 OS Detection
- [ ] 2.1.1 Define WINDOWS on Windows builds
- [ ] 2.1.2 Define LINUX on Linux builds
- [ ] 2.1.3 Define MACOS on macOS builds
- [ ] 2.1.4 Define ANDROID on Android targets
- [ ] 2.1.5 Define IOS on iOS targets
- [ ] 2.1.6 Define FREEBSD on FreeBSD targets
- [ ] 2.1.7 Define UNIX family symbol (Linux, macOS, BSD, etc.)
- [ ] 2.1.8 Define POSIX symbol for POSIX-compliant systems

### 2.2 Architecture Detection
- [ ] 2.2.1 Define X86_64 on x86-64 architecture
- [ ] 2.2.2 Define X86 on 32-bit x86
- [ ] 2.2.3 Define ARM64 on AArch64
- [ ] 2.2.4 Define ARM on 32-bit ARM
- [ ] 2.2.5 Define WASM32 on WebAssembly
- [ ] 2.2.6 Define RISCV64 on RISC-V 64-bit

### 2.3 Other Detection
- [ ] 2.3.1 Define PTR_32 / PTR_64 based on pointer width
- [ ] 2.3.2 Define LITTLE_ENDIAN / BIG_ENDIAN
- [ ] 2.3.3 Define MSVC / GNU / MUSL based on environment
- [ ] 2.3.4 Add --target flag to CLI for cross-compilation
- [ ] 2.3.5 Parse target triple string (e.g., x86_64-unknown-linux-gnu)

## Phase 3: Directive Lexer

### 3.1 Directive Recognition
- [ ] 3.1.1 Create `directive_lexer.hpp/cpp`
- [ ] 3.1.2 Recognize `#if` directive
- [ ] 3.1.3 Recognize `#ifdef` directive
- [ ] 3.1.4 Recognize `#ifndef` directive
- [ ] 3.1.5 Recognize `#elif` directive
- [ ] 3.1.6 Recognize `#else` directive
- [ ] 3.1.7 Recognize `#endif` directive
- [ ] 3.1.8 Recognize `#define` directive
- [ ] 3.1.9 Recognize `#undef` directive
- [ ] 3.1.10 Recognize `#error` directive
- [ ] 3.1.11 Recognize `#warning` directive

### 3.2 Expression Tokenization
- [ ] 3.2.1 Tokenize identifiers (symbol names)
- [ ] 3.2.2 Tokenize `&&` operator
- [ ] 3.2.3 Tokenize `||` operator
- [ ] 3.2.4 Tokenize `!` operator
- [ ] 3.2.5 Tokenize `(` and `)` parentheses
- [ ] 3.2.6 Tokenize `defined` keyword
- [ ] 3.2.7 Handle string literals in #error/#warning

## Phase 4: Directive Expression Parser

### 4.1 Expression Parsing
- [ ] 4.1.1 Create `directive_parser.hpp/cpp`
- [ ] 4.1.2 Parse simple symbol: `#if SYMBOL`
- [ ] 4.1.3 Parse negation: `#if !SYMBOL`
- [ ] 4.1.4 Parse AND: `#if A && B`
- [ ] 4.1.5 Parse OR: `#if A || B`
- [ ] 4.1.6 Parse parentheses: `#if (A && B) || C`
- [ ] 4.1.7 Parse `defined(SYMBOL)` function
- [ ] 4.1.8 Implement operator precedence (! > && > ||)

## Phase 5: Directive Evaluation

### 5.1 Condition Handling
- [ ] 5.1.1 Implement `evaluate_condition()` for expression evaluation
- [ ] 5.1.2 Implement `#if` handling (push to condition stack)
- [ ] 5.1.3 Implement `#ifdef` as `#if defined(SYMBOL)`
- [ ] 5.1.4 Implement `#ifndef` as `#if !defined(SYMBOL)`
- [ ] 5.1.5 Implement `#elif` handling
- [ ] 5.1.6 Implement `#else` handling
- [ ] 5.1.7 Implement `#endif` handling (pop from stack)
- [ ] 5.1.8 Handle nested conditionals correctly

## Phase 6: Define/Undef

### 6.1 Define Handling
- [ ] 6.1.1 Implement `#define SYMBOL` (no value)
- [ ] 6.1.2 Implement `#define SYMBOL VALUE` (with value)
- [ ] 6.1.3 Implement `#undef SYMBOL`
- [ ] 6.1.4 Add `-D SYMBOL` / `--define SYMBOL` CLI flag
- [ ] 6.1.5 Add `-D SYMBOL=VALUE` support
- [ ] 6.1.6 CLI defines take precedence over source defines

## Phase 7: Error/Warning Directives

### 7.1 Message Handling
- [ ] 7.1.1 Implement `#error "message"` - emit error and stop
- [ ] 7.1.2 Implement `#warning "message"` - emit warning and continue
- [ ] 7.1.3 Include file/line in error messages

## Phase 8: Line Number Mapping

### 8.1 Source Mapping
- [ ] 8.1.1 Track original line numbers during preprocessing
- [ ] 8.1.2 Create mapping from output lines to original lines
- [ ] 8.1.3 Pass line mapping to lexer
- [ ] 8.1.4 Error messages show original file positions

## Phase 9: Integration

### 9.1 Build Pipeline
- [ ] 9.1.1 Add preprocessor pass before lexer
- [ ] 9.1.2 Pass defined symbols from CLI to preprocessor
- [ ] 9.1.3 Set DEBUG symbol when --debug is used
- [ ] 9.1.4 Set RELEASE symbol when --release is used
- [ ] 9.1.5 Set TEST symbol when running `tml test`
- [ ] 9.1.6 Update build cache to include defines in hash

## Phase 10: Testing

### 10.1 Unit Tests
- [ ] 10.1.1 Test basic `#if`/`#endif`
- [ ] 10.1.2 Test `#ifdef`/`#ifndef`
- [ ] 10.1.3 Test `#elif`/`#else` chains
- [ ] 10.1.4 Test nested conditionals
- [ ] 10.1.5 Test logical operators (`&&`, `||`, `!`)
- [ ] 10.1.6 Test `defined()` function
- [ ] 10.1.7 Test `#define`/`#undef`
- [ ] 10.1.8 Test `-D` CLI flag
- [ ] 10.1.9 Test predefined OS symbols
- [ ] 10.1.10 Test predefined architecture symbols
- [ ] 10.1.11 Test `#error` directive
- [ ] 10.1.12 Test `#warning` directive
- [ ] 10.1.13 Test line number mapping in errors
- [ ] 10.1.14 Test cross-compilation with --target

## Phase 11: Documentation

### 11.1 Update Docs
- [ ] 11.1.1 Update docs/02-LEXICAL.md with preprocessor directives
- [ ] 11.1.2 Update docs/09-CLI.md with -D flag
- [ ] 11.1.3 Add examples to docs/14-EXAMPLES.md
- [ ] 11.1.4 Update CLAUDE.md with preprocessor information

## Validation

- [ ] V.1 All directive types work correctly
- [ ] V.2 Predefined symbols set correctly per platform
- [ ] V.3 CLI -D flag works
- [ ] V.4 Cross-compilation with --target works
- [ ] V.5 Nested conditionals work
- [ ] V.6 Line numbers in errors are correct
- [ ] V.7 #error stops compilation
- [ ] V.8 #warning emits warning but continues
- [ ] V.9 Test coverage >= 90%
