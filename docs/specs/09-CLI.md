# TML v1.0 — Toolchain CLI

## 1. Overview

`tml` is the unified command-line tool for TML.

```bash
tml --version
# tml 1.0.0 (abc1234 2024-01-15)

tml --help
# TML Toolchain
#
# USAGE:
#     tml <COMMAND> [OPTIONS]
#
# COMMANDS:
#     new         Create a new project
#     build       Compile the project
#     run         Build and execute
#     test        Run tests
#     check       Check for errors without compiling
#     fmt         Format source code
#     lint        Run linter
#     doc         Generate documentation
#     repl        Interactive REPL
#     ir          Generate IR
#     clean       Remove build artifacts
```

## 2. Main Commands

### 2.1 tml new — Create Project

```bash
tml new myproject

# Structure created:
# myproject/
# ├── tml.toml
# ├── src/
# │   └── lib.tml
# └── tests/
#     └── lib_test.tml

# Options
tml new myproject --bin      # executable (src/main.tml)
tml new myproject --lib      # library (default)
```

### 2.2 tml build — Compile

```bash
# Debug build
tml build

# Release build
tml build --release

# Specific target
tml build --target wasm32
tml build --target x86_64-linux

# With features
tml build --features "async,serde"

# JSON output (for IDEs/LLMs)
tml build --message-format json
```

Output:
```
   Compiling myproject v1.0.0
    Building src/lib.tml
    Finished dev [unoptimized + debuginfo] in 0.54s
```

### 2.3 tml run — Execute

```bash
# Build and run
tml run

# With arguments
tml run -- arg1 arg2 --flag

# Release mode
tml run --release

# Watch mode
tml run --watch
```

### 2.4 tml test — Tests

```bash
# All tests
tml test

# Filter
tml test add              # tests containing "add"
tml test --filter "test_*"
tml test --module math

# Parallel
tml test --jobs 4

# Coverage
tml test --coverage
tml test --coverage --format html

# Verbose
tml test --verbose
```

Output:
```
   Running tests for myproject v1.0.0

running 12 tests
test math::test_add ... ok (0.1ms)
test math::test_subtract ... ok (0.1ms)
test math::test_multiply ... ok (0.2ms)

test result: ok. 12 passed; 0 failed
   finished in 0.32s
```

### 2.5 tml check — Quick Verification

```bash
# Check without generating code
tml check

# Include tests
tml check --tests
```

Faster than build, ideal for feedback during development.

### 2.6 tml fmt — Formatting

```bash
# Format all
tml fmt

# Check without modifying
tml fmt --check

# Specific file
tml fmt src/main.tml
```

### 2.7 tml lint — Linter

```bash
# All checks
tml lint

# Auto fix
tml lint --fix

# Levels
tml lint --warn all
tml lint --deny unused
```

Categories:
- `correctness` — probable errors
- `performance` — inefficiencies
- `style` — conventions
- `unused` — dead code
- `security` — vulnerabilities

### 2.8 tml doc — Documentation

```bash
# Generate docs
tml doc

# Open in browser
tml doc --open

# JSON format
tml doc --format json
```

### 2.9 tml repl — REPL

```bash
tml repl

# >>> let x = 42
# x: I32 = 42
#
# >>> x * 2
# I32 = 84
#
# >>> :type x
# I32
#
# >>> :load src/lib.tml
# Loaded 15 definitions
#
# >>> :help
# Commands:
#   :help    Show help
#   :type    Show type
#   :load    Load file
#   :clear   Clear state
#   :quit    Exit
```

### 2.10 tml ir — Generate IR

```bash
# S-expression
tml ir src/lib.tml

# JSON
tml ir src/lib.tml --format json

# To file
tml ir src/lib.tml --output lib.tml.ir
```

### 2.11 tml clean — Cleanup

```bash
tml clean           # remove target/
tml clean --all     # remove everything including cache
```

## 3. Dependencies

### 3.1 tml add

```bash
tml add serde
tml add tokio@1.0
tml add tokio --features "full"
tml add mylib --path ../mylib
tml add mylib --git https://github.com/user/mylib
```

### 3.2 tml remove

```bash
tml remove serde
```

### 3.3 tml update

```bash
tml update          # all
tml update serde    # specific
tml update --dry-run
```

### 3.4 tml tree

```bash
tml tree

# myproject v1.0.0
# ├── serde v1.0.152
# │   └── serde_derive v1.0.152
# └── tokio v1.28.0
#     ├── bytes v1.4.0
#     └── mio v0.8.6
```

## 4. Workspaces

```bash
tml build --workspace
tml build -p core          # specific package
tml test --workspace
```

## 5. Configuration

### 5.1 tml.toml

```toml
[package]
name = "myproject"
version = "1.0.0"
edition = "2024"

[build]
jobs = 8
incremental = true

[test]
threads = 4
timeout = 30

[fmt]
max_width = 100
indent_size = 4

[lint]
deny = ["unused", "deprecated"]
```

### 5.2 Environment Variables

```bash
TML_HOME           # ~/.tml
TML_CACHE_DIR      # package cache
TML_LOG            # log level (error, warn, info, debug)
TML_JOBS           # parallel jobs
```

## 6. Output for LLMs

### 6.1 Structured JSON

```bash
tml build --message-format json
```

```json
{"reason":"compiler-artifact","target":"lib","success":true}
{"reason":"compiler-message","message":{"code":"E001","level":"error","message":"Type mismatch","spans":[{"file":"src/lib.tml","line":42,"column":15,"id":"@let_result"}]}}
{"reason":"build-finished","success":false}
```

### 6.2 Diagnostics for LLM

```bash
tml check --error-format llm
```

Optimized output with:
- Expanded context
- Suggestions as patches
- References by @id
- Type metadata

## 7. IDE Integration

### 7.1 Language Server

```bash
tml lsp
```

Features:
- Auto-complete
- Go to definition
- Find references
- Hover docs
- Diagnostics
- Code actions
- Rename
- Format

### 7.2 VSCode Settings

```json
{
  "tml.server.path": "tml",
  "tml.server.args": ["lsp"],
  "tml.format.enable": true
}
```

---

*Previous: [08-IR.md](./08-IR.md)*
*Next: [10-TESTING.md](./10-TESTING.md) — Testing Framework*
