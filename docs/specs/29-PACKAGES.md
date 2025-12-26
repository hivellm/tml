# TML v1.0 — Package System

## 1. Overview

TML uses a package-based code organization system inspired by Rust's Cargo and Node's npm. Each package is a self-contained unit with:

- **Manifest** - `package.toml` configuration file
- **Source code** - Organized in `src/` directory
- **Tests** - In `tests/` and `*.test.tml` files
- **Examples** - In `examples/` directory
- **Documentation** - Markdown files

## 2. Package Structure

### 2.1 Standard Layout

```
my-package/
├── package.toml          # Package manifest (required)
├── README.md             # Package description
├── LICENSE               # License file
├── CHANGELOG.md          # Version history
├── .gitignore            # Git ignore patterns
├── src/
│   ├── mod.tml           # Module root (library entry)
│   ├── main.tml          # Binary entry (for executables)
│   ├── lib.tml           # Alternative library entry
│   └── submodule/
│       ├── mod.tml       # Submodule root
│       └── utils.tml     # Module files
├── tests/
│   ├── integration.test.tml
│   └── e2e.test.tml
├── examples/
│   ├── basic.tml
│   └── advanced.tml
├── benches/
│   └── performance.bench.tml
└── docs/
    ├── api.md
    └── guide.md
```

### 2.2 Minimal Package

Minimum required structure:

```
minimal/
├── package.toml
└── src/
    └── mod.tml
```

### 2.3 Binary Package

For executable applications:

```
my-app/
├── package.toml
└── src/
    └── main.tml    # Entry point with main() function
```

### 2.4 Library Package

For reusable libraries:

```
my-lib/
├── package.toml
└── src/
    ├── mod.tml     # Public API
    └── internal.tml
```

### 2.5 Workspace

Multiple packages in monorepo:

```
workspace/
├── workspace.toml        # Workspace configuration
├── packages/
│   ├── core/
│   │   ├── package.toml
│   │   └── src/
│   ├── utils/
│   │   ├── package.toml
│   │   └── src/
│   └── cli/
│       ├── package.toml
│       └── src/
└── README.md
```

## 3. Package Manifest (package.toml)

### 3.1 Basic Fields

```toml
[package]
name = "my-package"              # Package name (required)
version = "0.1.0"                # Semantic version (required)
edition = "2025"                 # TML edition
authors = ["Name <email@example.com>"]
description = "A brief description"
license = "Apache-2.0"
repository = "https://github.com/user/repo"
homepage = "https://example.com"
documentation = "https://docs.example.com"
readme = "README.md"
keywords = ["web", "http", "server"]
categories = ["network-programming"]
```

### 3.2 Dependencies

```toml
[dependencies]
http = "1.2.0"                   # Version requirement
json = { version = "2.0", features = ["fast"] }
local-lib = { path = "../local-lib" }
git-lib = { git = "https://github.com/user/repo", tag = "v1.0" }

[dev-dependencies]
test = "0.1.0"                   # Only for tests
bench = "0.2.0"                  # Only for benchmarks

[build-dependencies]
codegen = "1.0.0"                # Only for build scripts
```

### 3.3 Library Configuration

```toml
[lib]
name = "mylib"                   # Library name
path = "src/mod.tml"             # Entry point
crate-type = ["lib"]             # Output type

[[bin]]
name = "my-app"                  # Binary name
path = "src/main.tml"            # Entry point
required-features = ["cli"]      # Optional features needed
```

### 3.4 Features

```toml
[features]
default = ["std"]                # Default features
std = []                         # Standard library support
async = ["tokio"]                # Async runtime
cli = ["clap", "colored"]        # CLI dependencies
full = ["std", "async", "cli"]   # All features
```

### 3.5 Target Configuration

```toml
[target.x86_64-unknown-linux]
dependencies = { openssl = "1.0" }

[target.wasm32-unknown-unknown]
dependencies = { wasm-bindgen = "0.2" }
```

### 3.6 Build Configuration

```toml
[build]
script = "build.tml"             # Build script
incremental = true               # Incremental compilation
opt-level = 2                    # Optimization level (0-3)
debug = false                    # Include debug info
```

### 3.7 Profile Configuration

```toml
[profile.dev]
opt-level = 0                    # No optimization
debug = true                     # Full debug info
incremental = true               # Fast rebuilds

[profile.release]
opt-level = 3                    # Maximum optimization
debug = false                    # No debug info
lto = true                       # Link-time optimization
codegen-units = 1                # Single codegen unit
```

### 3.8 Metadata

```toml
[package.metadata.tml]
min-version = "0.1.0"            # Minimum TML version

[package.metadata.docs]
rustdoc-args = ["--theme", "dark"]
```

## 4. Version Requirements

### 4.1 Semver Syntax

