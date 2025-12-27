# 26. Canonical Formatter

The TML formatter produces deterministic, canonical output. Every valid TML program has exactly one formatted form.

## 1. Design Goals

1. **Deterministic** - Same AST always produces identical output
2. **Idempotent** - `format(format(code)) == format(code)`
3. **Minimal diff** - Small changes produce small diffs
4. **Readable** - Output follows consistent style
5. **Fast** - O(n) complexity, suitable for format-on-save

## 2. Indentation

### 2.1 Rules

- Indent with **4 spaces** (no tabs)
- Increase indent after `{`, `[`, `(`
- Decrease indent before `}`, `]`, `)`
- Continuation lines indent 4 additional spaces

```tml
func example() {
    let x: T = long_function_call(
        arg1,
        arg2,
        arg3,
    )
}
```

### 2.2 No Alignment

Never align code by content. Alignment creates noisy diffs.

```tml
// WRONG - alignment
let short    = 1
let very_long: I32 = 2

// RIGHT - no alignment
let short: I32 = 1
let very_long: I32 = 2
```

## 3. Line Length

### 3.1 Target: 100 characters

- Soft limit at 100 characters
- Break before exceeding when possible
- Never break string literals (allow overflow)
- Never break in middle of tokens

### 3.2 Breaking Priority

When a line is too long, break in this order:

1. After `{` in blocks
2. After `,` in argument lists
3. After binary operators
4. After `->` in function types
5. Before `.` in method chains

```tml
// Long argument list
let result: T = very_long_function_name(
    first_argument,
    second_argument,
    third_argument,
)

// Long method chain
let processed: T = data
    .filter(do(x) x > 0)
    .map(do(x) x * 2)
    .collect()

// Long binary expression
let value: T = first_operand
    + second_operand
    + third_operand
```

## 4. Spacing

### 4.1 Binary Operators

Space around all binary operators:

```tml
// Arithmetic
x + y
x - y
x * y
x / y
x % y
x ** y

// Comparison
x == y
x != y
x < y
x <= y
x > y
x >= y

// Logical
x and y
x or y

// Bitwise
x & y
x | y
x ^ y
x << y
x >> y

// Assignment
x = y
x += y
```

### 4.2 Unary Operators

No space after unary operators:

```tml
-x
not x
~x
ref x
*x
```

### 4.3 Colons

Space after, not before:

```tml
let x: I32 = 5
func foo(a: I32, b: I32) -> I32
type Point = { x: F64, y: F64 }
```

### 4.4 Commas

Space after, not before:

```tml
foo(a, b, c)
[1, 2, 3]
(x, y, z)
```

### 4.5 Semicolons

No space before:

```tml
let x: I32 = 5;
use std::io;
```

### 4.6 Arrows

Space around `->`:

```tml
func foo() -> I32
do(x) -> I32 { x * 2 }
Ok(x) -> process(x),
```

### 4.7 Path Separator

No space around `::`:

```tml
std::collections::Map
crate::utils::helper
```

### 4.8 Generic Brackets

No space inside `[]`:

```tml
List[I32]
Map[String, Value]
func foo[T: Ord](x: T)
```

### 4.9 Parentheses

No space inside `()`:

```tml
foo(a, b)
(x, y, z)
if (condition) then
```

### 4.10 Braces

Space inside for single-line, newline for multi-line:

```tml
// Single expression
Point { x: 0, y: 0 }

// Multiple fields
Point {
    x: 0,
    y: 0,
    z: 0,
}
```

## 5. Blank Lines

### 5.1 Between Items

One blank line between top-level items:

```tml
func foo() { }

func bar() { }

type Point = { x: F64, y: F64 }
```

### 5.2 Inside Functions

One blank line to separate logical sections:

```tml
func process(data: Data) -> Outcome[Result, Error] {
    // Validation
    validate(data)!

    // Processing
    let step1 = transform(data)
    let step2 = analyze(step1)

    // Result
    Ok(step2)
}
```

### 5.3 Maximum Blank Lines

Never more than one consecutive blank line.

### 5.4 No Trailing Blank Lines

Files end with single newline, no trailing blanks.

## 6. Braces

### 6.1 Same Line Opening

Opening brace on same line:

```tml
func foo() {
    // body
}

if condition then {
    // body
}

when value {
    // arms
}
```

### 6.2 Closing Brace Alone

Closing brace on its own line:

```tml
func foo() {
    // body
}
```

### 6.3 Empty Blocks

Empty blocks on same line:

```tml
func empty() { }
impl Foo for Bar { }
```

## 7. Trailing Commas

### 7.1 Multi-line: Always

```tml
let point: Point = Point {
    x: 0,
    y: 0,  // trailing comma
}

foo(
    arg1,
    arg2,  // trailing comma
)
```

### 7.2 Single-line: Never

```tml
let point: Point = Point { x: 0, y: 0 }
foo(arg1, arg2)
```

## 8. Imports

### 8.1 Ordering

1. `crate::` imports first
2. External crate imports
3. `std::` imports last
4. Alphabetical within each group
5. Blank line between groups

