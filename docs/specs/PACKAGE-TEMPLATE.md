# TML Package Template

This document provides standard templates for creating TML packages.

## Library Package Template

### Directory Structure

```
my-library/
├── package.toml
├── README.md
├── LICENSE
├── CHANGELOG.md
├── .gitignore
├── src/
│   ├── mod.tml
│   ├── types.tml
│   ├── error.tml
│   └── utils.tml
├── tests/
│   └── integration.test.tml
├── examples/
│   ├── basic.tml
│   └── advanced.tml
└── docs/
    └── api.md
```

### package.toml

```toml
[package]
name = "my-library"
version = "0.1.0"
edition = "2025"
authors = ["Your Name <you@example.com>"]
description = "A brief description of your library"
license = "Apache-2.0"
repository = "https://github.com/username/my-library"
homepage = "https://my-library.example.com"
documentation = "https://docs.example.com/my-library"
readme = "README.md"
keywords = ["keyword1", "keyword2", "keyword3"]
categories = ["category"]

[lib]
name = "my_library"
path = "src/mod.tml"

[dependencies]
# Add your dependencies here

[dev-dependencies]
test = "0.1.0"

[features]
default = ["std"]
std = []

[profile.dev]
opt-level = 0
debug = true

[profile.release]
opt-level = 3
debug = false
lto = true
```

### src/mod.tml

```tml
// Library root - public API
pub mod types
pub mod error
mod utils  // Private module

pub use types::{MyType, OtherType}
pub use error::Error

/// Main library function
pub func do_something(input: Str) -> Outcome[MyType, Error] {
    // Implementation
    Ok(MyType::new(input))
}
```

### src/types.tml

```tml
// Core types

pub type MyType {
    pub value: Str,
}

impl MyType {
    pub func new(value: Str) -> MyType {
        MyType { value: value }
    }

    pub func process(this) -> Str {
        this.value.to_upper()
    }
}

pub type OtherType {
    pub count: I32,
}
```

### src/error.tml

```tml
// Error types

pub type Error {
    InvalidInput(message: Str),
    ProcessingFailed(reason: Str),
    Io(path: Str),
}

impl Error {
    pub func message(this) -> Str {
        when this {
            Error::InvalidInput(msg) => "Invalid input: " + msg,
            Error::ProcessingFailed(reason) => "Processing failed: " + reason,
            Error::Io(path) => "I/O error at: " + path,
        }
    }
}
```

### src/utils.tml

```tml
// Internal utilities (not exported)

func validate_input(input: Str) -> Bool {
    input.len() > 0 and input.len() < 1000
}

func sanitize(input: Str) -> Str {
    input.trim()
}
```

### tests/integration.test.tml

```tml
use test
use my_library

@test
func test_basic_usage() {
    let result: Outcome[MyType, Error] = my_library::do_something("test")
    assert_ok!(result)
}

@test
func test_type_creation() {
    let t: MyType = MyType::new("hello")
    assert_eq!(t.value, "hello")
}

@test
func test_processing() {
    let t: MyType = MyType::new("hello")
    assert_eq!(t.process(), "HELLO")
}
```

### examples/basic.tml

```tml
use my_library

func main() {
    println("Basic Example")

    let result: Outcome[MyType, Error] = my_library::do_something("Hello")

    when result {
        Ok(value) => println("Success: " + value.process()),
        Err(e) => println("Error: " + e.message()),
    }
}
```

### README.md

```markdown
# My Library

Brief description of what your library does.

## Features

- Feature 1
- Feature 2
- Feature 3

## Installation

Add this to your `package.toml`:

```toml
[dependencies]
my-library = "0.1.0"
```

## Quick Start

```tml
use my_library

func main() {
    let result = my_library::do_something("input")
    // Use result
}
```

## Documentation

Full documentation is available at [docs.example.com/my-library](https://docs.example.com/my-library).

## Examples

See the `examples/` directory for more examples:

```bash
tml run --example basic
tml run --example advanced
```

## Testing

```bash
tml test
```

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.
```

## Binary Package Template

### Directory Structure

```
my-application/
├── package.toml
├── README.md
├── LICENSE
├── .gitignore
├── src/
│   ├── main.tml
│   ├── cli.tml
│   ├── config.tml
│   └── commands/
│       ├── mod.tml
│       ├── run.tml
│       └── build.tml
├── tests/
│   └── cli.test.tml
└── docs/
    └── usage.md
```

### package.toml

```toml
[package]
name = "my-application"
version = "1.0.0"
edition = "2025"
authors = ["Your Name <you@example.com>"]
description = "A command-line application"
license = "Apache-2.0"
repository = "https://github.com/username/my-application"

[[bin]]
name = "my-app"
path = "src/main.tml"

[dependencies]
# CLI dependencies
# colored = "2.0"
# config = "1.0"

[dev-dependencies]
test = "0.1.0"

[profile.release]
opt-level = 3
lto = true
codegen-units = 1
```

### src/main.tml

