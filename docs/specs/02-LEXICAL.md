# TML v1.0 — Lexical Specification

## 1. Encoding

| Aspect | Specification |
|--------|---------------|
| Encoding | UTF-8 (mandatory) |
| Newlines | LF (`\n`) canonical; CRLF normalized |
| Indentation | Spaces (2 or 4); tabs prohibited |
| BOM | Prohibited |

## 2. Keywords (35 reserved words)

```
// Declarations
module    import    public    private
func      type      behavior  extend
let       var       const     where

// Control flow
if        then      else      when
loop      in        while     break
continue  return    catch     do

// Logical operators (words, not symbols)
and       or        not

// Values and references
true      false     this      This
ref       lowlevel

// Range keywords
to        through
```

### Why Words Over Symbols

| Symbol-based | TML (Word-based) | Reason |
|--------------|------------------|--------|
| `&&` `\|\|` `!` | `and` `or` `not` | Natural language, unambiguous |
| `&T` `&mut T` | `ref T` `mut ref T` | Clear meaning, no symbol overload |
| `..` `..=` | `to` `through` | Reads like English |
| `unsafe` | `lowlevel` | Neutral, descriptive |
| `trait` | `behavior` | Describes what it defines |

## 3. Identifiers

```ebnf
Ident  = Letter (Letter | Digit | '_')*
Letter = 'a'..'z' | 'A'..'Z' | '_'
Digit  = '0'..'9'
```

### Conventions

| Type | Style | Example |
|------|-------|---------|
| Variables | snake_case | `my_value` |
| Functions | snake_case | `calculate_total` |
| Types | PascalCase | `HttpClient` |
| Behaviors | PascalCase | `Comparable` |
| Constants | SCREAMING_CASE | `MAX_SIZE` |
| Modules | snake_case | `http_client` |

### Reserved Identifiers

```
_        // placeholder (discards value)
_name    // private by convention
```

## 4. Literals

### 4.1 Integers

```ebnf
IntLit  = DecInt | HexInt | BinInt | OctInt
DecInt  = Digit (Digit | '_')*
HexInt  = '0x' HexDigit (HexDigit | '_')*
BinInt  = '0b' ('0' | '1' | '_')+
OctInt  = '0o' OctDigit (OctDigit | '_')*
```

**Type suffixes:**
```
i8  i16  i32  i64  i128    // signed
u8  u16  u32  u64  u128    // unsigned
```

**Examples:**
```tml
42
1_000_000
0xFF_AA_BB
0b1010_1010
0o755

42i32
255u8
1_000_000i64
```

### 4.2 Floats

```ebnf
FloatLit = DecInt '.' DecInt Exponent?
Exponent = ('e' | 'E') ('+' | '-')? DecInt
```

**Suffixes:**
```
f32  f64
```

**Examples:**
```tml
3.14159
2.5e10
1.0e-5
3.14f32
```

### 4.3 Strings

```ebnf
String    = '"' StringChar* '"'
StringChar = EscapeSeq | [^"\\\n]

RawString = 'r"' [^"]* '"'
          | 'r#"' .* '"#'

MultiLine = '"""' .* '"""'
```

**Escape sequences:**
| Escape | Meaning |
|--------|---------|
| `\\` | Backslash |
| `\"` | Quote |
| `\n` | Newline |
| `\r` | Carriage return |
| `\t` | Tab |
| `\0` | Null |
| `\xNN` | Hex byte |
| `\u{NNNN}` | Unicode codepoint |

**Examples:**
```tml
"hello world"
"line1\nline2"
"path: C:\\Users"

r"no \escapes here"
r#"can "quote" freely"#

"""
Multi-line
string
"""
```

### 4.4 Bytes

```ebnf
Bytes    = 'b"' ByteChar* '"'
ByteChar = EscapeSeq | [\x00-\x7F]
```

**Examples:**
```tml
b"hello"
b"\x00\xFF"
```

### 4.5 Characters

```ebnf
Char = "'" (EscapeSeq | [^'\\\n]) "'"
```

**Examples:**
```tml
'a'
'\n'
'\u{1F600}'
```

## 5. Operators

### 5.1 Arithmetic

| Operator | Meaning |
|----------|---------|
| `+` | Addition |
| `-` | Subtraction / Negation |
| `*` | Multiplication |
| `/` | Division |
| `%` | Modulo |
| `**` | Exponentiation |

### 5.2 Comparison

| Operator | Meaning |
|----------|---------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less or equal |
| `>=` | Greater or equal |

### 5.3 Logical (Keywords Only)

| Keyword | Meaning |
|---------|---------|
| `and` | Logical AND (short-circuit) |
| `or` | Logical OR (short-circuit) |
| `not` | Logical negation |

