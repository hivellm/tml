# Appendix B - Operators

This appendix lists all TML operators, grouped by category.

## Arithmetic Operators

| Operator | Name | Example |
|----------|------|---------|
| `+` | Addition | `a + b` |
| `-` | Subtraction | `a - b` |
| `*` | Multiplication | `a * b` |
| `/` | Division | `a / b` |
| `%` | Remainder | `a % b` |
| `-` | Negation (unary) | `-a` |

## Comparison Operators

| Operator | Name | Example |
|----------|------|---------|
| `==` | Equal | `a == b` |
| `!=` | Not equal | `a != b` |
| `<` | Less than | `a < b` |
| `>` | Greater than | `a > b` |
| `<=` | Less or equal | `a <= b` |
| `>=` | Greater or equal | `a >= b` |

## Logical Operators

| Operator | Name | Example |
|----------|------|---------|
| `and` | Logical AND | `a and b` |
| `or` | Logical OR | `a or b` |
| `not` | Logical NOT | `not a` |

## Bitwise Operators

| Operator | Name | Example |
|----------|------|---------|
| `&` | Bitwise AND | `a & b` |
| `\|` | Bitwise OR | `a \| b` |
| `^` | Bitwise XOR | `a ^ b` |
| `~` | Bitwise NOT | `~a` |
| `<<` | Left shift | `a << n` |
| `>>` | Right shift | `a >> n` |

## Assignment Operators

| Operator | Name | Example |
|----------|------|---------|
| `=` | Assignment | `a = b` |

## Reference Operators

| Operator | Name | Example |
|----------|------|---------|
| `ref` | Create reference | `ref x` |
| `*` | Dereference | `*ptr` |

## Range Operators

| Operator | Name | Example |
|----------|------|---------|
| `to` | Exclusive range | `0 to 10` |
| `through` | Inclusive range | `1 through 10` |

## Access Operators

| Operator | Name | Example |
|----------|------|---------|
| `.` | Field access | `obj.field` |
| `::` | Path separator | `module::func` |

## Operator Precedence

From highest to lowest precedence:

| Precedence | Operators |
|------------|-----------|
| 1 (highest) | `.` `::` function calls |
| 2 | `-` (unary) `not` `~` `ref` `*` |
| 3 | `*` `/` `%` |
| 4 | `+` `-` |
| 5 | `<<` `>>` |
| 6 | `&` |
| 7 | `^` |
| 8 | `\|` |
| 9 | `==` `!=` `<` `>` `<=` `>=` |
| 10 | `and` |
| 11 | `or` |
| 12 (lowest) | `=` |

## Examples

```tml
// Arithmetic
let sum = 10 + 5      // 15
let product = 3 * 4   // 12

// Comparison
let is_equal = 5 == 5      // true
let is_greater = 10 > 5    // true

// Logical (note: uses words, not symbols)
let both = true and false   // false
let either = true or false  // true
let negated = not true      // false

// Bitwise
let masked = 0xFF & 0x0F    // 15
let shifted = 1 << 4        // 16

// Combined with precedence
let result = 2 + 3 * 4      // 14 (not 20)
let grouped = (2 + 3) * 4   // 20
```
