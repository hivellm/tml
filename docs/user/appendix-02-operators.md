# Appendix B - Operators

This appendix lists every TML operator, grouped by category, followed by the
complete precedence table.

---

## Arithmetic Operators

| Operator | Name | Example | Result |
|----------|------|---------|--------|
| `+` | Addition | `3 + 4` | `7` |
| `-` | Subtraction | `10 - 3` | `7` |
| `*` | Multiplication | `3 * 4` | `12` |
| `/` | Division | `10 / 4` | `2` (integer truncation) |
| `%` | Remainder | `10 % 3` | `1` |
| `-` (unary) | Negation | `-x` | Negate `x` |

Division of two integers truncates toward zero. For floating-point division,
at least one operand must be a float type.

### Arithmetic Assignment

| Operator | Equivalent |
|----------|------------|
| `+=` | `x = x + rhs` |
| `-=` | `x = x - rhs` |
| `*=` | `x = x * rhs` |
| `/=` | `x = x / rhs` |
| `%=` | `x = x % rhs` |

---

## Comparison Operators

All comparison operators return `Bool`.

| Operator | Name | Example |
|----------|------|---------|
| `==` | Equal | `a == b` |
| `!=` | Not equal | `a != b` |
| `<` | Less than | `a < b` |
| `>` | Greater than | `a > b` |
| `<=` | Less than or equal | `a <= b` |
| `>=` | Greater than or equal | `a >= b` |

---

## Logical Operators

TML uses keywords for logical operations to avoid visual conflicts with
bitwise operators and to improve readability.

| Operator | Name | Short-circuits | Example |
|----------|------|---------------|---------|
| `and` | Logical AND | Yes — right side not evaluated if left is `false` | `x > 0 and x < 100` |
| `or` | Logical OR | Yes — right side not evaluated if left is `true` | `x == 0 or x == -1` |
| `not` | Logical NOT | N/A (unary) | `not done` |

---

## Bitwise Operators

Bitwise operators work on integer types only.

| Operator | Name | Example |
|----------|------|---------|
| `&` | Bitwise AND | `a & 0xFF` |
| `\|` | Bitwise OR | `a \| 0x01` |
| `^` | Bitwise XOR | `a ^ mask` |
| `~` | Bitwise NOT (unary) | `~flags` |
| `<<` | Left shift | `1 << 4` |
| `>>` | Right shift | `x >> 2` |

### Bitwise Assignment

| Operator | Equivalent |
|----------|------------|
| `&=` | `x = x & rhs` |
| `\|=` | `x = x \| rhs` |
| `^=` | `x = x ^ rhs` |
| `<<=` | `x = x << rhs` |
| `>>=` | `x = x >> rhs` |

---

## Reference Operators

| Operator | Name | Description |
|----------|------|-------------|
| `ref x` | Shared borrow | Creates an immutable reference to `x`. `x` must remain valid for the reference's lifetime. |
| `mut ref x` | Mutable borrow | Creates a mutable reference to `x`. Only one mutable reference may exist at a time. |
| `*ptr` | Dereference | Reads the value pointed to by `ptr`. |

```tml
let value = 42
let r: ref I32 = ref value      // shared reference
let m: mut ref I32 = mut ref value  // mutable reference (cannot have r active)
*m = 100
println(*m)  // 100
```

---

## Error Propagation Operator

| Operator | Name | Description |
|----------|------|-------------|
| `expr!` | Propagate | If `expr` is an error or `Nothing`, returns that error/nothing from the enclosing function. Otherwise, unwraps the value. |

The `!` operator works on `Maybe[T]` and `Outcome[T, E]`. The enclosing
function must return a compatible type.

```tml
func read_count(path: Str) -> Outcome[I32, IoError] {
    let text = file_read(path)!   // propagates IoError if reading fails
    let n = text.parse_i32()!     // propagates ParseError if parsing fails
    return Just(n)
}
```

---

## Member Access Operators

| Operator | Name | Description |
|----------|------|-------------|
| `.` | Field / method access | `point.x`, `list.len()` |
| `::` | Path separator | `List::new()`, `core::str::parse` |

