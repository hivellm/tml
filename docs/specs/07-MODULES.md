# TML v1.0 — Module System

## 1. Structure

### 1.1 Module Declaration

```tml
// src/lib.tml
module mylib

public func hello() {
    print("Hello from mylib!")
}
```

```tml
// src/utils.tml
module mylib.utils

public func helper() {
    // ...
}
```

### 1.2 File Hierarchy

```
myproject/
├── tml.toml
├── src/
│   ├── lib.tml          # module myproject
│   ├── utils.tml        # module myproject.utils
│   ├── http/
│   │   ├── mod.tml      # module myproject.http
│   │   ├── client.tml   # module myproject.http.client
│   │   └── server.tml   # module myproject.http.server
│   └── main.tml         # module main (executable)
└── tests/
    └── test_utils.tml
```

## 2. Imports

### 2.1 Simple Import

```tml
import std.io
import std.collections.List

// Usage
io.print("hello")
let list: List[T] = List.new()
```

### 2.2 Import with Alias

```tml
import std.collections.HashMap as Map
import very.long.module.name as short

let m: Map[String, I32] = Map.new()
```

### 2.3 Multiple Import

```tml
import std.collections.{List, Map, Set}
import std.io.{read, write, File}
```

### 2.4 Glob Import

```tml
import std.prelude.*

// Brings all public items
// Use sparingly
```

### 2.5 Re-export

```tml
// lib.tml
module mylib

public import internal.Parser
public import internal.Lexer

// Users do: import mylib.Parser
```

## 3. Visibility

### 3.1 public vs private

```tml
module mylib

public type User {
    public name: String,    // public field
    private password: String,  // private field
}

public func create_user(name: String) -> User {
    return User {
        name: name,
        password: generate_password(),
    }
}

private func generate_password() -> String {
    // not visible outside module
    return random_string(16)
}
```

### 3.2 Visibility Rules

| Declaration | Visible in |
|-------------|------------|
| `private` (default) | Same module |
| `public` | Anywhere |

### 3.3 Opaque Types

```tml
// Public type, private fields
public type Handle {
    private id: U64,
    private data: ptr Unit,
}

// Only module functions can create/access
public func create() -> Handle {
    return Handle { id: next_id(), data: alloc() }
}
```

## 4. Packages

### 4.1 tml.toml

```toml
[package]
name = "myproject"
version = "1.0.0"
edition = "2024"
authors = ["Alice <alice@example.com>"]
license = "MIT"
description = "My awesome project"
repository = "https://github.com/alice/myproject"

[dependencies]
serde = "1.0"
tokio = { version = "1.28", features = ["full"] }
local_lib = { path = "../local_lib" }
git_lib = { git = "https://github.com/user/lib", branch = "main" }

[dev-dependencies]
test_utils = "0.1"

[build]
target = "native"  # or "wasm32", "aarch64-linux"

[features]
default = ["std"]
std = []
async = ["tokio"]
```

### 4.2 Lock File

```toml
# tml.lock (generated automatically)
[[package]]
name = "serde"
version = "1.0.152"
checksum = "abc123..."

[[package]]
name = "tokio"
version = "1.28.0"
checksum = "def456..."
dependencies = ["mio", "bytes"]
```

## 5. Workspaces

### 5.1 Structure

```
workspace/
├── tml.toml           # workspace root
├── core/
│   ├── tml.toml
│   └── src/
├── cli/
│   ├── tml.toml
│   └── src/
└── web/
    ├── tml.toml
    └── src/
```

### 5.2 Workspace tml.toml

```toml
[workspace]
members = ["core", "cli", "web"]

[workspace.dependencies]
serde = "1.0"
log = "0.4"

[workspace.package]
version = "1.0.0"
edition = "2024"
```

### 5.3 Member tml.toml

```toml
[package]
name = "cli"
version.workspace = true
edition.workspace = true

[dependencies]
core = { path = "../core" }
serde = { workspace = true }
clap = "4.0"
```

## 6. Standard Library

### 6.1 Structure

```
std
├── prelude          # auto-imported
├── core             # fundamental types
│   ├── maybe
│   ├── outcome
│   └── primitives
├── collections
│   ├── list
│   ├── map
│   ├── set
│   └── vec
├── string
├── io
│   ├── file
│   ├── net
│   └── stdio
├── sync
│   ├── mutex
│   ├── rwlock
│   └── channel
├── time
├── math
├── fmt
├── iter
└── mem
```

### 6.2 Prelude

Auto-imported in every module:

```tml
// Implicit equivalent:
import std.prelude.*

// Includes:
// - Maybe, Just, Nothing
// - Outcome, Success, Failure
// - Bool, true, false
// - String, List, Map
// - print, panic, assert
// - Common behaviors: Equal, Ordered, Duplicate, Debug
```

## 7. Conditional Compilation

### 7.1 @when Directive

```tml
@when(target: wasm32)
func wasm_specific() {
    // only compiled for wasm
}

@when(feature: async)
func async_version() {
    // only if feature "async" is active
}

@when(debug)
func debug_only() {
    // only in debug builds
}
```

### 7.2 @when in Module

```tml
@when(target: windows)
module platform {
    import win32.*
    // ...
}

@when(target: linux)
module platform {
    import posix.*
    // ...
}
```

## 8. Build Scripts

### 8.1 build.tml

```tml
// build.tml - executed before compilation

func main() {
    // Generate code
    let proto: String = read("schema.proto")
    let generated: String = compile_proto(proto)
    write("src/generated.tml", generated)

    // Configure paths
    println("cargo:include=/usr/local/include")
}
```

## 9. Documentation

### 9.1 Doc Comments

```tml
/// Represents a 2D point.
///
/// ## Example
/// ```
/// let p: Point = Point.new(1.0, 2.0)
/// assert_eq(p.x, 1.0)
/// ```
public type Point {
    /// X coordinate
    x: F64,
    /// Y coordinate
    y: F64,
}

/// Calculates the distance between two points.
///
/// ## Arguments
/// * `other` - The other point
///
/// ## Returns
/// The Euclidean distance
public func distance(this, other: Point) -> F64 {
    // ...
}
```

### 9.2 Module Docs

```tml
//! # Geometry Module
//!
//! This module provides types and functions for
//! 2D geometric operations.
//!
//! ## Features
//! - Points and vectors
//! - Transformations
//! - Intersections

module geometry
```

### 9.3 Generate Docs

```bash
tml doc --open
tml doc --format json --output docs/
```

## 10. Tests

### 10.1 Inline Tests

```tml
module math

public func add(a: I32, b: I32) -> I32 {
    return a + b
}

@test
func test_add() {
    assert_eq(add(2, 3), 5)
    assert_eq(add(-1, 1), 0)
}

@test(should_fail)
func test_overflow() {
    add(I32.MAX, 1)  // should panic in debug
}
```

### 10.2 Test Module

```tml
// tests/test_math.tml

import mylib.math.*

@test
func test_complex_calculation() {
    let result: I32 = complex_math(42)
    assert(result > 0)
}
```

### 10.3 Run Tests

```bash
tml test
tml test --filter "test_add*"
tml test --module math
```

---

*Previous: [06-MEMORY.md](./06-MEMORY.md)*
*Next: [08-IR.md](./08-IR.md) — Intermediate Representation*
