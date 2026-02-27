# Proposal: Text Type - Dynamic String Type

## Status
- **Created**: 2025-01-15
- **Status**: Draft
- **Priority**: High

## Why

TML currently uses `Str` as a pointer-based string type that is fast and efficient for immutable strings. However, modern applications often need dynamic string manipulation (building HTML, JSON, logs, user messages). Adding a `Text` type would:

1. **Safe dynamic strings** - Heap-allocated, growable strings with bounds checking
2. **Rich manipulation API** - JavaScript-like methods for common string operations
3. **Template literals** - Backtick syntax for clean string interpolation
4. **Developer choice** - Use `Str` for static/immutable strings, `Text` for dynamic ones
5. **LLM familiarity** - Methods named like JavaScript/Python for easier code generation

### Current State vs Proposed

```tml
// CURRENT: Str - pointer-based, immutable
let name: Str = "Alice"
let greeting = "Hello, " + name + "!"  // Works but creates temp strings

// Proposed format() - requires function call
let greeting = format("Hello, {}!", name)
```

```tml
// PROPOSED: Text - heap-allocated, mutable, with template literals
let name = "Alice"
let greeting = `Hello, {name}!`  // Template literal - clean interpolation

// Rich manipulation API
let text = Text::new()
text.append("Hello")
text.push_str(", World!")
text.to_upper_case()

// JavaScript-like methods
let result = text.trim().replace("World", "TML").split(",")
```

## What Changes

### New Type: Text

```tml
// Core structure (internal representation)
class Text {
    private data: mut ptr U8    // Heap-allocated buffer
    private len: U64            // Current length
    private cap: U64            // Allocated capacity

    // Constructors
    static func new() -> Text
    static func from(s: Str) -> Text
    static func with_capacity(cap: U64) -> Text

    // Conversion
    func as_str(this) -> Str           // Borrow as Str (no copy)
    func to_str(this) -> Str           // Copy to Str
    func clone(this) -> Text           // Deep copy

    // Basic operations
    func len(this) -> U64
    func capacity(this) -> U64
    func is_empty(this) -> Bool
    func clear(mut this)

    // Growing
    func push(mut this, c: U8)         // Append single char
    func push_str(mut this, s: Str)    // Append string
    func append(mut this, other: ref Text)

    // Reserve
    func reserve(mut this, additional: U64)
    func shrink_to_fit(mut this)
}
```

### JavaScript-like Methods

```tml
// These methods follow JavaScript naming for LLM familiarity

// Character access
func char_at(this, idx: U64) -> Maybe[U8]
func char_code_at(this, idx: U64) -> Maybe[U32]

// Search
func index_of(this, search: Str) -> Maybe[U64]
func last_index_of(this, search: Str) -> Maybe[U64]
func includes(this, search: Str) -> Bool
func starts_with(this, prefix: Str) -> Bool
func ends_with(this, suffix: Str) -> Bool

// Extraction
func slice(this, start: I64, end: I64) -> Text
func substring(this, start: U64, end: U64) -> Text
func substr(this, start: U64, length: U64) -> Text

// Transformation (return new Text)
func to_upper_case(this) -> Text
func to_lower_case(this) -> Text
func trim(this) -> Text
func trim_start(this) -> Text
func trim_end(this) -> Text
func pad_start(this, length: U64, pad: Str) -> Text
func pad_end(this, length: U64, pad: Str) -> Text
func repeat(this, count: U64) -> Text
func replace(this, search: Str, replacement: Str) -> Text
func replace_all(this, search: Str, replacement: Str) -> Text
func reverse(this) -> Text

// Splitting and joining
func split(this, separator: Str) -> List[Text]
func split_n(this, separator: Str, limit: U64) -> List[Text]
static func join(parts: List[Text], separator: Str) -> Text

// Comparison
func compare(this, other: ref Text) -> I32
func compare_ignore_case(this, other: ref Text) -> I32
func equals_ignore_case(this, other: ref Text) -> Bool

// Formatting
func format(this, args: ...Any) -> Text  // sprintf-style
```

### Template Literals

Template literals use backticks and allow embedded expressions:

