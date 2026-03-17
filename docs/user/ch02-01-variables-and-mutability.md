# Variables and Mutability

TML separates immutable bindings from mutable ones at the declaration site. This is not a
restriction — it is a signal. When you read `let x`, you know `x` will never change. When you
read `var x`, you know it might.

## Immutable Bindings: `let`

Use `let` to introduce a binding that cannot be reassigned:

```tml
func main() {
    let x: I32 = 42
    println(x.to_string())  // 42
}
```

Attempting to reassign a `let` binding is a compile error:

```tml
func main() {
    let x: I32 = 42
    x = 100  // Error: cannot assign to immutable binding `x`
}
```

Immutability is the default in TML. Prefer `let` and switch to `var` only when the value
genuinely needs to change.

## Mutable Bindings: `var`

Use `var` when the value must change over the course of the program:

```tml
func main() {
    var count: I32 = 0
    count = count + 1
    count = count + 1
    println(count.to_string())  // 2
}
```

The `var` keyword makes mutability visible at the declaration. Anyone reading the function knows
`count` may be modified.

## Type Annotations

Both `let` and `var` support explicit type annotations with a colon after the name:

```tml
let name: Str    = "Alice"
let age:  I32    = 30
let ratio: F64   = 0.75
var active: Bool = true
```

TML can infer the type from the value in most cases, so annotations are optional when the type
is unambiguous:

```tml
let x    = 42        // inferred as I32
let name = "Alice"   // inferred as Str
let pi   = 3.14159   // inferred as F64
var flag = false     // inferred as Bool
```

Add annotations when inference produces the wrong type, when the intent is not obvious from the
value, or when a specific type is required and the literal is ambiguous:

```tml
let big: I64 = 1_000_000  // without annotation, inferred as I32
let half: F32 = 0.5       // without annotation, inferred as F64
```

## Constants: `const`

Use `const` for values that are fixed at compile time and do not change:

```tml
const MAX_CONNECTIONS: I32 = 100
const PI: F64 = 3.14159265358979
const GREETING: Str = "Hello"
```

Constants differ from `let` bindings in several ways:

- The type annotation is required.
- The value must be a compile-time constant expression — function calls and runtime values are
  not allowed.
- Constants are always immutable; there is no `var const`.
- Constants can be declared at module scope, outside any function.
- By convention, constant names use `SCREAMING_SNAKE_CASE`.

```tml
const BUFFER_SIZE: I32 = 4096

func main() {
    println(BUFFER_SIZE.to_string())  // 4096
}
```

Constants are available throughout the module from the point of declaration. They are not
stack-allocated — the compiler substitutes the value at each use.

## Shadowing

You can declare a new binding with the same name as an existing one. The new binding *shadows*
the previous one within the same scope:

```tml
func main() {
    let x: I32 = 5
    let x: I32 = x + 1   // x is now 6
    let x: I32 = x * 2   // x is now 12
    println(x.to_string())  // 12
}
```

Shadowing is not mutation. Each `let x` creates a new binding that happens to reuse the name.
This distinction matters because shadowing allows a type change:

```tml
func main() {
    let input: Str = "  42  "
    let input: I32 = 42     // same name, different type — this is valid
    println(input.to_string())  // 42
}
```

Trying the same with `var` would be a type mismatch error, because `var` reassigns the existing
binding rather than creating a new one.

Shadowing is most useful for transforming a value through multiple steps without inventing a new
name at each step. Use it judiciously — shadowing a binding that is still in active use in the
surrounding code makes the program harder to read.

## Variable Naming

Variable names in TML:

- Must start with a letter (`a`–`z`, `A`–`Z`) or an underscore (`_`)
- Can contain letters, digits, and underscores after the first character
- Are case-sensitive (`count` and `Count` are different names)
- Should use `snake_case` by convention

```tml
let user_name: Str   = "Alice"
let age_in_years: I32 = 30
let _unused: Bool    = true   // leading underscore suppresses unused-variable warnings
```

Type names and enum variants use `PascalCase`. Constants use `SCREAMING_SNAKE_CASE`. The
compiler does not enforce these conventions, but the standard library follows them uniformly and
tooling may warn when code deviates.

## `let` vs `var`: When to Use Each

| Situation | Use |
|-----------|-----|
| Value assigned once and read many times | `let` |
| Loop counter or accumulator | `var` |
| Building a value incrementally | `var` |
| Configuration or default values | `let` or `const` |
| Compile-time known values shared across functions | `const` |

The goal is to express intent. If a value should not change, say so with `let`. Reserve `var`
for the places where change is part of the algorithm.