The `.` operator is also used for method chaining. The `::` operator
separates module or type paths from their members.

---

## Range Operators

| Operator | Name | Includes end? | Example |
|----------|------|--------------|---------|
| `to` | Exclusive range | No | `0 to 10` → 0, 1, …, 9 |
| `through` | Inclusive range | Yes | `1 through 5` → 1, 2, 3, 4, 5 |

Ranges are most commonly used in `for` loops and slice expressions.

```tml
for i in 0 to 5 {
    print("${i} ")
}
// prints: 0 1 2 3 4

for i in 1 through 5 {
    print("${i} ")
}
// prints: 1 2 3 4 5
```

---

## Type Cast Operator

| Operator | Name | Description |
|----------|------|-------------|
| `as` | Type cast | Converts a value to the specified type. |

Numeric casts follow C-style truncation and extension rules. Casting to a
smaller type drops the high bits.

```tml
let x: I64 = 300
let y = x as I32   // 300
let z = x as U8    // 44 (300 % 256)
```

---

## Ternary Conditional Operator

| Operator | Name | Description |
|----------|------|-------------|
| `condition ? true_expr : false_expr` | Ternary | Evaluates to `true_expr` if `condition` is `true`, otherwise `false_expr`. |

```tml
let abs_val = x < 0 ? -x : x
let label = count == 1 ? "item" : "items"
```

Both branches must have the same type. For complex logic, prefer an `if`
expression.

---

## String Interpolation

String literals may embed expressions using `${}` syntax. The expression is
evaluated at runtime and converted to a string using its `Display` behavior.

```tml
let name = "Alice"
let score = 97
let msg = "Player ${name} scored ${score} points."
// "Player Alice scored 97 points."
```

Method calls and arithmetic are valid inside `${}`:

```tml
let path = "/tmp/data"
println("File: ${path.split('/').last()}")
println("Double: ${x * 2}")
```

---

## Assignment Operator

| Operator | Description |
|----------|-------------|
| `=` | Assigns a new value to a mutable binding or field. |

Plain `let` bindings are immutable after initialization. Use `var` for
bindings that will be reassigned.

---

## Operator Precedence

Operators are listed from highest precedence (evaluated first) to lowest
precedence (evaluated last). Operators on the same row have the same precedence
and are evaluated left-to-right unless otherwise noted.

| Level | Operators | Associativity | Notes |
|-------|-----------|---------------|-------|
| 1 (highest) | `.` `::` `()` `[]` | Left | Member access, calls, indexing |
| 2 | `!` | Left (postfix) | Error propagation |
| 3 | `*` (deref) `ref` `-` (unary) `not` `~` | Right | Unary operators |
| 4 | `as` | Left | Type casting |
| 5 | `*` `/` `%` | Left | Multiplicative |
| 6 | `+` `-` | Left | Additive |
| 7 | `<<` `>>` | Left | Bit shifts |
| 8 | `&` | Left | Bitwise AND |
| 9 | `^` | Left | Bitwise XOR |
| 10 | `\|` | Left | Bitwise OR |
| 11 | `==` `!=` `<` `>` `<=` `>=` | Left | Comparison |
| 12 | `and` | Left | Logical AND |
| 13 | `or` | Left | Logical OR |
| 14 | `to` `through` | Left | Range construction |
| 15 | `? :` | Right | Ternary conditional |
| 16 (lowest) | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` | Right | Assignment |

### Precedence Examples

```tml
// Multiplication binds tighter than addition
let a = 2 + 3 * 4       // 14, not 20

// Comparison binds tighter than logical operators
let b = 1 < 2 and 3 < 4  // true: (1 < 2) and (3 < 4)

// Unary minus binds tighter than multiplication
let c = -2 * 3           // -6: (-2) * 3

// Error propagation binds tighter than method access
let d = parse(text)!.len()  // unwrap first, then call .len()

// Use parentheses to override precedence
let e = (2 + 3) * 4     // 20
```