```tml
use crate::config::Config
use crate::utils::helper

use serde::Serialize
use tokio::runtime

use std::collections::Map
use std::io::File
```

### 8.2 Grouping

Prefer grouped imports when 3+ items from same path:

```tml
// Prefer
use std::collections::{Map, Set, Vec}

// Avoid
use std::collections::Map
use std::collections::Set
use std::collections::Vec
```

### 8.3 Line Breaking

Break after `{` if too long:

```tml
use std::collections::{
    BTreeMap,
    BTreeSet,
    HashMap,
    HashSet,
    LinkedList,
}
```

## 9. Function Definitions

### 9.1 Short Functions

Single line if fits:

```tml
func add(a: I32, b: I32) -> I32 { a + b }
```

### 9.2 Long Signatures

Break parameters:

```tml
func process_data(
    input: ref Data,
    config: Config,
    options: Options,
) -> Outcome[Result, Error] {
    // body
}
```

### 9.3 Generic Constraints

On same line if short, otherwise break:

```tml
// Short
func foo[T: Ord](x: T) -> T

// Long
func complex[
    T: Serialize + Deserialize,
    E: Error + Display,
](
    value: T,
) -> Outcome[T, E]
```

## 10. Type Definitions

### 10.1 Short Structs

Single line if fits:

```tml
type Point = { x: F64, y: F64 }
```

### 10.2 Long Structs

One field per line:

```tml
type User = {
    id: U64,
    name: String,
    email: String,
    created_at: DateTime,
}
```

### 10.3 Sum Types

One variant per line:

```tml
type Outcome[T, E] =
    | Ok(T)
    | Err(E)
```

Or inline if short:

```tml
type Bool = True | False
```

## 11. Expressions

### 11.1 Method Chains

Break before `.` when long:

```tml
let result: T = data
    .iter()
    .filter(do(x) x.is_valid())
    .map(do(x) x.transform())
    .collect()
```

### 11.2 Binary Expressions

Break after operator:

```tml
let total: I32 = first_value +
    second_value +
    third_value
```

### 11.3 Match Arms

Each arm on its own line:

```tml
when value {
    Ok(x) -> process(x),
    Err(e) -> handle_error(e),
}
```

Short arms can stay on one line:

```tml
when value {
    Just(x) -> x,
    Nothing -> default,
}
```

### 11.4 If Expressions

```tml
// Short
if condition then value1 else value2

// Long condition
if some_long_condition and another_condition then {
    result1
} else {
    result2
}
```

## 12. Comments

### 12.1 Line Comments

Space after `//`:

```tml
// This is a comment
let x: I32 = 5  // inline comment
```

### 12.2 Doc Comments

Use `///` for documentation:

```tml
/// Calculates the factorial of n.
///
/// # Examples
///
/// ```
/// assert_eq(factorial(5), 120)
/// ```
func factorial(n: U64) -> U64 {
    if n <= 1 then 1 else n * factorial(n - 1)
}
```

### 12.3 Block Comments

Avoid when possible, use line comments instead.

## 13. Decorators

### 13.1 Single Decorator

On same line if short:

```tml
@inline func add(a: I32, b: I32) -> I32 { a + b }
```

### 13.2 Multiple Decorators

Each on its own line:

```tml
@test
@timeout(5000)
func test_example() {
    // test body
}
```

### 13.3 Decorator Arguments

Break if long:

```tml
@route(
    method: HttpMethod.Post,
    path: "/api/users/{id}/profile",
    auth: AuthLevel.Required,
)
func update_profile(req: Request) -> Response {
    // handler body
}
```

## 14. Special Cases

### 14.1 Preserve User Formatting

**Never** for semantic code. Formatter is authoritative.

### 14.2 String Literals

Do not break or reformat:

```tml
let long_string: String = "This is a very long string that exceeds the line limit but we do not break it"
```

### 14.3 Raw Strings

Preserve internal formatting:

```tml
let sql: String = """
    SELECT *
    FROM users
    WHERE id = ?
"""
```

## 15. Implementation

### 15.1 Algorithm

```
1. Parse source to AST
2. Walk AST with formatting context
3. For each node:
   a. Emit opening tokens
   b. Recursively format children
   c. Emit closing tokens
4. Apply line breaking where needed
5. Emit final string
```

### 15.2 Context

Formatter maintains:
- Current indentation level
- Current column position
- Whether in "single-line mode"
- Parent node type

### 15.3 Line Breaking Strategy

1. Try to emit node on single line
2. If exceeds limit, switch to multi-line mode
3. Re-emit with line breaks

## 16. CLI

```bash
# Format file in place
tml fmt file.tml

# Format all files
tml fmt .

# Check formatting (exit 1 if unformatted)
tml fmt --check file.tml

# Show diff
tml fmt --diff file.tml

# Read from stdin
echo "func foo(){}" | tml fmt -

# Custom line width
tml fmt --line-width 80 file.tml
```

## 17. Editor Integration

### 17.1 Format on Save

Editors should format on save by default.

### 17.2 Range Formatting

Format selection only (for partial edits).

### 17.3 Format on Type

Format closing braces, semicolons.