**Why keywords?**
```tml
// TML - reads like English
if a and b or not c then ...

// Symbol-based languages - cryptic
if a && b || !c { ... }
```

### 5.4 Bitwise

| Operator | Meaning |
|----------|---------|
| `&` | Bitwise AND |
| `\|` | Bitwise OR |
| `^` | Bitwise XOR |
| `~` | Bitwise NOT |
| `<<` | Shift left |
| `>>` | Shift right |

**Note:** `&` is **only** bitwise AND in TML. References use the `ref` keyword.

### 5.5 Assignment

| Operator | Meaning |
|----------|---------|
| `=` | Assignment |
| `+=` | Add-assign |
| `-=` | Sub-assign |
| `*=` | Mul-assign |
| `/=` | Div-assign |
| `%=` | Mod-assign |
| `&=` | Bitwise And-assign |
| `\|=` | Bitwise Or-assign |
| `^=` | Bitwise Xor-assign |
| `<<=` | Shl-assign |
| `>>=` | Shr-assign |

### 5.6 Other

| Operator | Meaning |
|----------|---------|
| `->` | Return type arrow |
| `!` | Error propagation |
| `?` | Optional chaining (reserved) |

### 5.7 Range Keywords

| Keyword | Meaning | Example |
|---------|---------|---------|
| `to` | Exclusive range | `0 to 10` = 0, 1, 2, ... 9 |
| `through` | Inclusive range | `0 through 10` = 0, 1, 2, ... 10 |

```tml
// Ranges in loops
loop i in 0 to 10 {
    print(i)  // 0 through 9
}

loop i in 1 through 5 {
    print(i)  // 1 through 5
}

// Ranges in slices
let first_five: T = items[0 to 5]
let all_from_three: T = items[3 to items.len()]
```

## 6. Type Modifiers

### 6.1 Reference Types

| Modifier | Meaning | Example |
|----------|---------|---------|
| `ref T` | Immutable reference | `ref String` |
| `mut ref T` | Mutable reference | `mut ref String` |
| `ptr T` | Raw pointer (lowlevel) | `ptr U8` |
| `mut ptr T` | Mutable raw pointer | `mut ptr U8` |

**Examples:**
```tml
// Immutable reference
func length(s: ref String) -> U64 {
    return s.len()
}

// Mutable reference
func append(s: mut ref String, suffix: String) {
    s.push(suffix)
}

// Raw pointers (lowlevel only)
@lowlevel
func read_byte(p: ptr U8) -> U8 {
    return p.read()
}
```

**Why `ref` instead of `&`?**
```tml
// TML - clear and unambiguous
func process(data: ref String) -> ref String

// Symbol-based - & has multiple meanings
fn process(data: &String) -> &String
```

## 7. Punctuation

| Symbol | Use |
|--------|-----|
| `(` `)` | Calls, grouping, parameters |
| `[` `]` | Generics, arrays, indices |
| `{` `}` | Blocks |
| `,` | Separator |
| `;` | Terminator (optional) |
| `:` | Type annotation |
| `::` | Path separator |
| `.` | Member access |
| `@` | Directive prefix / Stable ID |

### Why `[]` for Generics?

```tml
// TML - no ambiguity
List[I32]
Map[String, Value]
a < b   // always comparison

// Symbol-based - ambiguous
Vec<i32>
HashMap<String, Value>
a < b   // comparison or generic?
```

## 8. Stable IDs

```ebnf
StableId = '@' HexDigit{8}
HexDigit = '0'..'9' | 'a'..'f'
```

**Examples:**
```tml
@a1b2c3d4
@00000001
@deadbeef
```

**Usage:**
```tml
func calculate@a1b2c3d4(x: I32) -> I32 {
    return x * 2
}

type Point@b2c3d4e5 {
    x: F64,
    y: F64,
}
```

IDs are:
- Generated by the compiler (or preserved from source)
- Unique within the module
- Immutable after creation
- Used for references in patches/diffs

## 9. Directives

Directives use `@` prefix with natural language syntax.

```ebnf
Directive     = '@' DirectiveName DirectiveArgs?
DirectiveName = Ident
DirectiveArgs = '(' (DirectiveArg (',' DirectiveArg)*)? ')'
DirectiveArg  = Ident (':' Value)?
```

**Examples:**
```tml
@test
@when(os: linux)
@auto(debug, duplicate, equal)
@deprecated("Use new_func instead")
@hint(inline: always)
@lowlevel
```

### 9.1 Builtin Directives