```tml
use cli
use config
use commands

func main() {
    let cfg: Config = config::load()
    let args: [Str] = std::env::args()

    if args.len() < 2 then {
        cli::print_help()
        return
    }

    let command: Str = args[1]

    when command {
        "run" => commands::run::execute(cfg, args),
        "build" => commands::build::execute(cfg, args),
        "--help" | "-h" => cli::print_help(),
        "--version" | "-V" => cli::print_version(),
        _ => {
            println("Unknown command: " + command)
            cli::print_help()
        },
    }
}
```

### src/cli.tml

```tml
pub func print_help() {
    println("My Application v1.0.0")
    println("")
    println("USAGE:")
    println("    my-app <COMMAND>")
    println("")
    println("COMMANDS:")
    println("    run      Run the application")
    println("    build    Build the project")
    println("    help     Print this message")
    println("")
}

pub func print_version() {
    println("my-app 1.0.0")
}
```

### src/config.tml

```tml
pub type Config {
    pub verbose: Bool,
    pub output_dir: Str,
}

pub func load() -> Config {
    Config {
        verbose: false,
        output_dir: "./output",
    }
}
```

## Hybrid Package Template (Library + Binary)

### package.toml

```toml
[package]
name = "my-project"
version = "0.1.0"

[lib]
name = "my_project"
path = "src/lib.tml"

[[bin]]
name = "my-project"
path = "src/main.tml"

[dependencies]
# Shared dependencies
```

### Directory Structure

```
my-project/
├── package.toml
├── src/
│   ├── lib.tml          # Library entry
│   ├── main.tml         # Binary entry
│   ├── core/            # Shared core logic
│   │   └── mod.tml
│   └── bin/
│       └── cli.tml      # CLI-specific code
```

### src/lib.tml

```tml
// Library API
pub mod core

pub use core::{process, Config}
```

### src/main.tml

```tml
// Binary entry point
use my_project::core

func main() {
    let config: Config = core::Config::default()
    core::process(config)
}
```

## Multi-Binary Package Template

### package.toml

```toml
[package]
name = "my-tools"
version = "0.1.0"

[[bin]]
name = "server"
path = "src/bin/server.tml"

[[bin]]
name = "client"
path = "src/bin/client.tml"

[[bin]]
name = "admin"
path = "src/bin/admin.tml"

[lib]
path = "src/lib.tml"
```

### Directory Structure

```
my-tools/
├── package.toml
├── src/
│   ├── lib.tml          # Shared library code
│   ├── common/          # Common modules
│   │   ├── mod.tml
│   │   └── protocol.tml
│   └── bin/
│       ├── server.tml
│       ├── client.tml
│       └── admin.tml
```

## Workspace Template

### workspace.toml

```toml
[workspace]
members = [
    "packages/core",
    "packages/utils",
    "packages/cli",
    "packages/server",
]

[workspace.dependencies]
# Shared dependencies
log = "1.0"
config = "2.0"

[workspace.metadata]
repository = "https://github.com/username/workspace"
license = "Apache-2.0"
```

### Directory Structure

```
my-workspace/
├── workspace.toml
├── README.md
├── packages/
│   ├── core/
│   │   ├── package.toml
│   │   └── src/
│   │       └── mod.tml
│   ├── utils/
│   │   ├── package.toml
│   │   └── src/
│   │       └── mod.tml
│   ├── cli/
│   │   ├── package.toml
│   │   └── src/
│   │       └── main.tml
│   └── server/
│       ├── package.toml
│       └── src/
│           └── main.tml
```

### packages/core/package.toml

```toml
[package]
name = "core"
version = "0.1.0"

[dependencies]
log = { workspace = true }
```

## Testing Package Template

### tests/integration.test.tml

```tml
use test
use my_library

@test
func test_feature_a() {
    // Test implementation
}

@test
func test_feature_b() {
    // Test implementation
}

@test
@should_panic
func test_error_handling() {
    // Should panic
}
```

### tests/common/mod.tml

```tml
// Common test utilities

pub func setup() -> TestContext {
    TestContext::new()
}

pub func teardown(ctx: TestContext) {
    ctx.cleanup()
}

pub type TestContext {
    // Test state
}
```

## .gitignore Template

```gitignore
# Build artifacts
/target/
/build/
*.exe
*.wasm

# IDE
.vscode/
.idea/
*.swp
*.swo

# OS
.DS_Store
Thumbs.db

# Package
/Cargo.lock  # For libraries, commit for binaries
package-lock.toml

# Temporary
*.tmp
*.log
```

## LICENSE Template (Apache 2.0)

```
Copyright 2025 Your Name

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

## CHANGELOG.md Template

```markdown
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- New features go here

### Changed
- Changes to existing functionality

### Deprecated
- Features that will be removed

### Removed
- Removed features

### Fixed
- Bug fixes

### Security
- Security fixes

## [0.1.0] - 2025-01-01

### Added
- Initial release
- Basic functionality
```
