# TML v1.0 — Module System

## 1. Structure

### 1.1 Module Organization

TML infers module names from file paths. No explicit `module` declaration is needed.

```tml
// src/lib.tml - module name inferred as "mylib"
pub func hello() {
    print("Hello from mylib!")
}
```

```tml
// src/utils.tml - module name inferred as "mylib::utils"
pub func helper() {
    // ...
}
```

### 1.2 File Hierarchy

```
myproject/
├── tml.toml
├── src/
│   ├── lib.tml          # myproject (root module)
│   ├── utils.tml        # myproject::utils
│   ├── http/
│   │   ├── mod.tml      # myproject::http
│   │   ├── client.tml   # myproject::http::client
│   │   └── server.tml   # myproject::http::server
│   └── main.tml         # main (executable entry)
└── tests/
    └── test_utils.tml
```

## 2. Imports

### 2.1 Simple Import

```tml
use std::io
use std::collections::List

// Usage
io::print("hello")
let list: List[T] = List::new()
```

### 2.2 Import with Alias

```tml
use std::collections::HashMap as Map
use very::long::module::name as short

let m: Map[String, I32] = Map::new()
```

### 2.3 Multiple Import

```tml
use std::collections::{List, Map, Set}
use std::io::{read, write, File}
```

### 2.4 Glob Import

```tml
use std::prelude::*

// Brings all public items
// Use sparingly
```

### 2.5 Re-export

```tml
// lib.tml
pub use internal::Parser
pub use internal::Lexer

// Users do: use mylib::Parser
```

## 3. Visibility

### 3.1 pub vs private (default)

```tml
pub type User {
    pub name: String,      // public field
    password: String,      // private field (default)
}

pub func create_user(name: String) -> User {
    return User {
        name: name,
        password: generate_password(),
    }
}

func generate_password() -> String {
    // not visible outside module (private by default)
    return random_string(16)
}
```

### 3.2 Visibility Rules

| Declaration | Visible in |
|-------------|------------|
| (default) | Same module only |
| `pub` | Anywhere |

### 3.3 Opaque Types

```tml
// Public type, private fields
pub type Handle {
    id: U64,           // private by default
    data: ptr Unit,    // private by default
}

// Only module functions can create/access
pub func create() -> Handle {
    return Handle { id: next_id(), data: alloc() }
}
```

## 4. FFI Module Integration

### 4.1 FFI Namespace from @link

When you declare FFI functions with `@link`, the library name becomes a module namespace:

```tml
@link("user32")
@extern("stdcall")
func MessageBoxA(hwnd: I32, text: Str, caption: Str, utype: I32) -> I32

@link("kernel32")
@extern("stdcall")
func GetTickCount64() -> U64

func main() -> I32 {
    // Qualified calls using library namespace
    user32::MessageBoxA(0, "Hello", "TML", 0)
    let ticks: U64 = kernel32::GetTickCount64()
    return 0
}
```

### 4.2 Library Name Extraction

The namespace is extracted from the `@link` path:

| @link value | Namespace |
|-------------|-----------|
| `"SDL2"` | `SDL2` |
| `"SDL2.dll"` | `SDL2` |
| `"libSDL2.so"` | `SDL2` |
| `"./vendor/mylib.dll"` | `mylib` |
| `"user32"` | `user32` |

### 4.3 Disambiguating Same-Name Functions

```tml
// Two libraries with same function name
@link("foo")
@extern("c")
func init() -> I32

@link("bar")
@extern("c")
func init() -> I32

func main() -> I32 {
    let a: I32 = foo::init()   // Calls foo library
    let b: I32 = bar::init()   // Calls bar library
    return 0
}
```

### 4.4 Unified Namespace

Both TML modules and FFI libraries use the same `::` syntax:

```tml
// Internal TML modules
use std::math::{abs, sqrt}
let x: I32 = std::math::abs(-5)

// FFI external libraries
let result: I32 = SDL2::SDL_Init(0)
let msg: I32 = user32::MessageBoxA(0, "Hi", "TML", 0)
```

## 5. Packages

### 5.1 tml.toml

```toml
[package]
name = "myproject"
version = "1.0.0"
edition = "2024"
authors = ["Alice <alice@example.com>"]
license = "Apache-2.0"
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

### 5.2 Lock File

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

## 6. Workspaces

### 6.1 Structure

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

### 6.2 Workspace tml.toml

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

### 6.3 Member tml.toml

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

## 7. Standard Library

### 7.1 Structure

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

### 7.2 Prelude

Auto-imported in every module:

```tml
// Implicit equivalent:
use std::prelude::*

// Includes:
// - Maybe, Just, Nothing
// - Outcome, Ok, Err
// - Bool, true, false
// - String, List, Map
// - print, panic, assert
// - Common behaviors: Equal, Ordered, Duplicate, Debug
```

## 8. Conditional Compilation

### 8.1 @when Directive

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

### 8.2 @when in Module

```tml
@when(target: windows)
use win32::*
// Windows-specific code...

@when(target: linux)
use posix::*
// Linux-specific code...
```

## 9. Build Scripts

### 9.1 build.tml

```tml
// build.tml - executed before compilation

func main() -> I32 {
    // Generate code
    let proto: String = read("schema.proto")
    let generated: String = compile_proto(proto)
    write("src/generated.tml", generated)

    // Configure paths
    println("cargo:include=/usr/local/include")
    return 0
}
```

## 10. Documentation

### 10.1 Doc Comments

```tml
/// Represents a 2D point.
///
/// ## Example
/// ```
/// let p: Point = Point::new(1.0, 2.0)
/// assert_eq(p.x, 1.0)
/// ```
pub type Point {
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
pub func distance(this, other: Point) -> F64 {
    // ...
}
```

### 10.2 Module Docs

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
```

### 10.3 Generate Docs

```bash
tml doc --open
tml doc --format=json --output=docs/
```

## 11. Tests

### 11.1 Inline Tests

```tml
pub func add(a: I32, b: I32) -> I32 {
    return a + b
}

@test
func test_add() {
    assert_eq(add(2, 3), 5)
    assert_eq(add(-1, 1), 0)
}

@test(should_fail)
func test_overflow() {
    add(I32::MAX, 1)  // should panic in debug
}
```

### 11.2 Test Module

```tml
// tests/test_math.tml

use mylib::math::*

@test
func test_complex_calculation() {
    let result: I32 = complex_math(42)
    assert(result > 0)
}
```

### 11.3 Run Tests

```bash
tml test
tml test --filter="test_add*"
tml test --module=math
```

---

*Previous: [06-MEMORY.md](./06-MEMORY.md)*
*Next: [08-IR.md](./08-IR.md) — Intermediate Representation*
