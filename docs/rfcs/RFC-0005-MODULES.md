# RFC-0005: Modules and Capabilities

## Status
Partial (v0.3.0)

> Basic `use` declarations implemented. Visibility rules and capability system pending.

## Summary

This RFC defines TML's module system, visibility rules, imports, and capability-based access control for effects.

## Motivation

The module system must:
1. **Organize** code into logical units
2. **Encapsulate** implementation details
3. **Control** access to sensitive operations
4. **Enable** separate compilation and caching

---

## 1. Module Structure

### 1.1 File = Module

Each `.tml` file is a module. The module name comes from the file path:

```
src/
├── main.tml          → mod main
├── utils.tml         → mod utils
├── http/
│   ├── mod.tml       → mod http
│   ├── client.tml    → mod http::client
│   └── server.tml    → mod http::server
└── db/
    ├── mod.tml       → mod db
    └── query.tml     → mod db::query
```

### 1.2 Inline Modules

Modules can be declared inline:

```tml
mod helpers {
    func internal_helper() -> I32 { 42 }

    pub func public_helper() -> I32 {
        internal_helper()
    }
}
```

### 1.3 Module Declaration

Reference external module files:

```tml
// In main.tml
mod utils;      // Loads ./utils.tml or ./utils/mod.tml
mod http;       // Loads ./http/mod.tml

use utils::format_date;
use http::client::Client;
```

---

## 2. Visibility

### 2.1 Visibility Modifiers

| Modifier | Visibility |
|----------|------------|
| (none) | Private to current module |
| `pub` | Public to all |
| `pub(crate)` | Public within crate |
| `pub(super)` | Public to parent module |
| `pub(self)` | Same as private (explicit) |
| `pub(in path)` | Public to specified path |

### 2.2 Default Visibility

Items are **private by default**:

```tml
func private_fn() { }          // Only visible in this module
pub func public_fn() { }       // Visible everywhere
pub(crate) func crate_fn() { } // Visible in crate
```

### 2.3 Struct Field Visibility

```tml
pub type User = {
    pub name: String,       // Public field
    pub(crate) id: U64,     // Crate-visible field
    password_hash: String,  // Private field
}

// Constructor needed for private fields
impl User {
    pub func new(name: String, password: String) -> This {
        This {
            name,
            id: generate_id(),
            password_hash: hash(password),
        }
    }
}
```

---

## 3. Imports

### 3.1 Use Declarations

```tml
// Import single item
use std::fs::File;

// Import multiple items
use std::fs::{File, read_dir, Metadata};

// Import all public items
use std::fs::*;

// Rename on import
use std::fs::File as FsFile;

// Import module itself
use std::fs;
// Then: fs::File, fs::read_dir, etc.
```

### 3.2 Path Types

| Path Start | Meaning |
|------------|---------|
| `crate::` | Root of current crate |
| `super::` | Parent module |
| `self::` | Current module |
| `std::` | Standard library |
| `name::` | External crate or sibling module |

```tml
use crate::utils::helpers;    // From crate root
use super::common::Config;    // From parent module
use self::internal::setup;    // From current module
use std::collections::Map;    // From stdlib
use serde::Serialize;         // From external crate
```

### 3.3 Re-exports

```tml
// In lib.tml
mod internal;

// Re-export for public use
pub use internal::PublicType;
pub use internal::public_function;

// Rename re-export
pub use internal::InternalName as PublicName;
```

---

## 4. Prelude

### 4.1 Automatic Imports

Every module automatically imports the prelude:

```tml
// Implicitly: use std::prelude::*;

// Which includes:
// - Outcome, Ok, Err
// - Maybe, Just, Nothing
// - Bool, true, false
// - String, Char
// - I8, I16, I32, I64, I128
// - U8, U16, U32, U64, U128
// - F32, F64
// - Vec, Map, Set
// - Heap, Shared, Sync
// - Copy, Duplicate, Default, Display, Debug
// - Ord, Eq, Hash
// - Iterator
```

