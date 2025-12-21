# TML v1.0 — Lexical Specification

## 1. Encoding

| Aspect | Specification |
|--------|---------------|
| Encoding | UTF-8 (mandatory) |
| Newlines | LF (`\n`) canonical; CRLF normalized |
| Indentation | Spaces (2 or 4); tabs prohibited |
| BOM | Prohibited |

## 2. Keywords (28 reserved words)

```
// Declarations
module    import    public    private
func      type      trait     extend
let       var       const

// Control flow
if        then      else      when
loop      in        while     break
continue  return    catch

// Logical operators
and       or        not

// Values and references
true      false     this      This
```

### Differences from Rust

| Rust | TML | Reason |
|------|-----|--------|
| `fn` | `func` | More explicit |
| `impl` | `extend` | Reads naturally |
| `mod` | `module` | Unambiguous |
| `use` | `import` | Universal |
| `pub` | `public` | Complete |
| `mut` | (via `var`) | Simplified |
| `match` | `when` | No conflict |
| `self`/`Self` | `this`/`This` | Universal |
| `&&`/`\|\|`/`!` | `and`/`or`/`not` | Unambiguous |
| `for` | `loop in` | Unified |

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
| Traits | PascalCase | `Comparable` |
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

### 5.3 Logical (Keywords)

| Keyword | Meaning |
|---------|---------|
| `and` | Logical AND |
| `or` | Logical OR |
| `not` | Negation |

**Why keywords?**
```tml
// TML - clear
if a and b or not c then ...

// Rust - && and || look like bitwise
if a && b || !c { ... }
```

### 5.4 Bitwise

| Operator | Meaning |
|----------|---------|
| `&` | AND |
| `\|` | OR |
| `^` | XOR |
| `~` | NOT |
| `<<` | Shift left |
| `>>` | Shift right |

### 5.5 Assignment

| Operator | Meaning |
|----------|---------|
| `=` | Assignment |
| `+=` | Add-assign |
| `-=` | Sub-assign |
| `*=` | Mul-assign |
| `/=` | Div-assign |
| `%=` | Mod-assign |
| `&=` | And-assign |
| `\|=` | Or-assign |
| `^=` | Xor-assign |
| `<<=` | Shl-assign |
| `>>=` | Shr-assign |

### 5.6 Other

| Operator | Meaning |
|----------|---------|
| `->` | Return type |
| `=>` | (reserved) |
| `..` | Exclusive range |
| `..=` | Inclusive range |
| `!` | Error propagation |
| `&` | Reference |

## 6. Punctuation

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
| `@` | Stable ID |
| `#[` | Annotation start |

### Why `[]` for Generics?

```tml
// TML - no ambiguity
List[Int]
Map[String, Value]
a < b   // always comparison

// Rust - ambiguous
Vec<i32>
HashMap<String, Value>
a < b   // comparison or generic?
```

## 7. Stable IDs

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

## 8. Annotations

```ebnf
Annotation     = '#[' AnnotationBody ']'
AnnotationBody = Ident AnnotationArgs?
AnnotationArgs = '(' (AnnotationArg (',' AnnotationArg)*)? ')'
AnnotationArg  = Ident ('=' Literal)?
```

**Examples:**
```tml
#[test]
#[inline]
#[deprecated(reason = "use new_func")]
#[cfg(target = "wasm")]
#[derive(Eq, Hash)]
```

**Builtin Annotations:**

| Annotation | Meaning |
|------------|---------|
| `#[test]` | Marks test function |
| `#[bench]` | Marks benchmark |
| `#[inline]` | Suggests inlining |
| `#[cold]` | Unlikely path |
| `#[deprecated]` | Marks as obsolete |
| `#[cfg(...)]` | Conditional compilation |
| `#[derive(...)]` | Derives traits |
| `#[allow(...)]` | Suppresses warning |
| `#[deny(...)]` | Turns warning into error |

## 9. Comments

```ebnf
LineComment  = '//' [^\n]* '\n'
BlockComment = '/*' .* '*/'
DocComment   = '///' [^\n]* '\n'
ModuleDoc    = '//!' [^\n]* '\n'
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

## 10. Whitespace

- Spaces and newlines ignored between tokens
- No indentation significance (unlike Python)
- Newlines significant only in strings

## 11. Precedence Table

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
| 12 | Unary `-` `~` `&` | Unary |
| 13 (highest) | `.` `()` `[]` | Left |

## 12. Tokenization Example

**Source:**
```tml
func add[T: Numeric](a: T, b: T) -> T {
    return a + b
}
```

**Tokens:**
```
KEYWORD(func)
IDENT(add)
LBRACKET
IDENT(T)
COLON
IDENT(Numeric)
RBRACKET
LPAREN
IDENT(a)
COLON
IDENT(T)
COMMA
IDENT(b)
COLON
IDENT(T)
RPAREN
ARROW
IDENT(T)
LBRACE
KEYWORD(return)
IDENT(a)
PLUS
IDENT(b)
RBRACE
```

---

*Previous: [01-OVERVIEW.md](./01-OVERVIEW.md)*
*Next: [03-GRAMMAR.md](./03-GRAMMAR.md) — EBNF Grammar*
