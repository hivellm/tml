# Use Declarations and Visibility

The `use` declaration imports symbols from another module into the current scope. Visibility — controlled by `pub` — determines which symbols are importable at all.

## Basic `use` Syntax

The simplest form imports a module by its path and makes its symbols accessible with the module name as a prefix:

```tml
use std::collections::List

func main() -> I32 {
    let nums: List[I32] = List[I32].new(8)
    nums.push(1)
    nums.push(2)
    nums.destroy()
    return 0
}
```

After `use std::collections::List`, you refer to the type as `List`, not as `std::collections::List`. The `use` declaration brings the last segment of the path into scope.

## Importing Multiple Symbols

To import several symbols from the same module path, list them in braces:

```tml
use std::collections::{HashMap, HashSet, List}
use core::fmt::{Display, Debug}
```

This is equivalent to three separate `use` declarations but more concise.

## Wildcard Imports

A `*` wildcard imports all public symbols from a module:

```tml
use std::collections::*
```

After this, `List`, `HashMap`, `HashSet`, and all other public names from `std::collections` are in scope without a prefix.

Wildcard imports are convenient in test files and short scripts. In library code, prefer explicit imports — they make it clear where each name comes from and prevent silent name collisions when the imported module adds new exports.

## Aliased Imports

Give an imported symbol a different local name with `as`:

```tml
use std::io as io_lib
use std::collections::HashMap as Map
use core::fmt::Display as Displayable

func process() -> I32 {
    let m: Map[Str, I32] = Map[Str, I32].new(8)
    m.set("key", 1)
    m.destroy()
    return 0
}
```

Aliases are useful when two imports would otherwise produce the same local name, or when a standard name conflicts with a local identifier.

## Calling Symbols with Module Prefixes

When you import a module without bringing its contents into scope, you use the module name as a qualifier:

```tml
use math

func main() -> I32 {
    let result = math::add(3, 4)   // qualified access
    return 0
}
```

When you import specific symbols directly, no qualifier is needed:

```tml
use math::{add, multiply}

func main() -> I32 {
    let result = add(3, 4)         // unqualified access
    return 0
}
```

Both styles are valid. Qualified access (`module::symbol`) makes the origin of each name explicit and is preferred in larger files. Unqualified access reduces visual noise for symbols used frequently.

## The `pub` Modifier

By default, all symbols in a module are private — they are inaccessible to other modules. The `pub` modifier makes a symbol part of the module's public interface:

```tml
mod analytics

pub type Report {
    title: Str,
    row_count: I64,
}

pub func generate(title: Str) -> Report {
    return Report { title: title, row_count: 0 }
}

// Private: not exported, not visible to importers
func internal_validate(r: ref Report) -> Bool {
    return r.row_count >= 0
}
```

Other modules that import `analytics` can use `Report` and `generate`, but not `internal_validate`.

### What Can Be `pub`

The `pub` modifier applies to:

- `func` — public functions
- `type` — public struct types
- `type = ...` — public enum types
- `behavior` — public behavior declarations
- Constants defined with `let` at module scope

```tml
pub func compute(x: I32) -> I32 { ... }
pub type Config { host: Str, port: I32 }
pub type Status = Active | Inactive | Pending
pub behavior Serialize { ... }
```

## Re-Exports

A module can re-export symbols from another module, making them part of its own public interface:

```tml
mod http

// Re-export from submodules so callers only need `use http`
pub use http::client::Client
pub use http::server::Server
pub use http::headers::Headers
```

Callers now write `use http` and get access to `Client`, `Server`, and `Headers` without knowing about the internal submodule structure. This is the standard pattern for building a module facade over a multi-file implementation.

Wildcard re-exports work the same way:

```tml
pub use http::headers::*   // re-export everything public from headers
```

## Importing Local Modules

When importing your own modules (files in the same project), the path is relative to the project root or the importing file's directory:

```tml
// Import math.tml from the same directory
use math

// Import a nested module
use db::query
use db::schema
```

The compiler resolves local imports before checking the standard library, so a local `math.tml` takes precedence over any standard library module with the same name.

## Standard Library Paths

The standard library is organized under two roots:

| Root | Contents |
|---|---|
| `core` | `alloc`, `str`, `fmt`, `cmp`, `iter`, `slice`, `hash`, `error`, `mem`, `num`, `ops` |
| `std` | `collections`, `io`, `file`, `json`, `crypto`, `net`, `sync`, `thread`, `time`, `os` |

Common imports:

```tml
use std::collections::List
use std::collections::{HashMap, HashSet}
use std::io::File
use std::json::{JsonValue, parse, stringify}
use std::time::Instant
use std::sync::{Mutex, Arc}
use core::fmt::Display
```

## Module Resolution Order

When the compiler resolves `use foo::bar`, it searches in this order:

1. **Same-directory file**: `bar.tml` in the same directory as the importing file
2. **Project files with matching `mod` declarations**: any `.tml` file declaring `mod foo::bar`
3. **Standard library**: `core::bar` or `std::bar` as appropriate

The first match wins. If you have a local file that shadows a standard library module, the compiler will use the local file. This is rarely what you want — avoid naming local modules `core`, `std`, or any other standard library name.

## Visibility in Practice

A well-designed module exposes a minimal, stable public API and keeps its implementation details private. This minimizes the surface area that callers depend on, making it easier to change the implementation later without breaking anything.

A useful discipline: start everything private, and promote to `pub` only when another module actually needs it. The compiler will tell you when a private symbol is referenced from outside its module.

```tml
mod user_service

// Public: callers need to create and inspect users
pub type User {
    id: I64,
    name: Str,
    email: Str,
}

pub func find_by_id(id: I64) -> Maybe[User] {
    return fetch_from_db(id)
}

pub func create(name: Str, email: Str) -> Outcome[User, Str] {
    if not validate_email(email) {
        return Err("invalid email address")
    }
    let u = insert_into_db(name, email)
    return Ok(u)
}

// Private: internal implementation detail
func validate_email(email: Str) -> Bool {
    return email.contains("@") and email.contains(".")
}

func fetch_from_db(id: I64) -> Maybe[User] { ... }
func insert_into_db(name: Str, email: Str) -> User { ... }
```

Callers see `User`, `find_by_id`, and `create`. The database access functions and validation logic remain encapsulated.
