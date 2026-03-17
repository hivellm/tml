# Defining Modules

A module in TML is a source file. You can declare the module's name explicitly with a `mod` statement, or let the compiler derive it from the filename. Public symbols are prefixed with `pub`; everything else is private to the file.

## File-Based Modules

The simplest module is a single file. If you place code in `math.tml`, that file defines a module named `math`. Other files in the same directory can import it with `use math`.

**math.tml**:
```tml
// Public functions are accessible from other modules
pub func add(a: I32, b: I32) -> I32 {
    return a + b
}

pub func subtract(a: I32, b: I32) -> I32 {
    return a - b
}

pub func multiply(a: I32, b: I32) -> I32 {
    return a * b
}

// Private helper — not accessible outside this file
func clamp_positive(n: I32) -> I32 {
    if n < 0 { return 0 }
    return n
}

pub func abs_diff(a: I32, b: I32) -> I32 {
    return clamp_positive(a - b)
}
```

**main.tml**:
```tml
use math

func main() -> I32 {
    let sum = math::add(10, 5)       // 15
    let diff = math::abs_diff(3, 8)  // 5

    println(sum.to_string())
    println(diff.to_string())
    return 0
}
```

## The `mod` Declaration

You can explicitly name a module with a `mod` statement at the top of the file:

```tml
mod http::client
```

This tells the compiler that this file belongs to the `http::client` module path. Other modules import it as:

```tml
use http::client
```

Without a `mod` declaration, the compiler uses the filename as the module name.

## Nested Module Paths

Module paths use `::` as a separator. A file at `src/http/client.tml` that declares `mod http::client` becomes part of a two-level hierarchy. You can nest as deeply as your project requires:

```
src/
├── main.tml
├── http/
│   ├── client.tml    (mod http::client)
│   ├── server.tml    (mod http::server)
│   └── headers.tml   (mod http::headers)
└── db/
    ├── query.tml     (mod db::query)
    └── schema.tml    (mod db::schema)
```

Consumers of this structure write:

```tml
use http::client
use http::server
use db::query
```

## Public vs. Private Symbols

By default, every symbol in a module — functions, types, constants — is private. Private symbols are inaccessible to other modules. Marking a symbol `pub` makes it part of the module's public interface:

```tml
mod geometry

// Public type: other modules can name and construct this
pub type Point {
    x: F64,
    y: F64,
}

// Public function: callable from other modules
pub func distance(a: ref Point, b: ref Point) -> F64 {
    let dx = a.x - b.x
    let dy = a.y - b.y
    return (dx * dx + dy * dy).sqrt()
}

// Private type: only used inside this file
type Segment {
    start: Point,
    end: Point,
}

// Private function: internal helper
func midpoint(s: ref Segment) -> Point {
    return Point {
        x: (s.start.x + s.end.x) / 2.0,
        y: (s.start.y + s.end.y) / 2.0,
    }
}
```

Attempting to use `Segment` or `midpoint` from another module produces a compile error.

## Organizing a Project

A well-organized TML project assigns one clear responsibility to each module:

```
project/
├── main.tml           // entry point, wires modules together
├── config.tml         // configuration loading
├── parser.tml         // input parsing
├── validator.tml      // business rule validation
├── formatter.tml      // output formatting
└── utils.tml          // shared helpers used by multiple modules
```

Each module imports only what it needs and exposes only what callers require. The `main.tml` file imports and orchestrates the others:

```tml
use config
use parser
use validator
use formatter

func main() -> I32 {
    let cfg = config::load("app.toml")
    let input = parser::parse_stdin()
    let valid = validator::check(ref input, ref cfg)
    if not valid {
        return 1
    }
    formatter::print_result(ref input)
    return 0
}
```

## Module Resolution Order

When you write `use foo`, the compiler searches for the module in this order:

1. **Current directory**: `foo.tml` in the same directory as the importing file
2. **Sibling files with mod declarations**: any `.tml` file in the project that declares `mod foo`
3. **Standard library**: the `core` and `std` libraries

The first match wins. If you have a local `math.tml`, it takes precedence over any standard library module named `math`.

For hierarchical paths like `use http::client`, the compiler resolves `http` first, then looks for `client` within that module's directory.

## Documenting Public Symbols

Add a comment immediately above each public function or type to document its purpose and any important constraints. These comments appear in generated documentation:

```tml
// Returns the factorial of n.
// Panics if n is negative.
// Note: overflows for n > 20 with I64.
pub func factorial(n: I64) -> I64 {
    if n < 0 { panic("factorial requires non-negative input") }
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
```

Keep private helpers uncommented or with brief implementation notes — they are not part of the public contract and do not appear in generated documentation.