| Directive | Meaning |
|-----------|---------|
| `@test` | Marks test function |
| `@test(should_fail)` | Test expected to fail |
| `@benchmark` | Marks benchmark function |
| `@when(...)` | Conditional compilation |
| `@unless(...)` | Negative conditional |
| `@auto(...)` | Auto-generate implementations |
| `@hint(...)` | Compiler optimization hints |
| `@deprecated(...)` | Marks as obsolete |
| `@lowlevel` | Allows low-level operations |
| `@export` | Public API marker |
| `@doc(...)` | Documentation |

### 9.2 Conditional Directives

```tml
@when(os: linux)
func linux_only() { ... }

@when(os: windows)
func windows_only() { ... }

@when(arch: x86_64)
func x64_optimized() { ... }

@when(feature: async)
func async_version() { ... }

@when(debug)
func debug_only() { ... }

@unless(os: windows)
func not_windows() { ... }
```

### 9.3 Auto-Generation Directives

```tml
@auto(debug)              // Generate debug representation
@auto(duplicate)          // Generate .duplicate() method
@auto(equal)              // Generate equality comparison
@auto(order)              // Generate ordering
@auto(hash)               // Generate hashing
@auto(format)             // Generate string formatting

@auto(debug, duplicate, equal)  // Multiple at once
type Point { x: F64, y: F64 }
```

### 9.4 Optimization Hints

```tml
@hint(inline)             // Suggest inlining
@hint(inline: always)     // Force inlining
@hint(inline: never)      // Prevent inlining
@hint(cold)               // Unlikely code path
@hint(hot)                // Hot code path
```

## 10. Comments

```ebnf
LineComment  = '//' [^\n]* '\n'
BlockComment = '/*' .* '*/'
DocComment   = '///' [^\n]* '\n'
ModuleDoc    = '//!' [^\n]* '\n'
AIComment    = '// @ai:' AIDirective
```

**Examples:**
```tml
// Line comment

/* Block
   comment */

/// Documents the following item
/// Supports **markdown**
func example() { }

//! Documents the current module
module utils
```

### 10.1 AI Context Comments

AI context comments provide hints to LLMs that are not captured in types or contracts.

```ebnf
AIComment = '// @ai:' Directive ':' Content
Directive = 'context' | 'intent' | 'invariant' | 'warning' | 'example'
Content   = [^\n]*
```

**Standard Directives:**

| Directive | Purpose |
|-----------|---------|
| `@ai:context` | General context about usage |
| `@ai:intent` | High-level purpose (security, performance) |
| `@ai:invariant` | Assumptions callers must maintain |
| `@ai:warning` | Caveats or deprecation notices |
| `@ai:example` | Example usage for generation |

**Examples:**

```tml
// @ai:context: Hot loop, optimize for speed
func process_batch(items: List[Data]) -> List[Outcome[Data, Error]] {
    // ...
}

// @ai:intent: User authentication - security critical
func validate_token(token: String) -> Outcome[User, AuthError] {
    // ...
}

// @ai:invariant: items is always sorted before this call
func binary_search[T: Ordered](items: ref List[T], target: T) -> Maybe[U64] {
    // ...
}
```

## 11. Whitespace

- Spaces and newlines ignored between tokens
- No indentation significance (unlike Python)
- Newlines significant only in strings

## 12. Precedence Table

| Precedence | Operators | Associativity |
|------------|-----------|---------------|
| 1 (lowest) | `or` | Left |
| 2 | `and` | Left |
| 3 | `not` | Unary |
| 4 | `==` `!=` `<` `>` `<=` `>=` | None |
| 5 | `\|` | Left |
| 6 | `^` | Left |
| 7 | `&` | Left |
| 8 | `<<` `>>` | Left |
| 9 | `+` `-` | Left |
| 10 | `*` `/` `%` | Left |
| 11 | `**` | Right |
| 12 | Unary `-` `~` | Unary |
| 13 (highest) | `.` `()` `[]` | Left |

## 13. Tokenization Example

**Source:**
```tml
func first[T](items: ref List[T]) -> Maybe[T] {
    return items.get(0)
}
```

**Tokens:**
```
KEYWORD(func)
IDENT(first)
LBRACKET
IDENT(T)
RBRACKET
LPAREN
IDENT(items)
COLON
KEYWORD(ref)
IDENT(List)
LBRACKET
IDENT(T)
RBRACKET
RPAREN
ARROW
IDENT(Maybe)
LBRACKET
IDENT(T)
RBRACKET
LBRACE
KEYWORD(return)
IDENT(items)
DOT
IDENT(get)
LPAREN
INT(0)
RPAREN
RBRACE
```

---

*Previous: [01-OVERVIEW.md](./01-OVERVIEW.md)*
*Next: [03-GRAMMAR.md](./03-GRAMMAR.md) — EBNF Grammar*