```toml
[dependencies]
# Exact version
exact = "=1.2.3"

# Compatible (^)
compat = "^1.2.3"    # >= 1.2.3, < 2.0.0
compat2 = "1.2.3"    # Same as ^1.2.3

# Tilde (~)
tilde = "~1.2.3"     # >= 1.2.3, < 1.3.0
tilde2 = "~1.2"      # >= 1.2.0, < 1.3.0

# Wildcard (*)
wild = "1.*"         # >= 1.0.0, < 2.0.0
wild2 = "*"          # Any version

# Range
range = ">= 1.2.0, < 2.0.0"

# Multiple
multi = ">= 1.2, < 1.5"
```

### 4.2 Pre-release Versions

```toml
[dependencies]
alpha = "0.1.0-alpha"
beta = "1.0.0-beta.2"
rc = "2.0.0-rc.1"
```

## 5. Package Types

### 5.1 Library (lib)

Provides reusable code:

```toml
[package]
name = "mylib"

[lib]
path = "src/mod.tml"
```

**Entry point (`src/mod.tml`):**
```tml
pub mod utils
pub mod types

pub func hello() -> Str {
    "Hello from mylib"
}
```

### 5.2 Binary (bin)

Executable application:

```toml
[package]
name = "myapp"

[[bin]]
name = "myapp"
path = "src/main.tml"
```

**Entry point (`src/main.tml`):**
```tml
func main() {
    println("Hello, world!")
}
```

### 5.3 Hybrid (lib + bin)

Both library and binary:

```toml
[package]
name = "myproject"

[lib]
path = "src/lib.tml"

[[bin]]
name = "myproject"
path = "src/main.tml"
```

### 5.4 Multi-Binary

Multiple executables:

```toml
[[bin]]
name = "server"
path = "src/bin/server.tml"

[[bin]]
name = "client"
path = "src/bin/client.tml"

[[bin]]
name = "cli"
path = "src/bin/cli.tml"
```

Directory structure:
```
src/
├── lib.tml
└── bin/
    ├── server.tml
    ├── client.tml
    └── cli.tml
```

## 6. Module System Integration

### 6.1 Package as Module

Each package is a module:

```tml
// Using a package
use mylib                    // Import entire package
use mylib::utils             // Import submodule
use mylib::{hello, goodbye}  // Import specific items
```

### 6.2 Module Resolution

```
mylib/
├── src/
│   ├── mod.tml              # mylib::
│   ├── utils.tml            # mylib::utils
│   ├── types/
│   │   ├── mod.tml          # mylib::types::
│   │   └── error.tml        # mylib::types::error
│   └── internal/
│       └── private.tml      # mylib::internal::private
```

### 6.3 Re-exports

```tml
// src/mod.tml
pub use utils::helper
pub use types::Error
pub mod types
mod internal  // Not pub - private to package
```

## 7. Testing Structure

### 7.1 Unit Tests

Inline tests in source files:

```tml
// src/math.tml
pub func add(a: I32, b: I32) -> I32 {
    a + b
}

@test
func test_add() {
    assert_eq!(add(2, 2), 4)
}
```

### 7.2 Integration Tests

Separate test files:

```tml
// tests/integration.test.tml
use mylib

@test
func test_public_api() {
    let result: Str = mylib::hello()
    assert_eq!(result, "Hello from mylib")
}
```

### 7.3 Test Organization

```
mylib/
├── src/
│   ├── mod.tml
│   ├── math.tml              # Unit tests inline
│   └── math.test.tml         # Additional unit tests
└── tests/
    ├── integration.test.tml  # Integration tests
    └── e2e.test.tml          # End-to-end tests
```

### 7.4 Test Configuration

```toml
[package]
name = "mylib"

[[test]]
name = "integration"
path = "tests/integration.test.tml"
harness = true               # Use built-in test harness

[[test]]
name = "custom"
path = "tests/custom.test.tml"
harness = false              # Custom test runner
```

## 8. Examples

### 8.1 Example Files

```
examples/
├── basic.tml                # Simple example
├── advanced.tml             # Complex example
└── tutorial/
    ├── step1.tml
    └── step2.tml
```

### 8.2 Running Examples

```bash
tml run --example basic
tml run --example tutorial/step1
```

### 8.3 Example Configuration

```toml
[[example]]
name = "basic"
path = "examples/basic.tml"
required-features = ["std"]
```

## 9. Benchmarks

### 9.1 Benchmark Files

```
benches/
├── performance.bench.tml
└── memory.bench.tml
```

### 9.2 Benchmark Configuration

```toml
[[bench]]
name = "performance"
path = "benches/performance.bench.tml"
harness = true
```

### 9.3 Running Benchmarks

```bash
tml test --bench
tml test --bench performance
```

## 10. Package Registry

### 10.1 Publishing

```bash
tml package publish
```

### 10.2 Registry Configuration

```toml
[registry]
default = "https://packages.tml-lang.org"

[registry.custom]
index = "https://my-registry.com"
token = "${TML_REGISTRY_TOKEN}"
```

### 10.3 Package Metadata