```tml
// Basic interpolation
let name = "Alice"
let age = 30
let message = `{name} is {age} years old`

// Expressions in templates
let result = `2 + 2 = {2 + 2}`
let greeting = `Hello, {user.name.to_upper_case()}!`

// Multi-line strings
let html = `
<div class="card">
    <h1>{title}</h1>
    <p>{description}</p>
</div>
`

// Nested templates
let items = `Items: {items.map(do(i) `<li>{i}</li>`).join("")}`

// Escape sequences
let escaped = `Use \{ for literal braces`
let newline = `Line 1\nLine 2`
```

### Implicit Conversions

```tml
// Str -> Text (automatic when needed)
let text: Text = "hello"  // Implicit conversion

// Text can be used where Str is expected (via as_str)
func greet(name: Str) { print("Hello, " + name) }
let name = Text::from("World")
greet(name.as_str())  // Explicit conversion required

// Template literals produce Text
let msg = `Hello`  // Type is Text
```

### Operator Overloads

```tml
// Concatenation with +
let a = Text::from("Hello")
let b = Text::from("World")
let c = a + " " + b  // "Hello World"

// Comparison with == and !=
if text1 == text2 { ... }

// Indexing with []
let c = text[0]  // First byte (returns Maybe[U8])
```

## Implementation Phases

### Phase 1: Core Type
- Define Text struct layout
- Implement memory allocation/deallocation
- Implement basic constructors and conversions

### Phase 2: Basic Methods
- len, capacity, is_empty, clear
- push, push_str, append
- reserve, shrink_to_fit

### Phase 3: JavaScript-like Methods
- Search methods (index_of, includes, etc.)
- Transformation methods (trim, replace, etc.)
- Split/join methods

### Phase 4: Template Literals
- Lexer: recognize backtick strings
- Parser: parse interpolated expressions
- Codegen: generate Text construction with formatting

### Phase 5: Operators
- Implement + for concatenation
- Implement == and != for comparison
- Implement [] for indexing

## Impact

- **Additive**: New type, existing `Str` code unchanged
- **Lexer**: Add backtick string token with interpolation
- **Parser**: Parse template literal expressions
- **Type System**: Add Text type with method signatures
- **Codegen**: Generate Text operations and template literal expansion
- **Runtime**: Memory allocation for Text buffers
- **Stdlib**: New `std::text` module

## Comparison with Other Languages

| Feature | JavaScript | Python | Rust | TML (Proposed) |
|---------|-----------|--------|------|----------------|
| Dynamic string | `String` | `str` | `String` | `Text` |
| Static string | `string literal` | `str` | `&str` | `Str` |
| Template literal | `` `${x}` `` | `f"{x}"` | `format!("{x}")` | `` `{x}` `` |
| Concatenation | `+` | `+` | `+ or format!` | `+` |
| Index access | `s[i]` | `s[i]` | `s.chars().nth(i)` | `s[i]` or `s.char_at(i)` |
| Slice | `s.slice(a,b)` | `s[a:b]` | `&s[a..b]` | `s.slice(a,b)` |
| Split | `s.split(",")` | `s.split(",")` | `s.split(",")` | `s.split(",")` |

## Memory Management

```tml
// Text owns its buffer and frees on drop
{
    let text = Text::from("Hello")
    // ... use text ...
}  // text dropped, buffer freed

// Clone creates independent copy
let original = Text::from("Hello")
let copy = original.clone()  // New allocation

// as_str() borrows without allocation
let text = Text::from("Hello")
let view: Str = text.as_str()  // No allocation, borrows from text
```

## Performance Considerations

- **Small String Optimization (SSO)**: Strings <= 23 bytes stored inline (no heap allocation)
- **Growth strategy**: 2x capacity on reallocation
- **UTF-8 encoding**: All strings are UTF-8, methods handle multi-byte chars
- **Lazy evaluation**: Some operations (like repeat) could be lazy

## References

- JavaScript String: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String
- Rust String: https://doc.rust-lang.org/std/string/struct.String.html
- Python str: https://docs.python.org/3/library/stdtypes.html#text-sequence-type-str