### 4.2 No Prelude

Opt out with attribute:

```tml
@no_prelude
mod raw {
    // Must import everything explicitly
    use std::outcome::{Outcome, Ok, Err};
}
```

---

## 5. Capabilities

Capabilities control access to effectful operations.

### 5.1 Capability Types

```tml
// Capability to perform IO
cap Io {
    func read(path: String) -> Outcome[String, IoError]
    func write(path: String, data: String) -> Outcome[Unit, IoError]
}

// Capability to access network
cap Net {
    func connect(addr: String) -> Outcome[Connection, NetError]
    func listen(port: U16) -> Outcome[Listener, NetError]
}

// Capability to allocate memory
cap Alloc {
    func allocate(size: U64) -> *mut U8
    func deallocate(ptr: *mut U8, size: U64)
}
```

### 5.2 Capability Requirements

Functions declare required capabilities:

```tml
func read_config(io: cap Io) -> Outcome[Config, Error] {
    let content = io.read("config.json")!
    Json.parse(content)
}

func fetch_data(net: cap Net) -> Outcome[Data, Error> {
    let conn = net.connect("api.example.com:443")!
    conn.request(...)
}
```

### 5.3 Capability Provision

Capabilities are provided at program entry:

```tml
func main(io: cap Io, net: cap Net) {
    let config = read_config(io)!
    let data = fetch_data(net)!
    process(data)
}
```

### 5.4 Capability Restriction

Narrow capabilities before passing:

```tml
cap ReadOnly {
    func read(path: String) -> Outcome[String, IoError]
}

func process_with_read_only(io: cap Io) {
    // Restrict to read-only
    let ro: cap ReadOnly = io.restrict()
    analyze_files(ro)  // Cannot write
}
```

---

## 6. Effect Capabilities

Effects (RFC-0001) relate to capabilities:

### 6.1 Effect to Capability Mapping

| Effect | Capability |
|--------|------------|
| `io` | `cap Io` |
| `async` | `cap Async` |
| `alloc` | `cap Alloc` |
| `unsafe` | `cap Unsafe` |

### 6.2 Effect Inference from Capabilities

```tml
// Effect is inferred from capability usage
func example(io: cap Io) -> String {
    io.read("file.txt").unwrap()
}
// Inferred: with io
```

### 6.3 Capability-less Effects

Some effects don't need explicit capability:

```tml
// Pure functions (no capability needed)
func pure_calc(x: I32) -> I32 { x * 2 }

// Panic effect (always available)
func must_succeed() -> I32 with panic {
    panic!("failed")
}
```

---

## 7. Crates and Packages

### 7.1 Crate Structure

A crate is a compilation unit:

```
my_crate/
├── Cargo.tml           # Package manifest
├── src/
│   ├── lib.tml         # Library root (if library)
│   └── main.tml        # Binary root (if binary)
└── tests/
    └── integration.tml
```

### 7.2 Package Manifest

```toml
# Cargo.tml
[package]
name = "my_crate"
version = "0.1.0"
edition = "2024"

[dependencies]
serde = "1.0"
tokio = { version = "1.0", features = ["full"] }

[dev-dependencies]
test_utils = "0.5"
```

### 7.3 Workspace

```toml
# Cargo.tml (workspace root)
[workspace]
members = [
    "core",
    "cli",
    "web",
]
```

---

## 8. Conditional Compilation

### 8.1 Platform Detection

```tml
@when(target_os = "linux")
func linux_specific() { ... }

@when(target_os = "windows")
func windows_specific() { ... }

@when(target_arch = "x86_64")
mod simd_optimized { ... }
```

### 8.2 Feature Flags

```tml
@when(feature = "async")
mod async_impl { ... }

@when(not(feature = "std"))
mod no_std_impl { ... }

@when(all(feature = "serde", feature = "json"))
impl Serialize for MyType { ... }
```