```toml
[package]
name = "mylib"
version = "1.0.0"
publish = true               # Allow publishing

[package.metadata.registry]
categories = ["web", "http"]
badges = { travis-ci = { repository = "user/repo" } }
```

## 11. Build Scripts

### 11.1 Build Script

```tml
// build.tml
func main() {
    println("Running build script")
    // Code generation, asset compilation, etc.
}
```

### 11.2 Build Configuration

```toml
[build]
script = "build.tml"
```

### 11.3 Build Dependencies

```toml
[build-dependencies]
codegen = "1.0.0"
```

## 12. Workspaces

### 12.1 Workspace Configuration

```toml
# workspace.toml
[workspace]
members = [
    "packages/core",
    "packages/utils",
    "packages/cli",
]

exclude = [
    "packages/experimental",
]

[workspace.dependencies]
common-lib = "1.0.0"

[workspace.metadata]
repository = "https://github.com/user/monorepo"
```

### 12.2 Member Package

```toml
# packages/core/package.toml
[package]
name = "core"
version = "0.1.0"

[dependencies]
common-lib = { workspace = true }  # Use workspace version
```

### 12.3 Workspace Commands

```bash
tml build                    # Build all members
tml test                     # Test all members
tml build -p core            # Build specific package
```

## 13. Package Commands

### 13.1 Create Package

```bash
tml new mylib                # Library
tml new myapp --bin          # Binary
tml new myproject --lib      # Explicit library
```

### 13.2 Build Package

```bash
tml build                    # Build package
tml build --release          # Release build
tml build --features async   # Enable features
```

### 13.3 Test Package

```bash
tml test                     # Run all tests
tml test pattern             # Filter tests
tml test --doc               # Test documentation
```

### 13.4 Run Package

```bash
tml run                      # Run binary
tml run --bin server         # Run specific binary
tml run --example basic      # Run example
```

### 13.5 Check Package

```bash
tml check                    # Type check only
tml check --all-features     # Check with all features
```

### 13.6 Package Management

```bash
tml install dep              # Add dependency
tml remove dep               # Remove dependency
tml update                   # Update dependencies
tml tree                     # Show dependency tree
```

## 14. Standard Packages

### 14.1 Core Packages

```
packages/
├── std/                     # Standard library
├── core/                    # Core primitives
├── alloc/                   # Allocation
├── test/                    # Testing framework
└── bench/                   # Benchmarking
```

### 14.2 Package Dependencies

```
std -> core, alloc
test -> std
bench -> std
```

## 15. Package Best Practices

### 15.1 Naming

- **Library:** `my-lib` (kebab-case)
- **Binary:** `my-app` (kebab-case)
- **Module:** `my_lib` (snake_case in code)

### 15.2 Versioning

Follow Semantic Versioning (semver):
- **Major:** Breaking changes
- **Minor:** New features (backward compatible)
- **Patch:** Bug fixes

### 15.3 Documentation

Required files:
- `README.md` - Overview and quick start
- `LICENSE` - License terms
- `CHANGELOG.md` - Version history

### 15.4 Testing

- Unit tests inline or in `*.test.tml`
- Integration tests in `tests/`
- Examples in `examples/`
- Benchmarks in `benches/`

### 15.5 Code Organization

```
src/
├── mod.tml              # Public API (small, re-exports)
├── lib.tml              # Alternative entry
├── types.tml            # Core types
├── error.tml            # Error types
├── utils.tml            # Utilities
├── internal/            # Private implementation
└── feature/             # Feature-gated code
```

## 16. Migration Guide

### 16.1 From Single File

Before:
```
myapp.tml
```

After:
```
myapp/
├── package.toml
└── src/
    └── main.tml
```

### 16.2 From Multiple Files

Before:
```
src/
├── main.tml
├── utils.tml
└── types.tml
```

After:
```
myapp/
├── package.toml
└── src/
    ├── main.tml
    ├── utils.tml
    └── types.tml
```

## 17. Examples

### 17.1 Simple Library

```toml
[package]
name = "math-utils"
version = "0.1.0"
authors = ["Alice <alice@example.com>"]
license = "Apache-2.0"

[lib]
path = "src/mod.tml"
```

### 17.2 Web Application

```toml
[package]
name = "web-app"
version = "1.0.0"
edition = "2025"

[[bin]]
name = "web-app"
path = "src/main.tml"

[dependencies]
http = "2.0"
router = "1.5"
templates = "0.8"

[features]
default = ["std"]
std = []
async = ["tokio"]

[profile.release]
opt-level = 3
lto = true
```

### 17.3 CLI Tool

```toml
[package]
name = "tml-cli"
version = "0.1.0"

[[bin]]
name = "tml"
path = "src/main.tml"

[dependencies]
clap = "4.0"
colored = "2.0"
```

---

*Previous: [14-EXAMPLES.md](./14-EXAMPLES.md)*
*Next: [16-COMPILER.md](./16-COMPILER.md) — Compiler Architecture*