### 8.3 Available Predicates

```tml
@when(target_os = "linux" | "macos" | "windows")
@when(target_arch = "x86_64" | "aarch64")
@when(target_pointer_width = "64")
@when(target_endian = "little")
@when(feature = "feature_name")
@when(not(...))
@when(all(...))
@when(any(...))
```

---

## 9. Examples

### 9.1 Library Structure

```
my_lib/
├── src/
│   ├── lib.tml         # Exports public API
│   ├── parser/
│   │   ├── mod.tml     # Parser module
│   │   ├── lexer.tml
│   │   └── ast.tml
│   └── codegen/
│       ├── mod.tml
│       └── llvm.tml
```

```tml
// src/lib.tml
pub mod parser;
pub mod codegen;

pub use parser::{parse, Ast};
pub use codegen::{generate, Output};
```

```tml
// src/parser/mod.tml
mod lexer;
mod ast;

pub use ast::{Ast, Node, Expr};

pub func parse(source: String) -> Outcome[Ast, ParseError] {
    let tokens = lexer::tokenize(source)!
    ast::build(tokens)
}
```

### 9.2 Capability Usage

```tml
// File operations with capability
func copy_file(io: cap Io, src: String, dst: String) -> Outcome[Unit, IoError] {
    let content = io.read(src)!
    io.write(dst, content)
}

// Main provides capabilities
func main(io: cap Io) {
    copy_file(io, "source.txt", "dest.txt").unwrap()
}
```

### 9.3 Sandboxed Execution

```tml
cap Sandbox {
    // Limited capability - only specific paths
    func read(path: String) -> Outcome[String, IoError]
        where path.starts_with("/sandbox/")
}

func run_untrusted(sandbox: cap Sandbox, code: String) {
    // Can only read from /sandbox/
    let data = sandbox.read("/sandbox/input.txt")!
    execute(code, data)
}
```

---

## 10. IR Representation

### 10.1 Module IR

```json
{
  "kind": "module",
  "name": "my_module",
  "path": "src/my_module.tml",
  "visibility": "pub",
  "imports": [
    { "path": ["std", "fs", "File"], "alias": null },
    { "path": ["super", "utils"], "alias": null }
  ],
  "items": [ ... ]
}
```

### 10.2 Capability IR

```json
{
  "kind": "func_def",
  "name": "read_config",
  "capabilities": [
    { "name": "io", "type": "Io" }
  ],
  "params": [],
  "return_type": { "name": "Outcome", "args": ["Config", "Error"] },
  "body": { ... }
}
```

---

## 11. Compatibility

- **RFC-0001**: Modules wrap core IR items
- **RFC-0002**: `mod`, `use`, `pub` are keywords
- **RFC-0003**: Contracts visible based on item visibility

---

## 12. Alternatives Rejected

### 12.1 Header Files (C-style)

```c
// Rejected
#include "header.h"
```

Problems:
- Textual inclusion is fragile
- Order-dependent
- Leads to include guards, forward declarations

### 12.2 Implicit Exports (Python-style)

```python
# Rejected: everything public by default
def my_function():
    pass
```

Problems:
- No encapsulation by default
- `_prefix` convention is weak
- Breaking changes too easy

### 12.3 Global Effects (No Capabilities)

```tml
// Rejected: effects without capability objects
func read_file(path: String) -> String with io {
    // Implicitly uses global IO
}
```

Problems:
- Hard to sandbox
- Hard to test (mocking)
- No fine-grained control

---

## 13. References

- [Rust Module System](https://doc.rust-lang.org/book/ch07-00-managing-growing-projects-with-packages-crates-and-modules.html)
- [Capability-Based Security](https://en.wikipedia.org/wiki/Capability-based_security)
- [Object Capabilities](http://erights.org/elib/capability/ode/ode-capabilities.html)
- [Pony Reference Capabilities](https://tutorial.ponylang.io/reference-capabilities/)
