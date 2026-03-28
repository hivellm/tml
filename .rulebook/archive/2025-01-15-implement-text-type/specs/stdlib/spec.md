# Text Type Technical Specification

## 1. Memory Layout

### 1.1 Text Structure (64 bytes total with SSO)

```c
// C representation in runtime
typedef struct {
    union {
        struct {
            uint8_t* data;     // Heap pointer (8 bytes)
            uint64_t len;      // Current length (8 bytes)
            uint64_t cap;      // Allocated capacity (8 bytes)
        } heap;
        struct {
            uint8_t data[23];  // Inline storage
            uint8_t len;       // Length (stored in last byte)
        } inline_str;
    };
    uint8_t flags;             // Bit 0: is_inline, Bits 1-7: reserved
} TmlText;
```

### 1.2 Small String Optimization (SSO)

- Strings <= 23 bytes are stored inline (no heap allocation)
- Flag byte indicates inline vs heap storage
- Inline strings: `len` stored in last byte of inline_str
- Heap strings: `flags & 0x01 == 0`

### 1.3 LLVM IR Type

```llvm
; Text type in LLVM
%struct.TmlText = type {
    i8*,    ; data pointer (or inline storage start)
    i64,    ; len
    i64,    ; cap
    i8      ; flags
}
```

## 2. Runtime Functions

### 2.1 Allocation

```c
// Create empty Text with default capacity
TmlText* tml_text_new(void);

// Create Text from string literal
TmlText* tml_text_from_str(const uint8_t* data, uint64_t len);

// Create Text with pre-allocated capacity
TmlText* tml_text_with_capacity(uint64_t cap);

// Clone (deep copy)
TmlText* tml_text_clone(const TmlText* text);

// Drop (free memory)
void tml_text_drop(TmlText* text);
```

### 2.2 Access

```c
// Get length
uint64_t tml_text_len(const TmlText* text);

// Get capacity
uint64_t tml_text_cap(const TmlText* text);

// Get data pointer (for reading)
const uint8_t* tml_text_data(const TmlText* text);

// Check if empty
bool tml_text_is_empty(const TmlText* text);
```

### 2.3 Modification

```c
// Push single byte
void tml_text_push(TmlText* text, uint8_t c);

// Push string
void tml_text_push_str(TmlText* text, const uint8_t* data, uint64_t len);

// Append another Text
void tml_text_append(TmlText* text, const TmlText* other);

// Clear (set length to 0)
void tml_text_clear(TmlText* text);

// Reserve additional capacity
void tml_text_reserve(TmlText* text, uint64_t additional);

// Shrink capacity to fit current length
void tml_text_shrink_to_fit(TmlText* text);
```

### 2.4 Growth Strategy

```c
// When capacity is exceeded:
// 1. If cap == 0: new_cap = 8
// 2. If cap < 64: new_cap = cap * 2
// 3. If cap >= 64: new_cap = cap + cap / 2 (1.5x)
// 4. Ensure new_cap >= required

static uint64_t grow_capacity(uint64_t current, uint64_t required) {
    uint64_t new_cap;
    if (current == 0) {
        new_cap = 8;
    } else if (current < 64) {
        new_cap = current * 2;
    } else {
        new_cap = current + current / 2;
    }
    return new_cap >= required ? new_cap : required;
}
```

## 3. Template Literals

### 3.1 Lexer Changes

Add new token types:

```cpp
enum class TokenKind {
    // ... existing tokens ...

    // Template literals
    TemplateLiteralStart,    // ` at start
    TemplateLiteralEnd,      // ` at end
    TemplateLiteralPart,     // String content between interpolations
    TemplateExprStart,       // {
    TemplateExprEnd,         // }
};
```

### 3.2 Lexer State Machine

```
State: Normal
  '`' -> State: InTemplate, emit TemplateLiteralStart

State: InTemplate
  '{' -> State: InTemplateExpr, emit TemplateLiteralPart, emit TemplateExprStart
  '`' -> State: Normal, emit TemplateLiteralPart, emit TemplateLiteralEnd
  '\{' -> escape, add '{' to buffer
  '\\' -> escape next char
  other -> add to buffer

State: InTemplateExpr
  '}' -> State: InTemplate, emit expression tokens, emit TemplateExprEnd
  '{' -> nested_depth++
  other -> lex as normal expression
```

### 3.3 Parser AST

```cpp
struct TemplateLiteralExpr {
    std::vector<TemplatePart> parts;

    struct TemplatePart {
        std::variant<
            std::string,     // String literal part
            ExprPtr          // Interpolated expression
        > content;
    };
};

// Example: `Hello, {name}!`
// Parts: ["Hello, ", name_expr, "!"]
```

### 3.4 Type Checking

```cpp
// In type checker:
auto TypeChecker::check_template_literal(const TemplateLiteralExpr& expr) -> TypePtr {
    for (const auto& part : expr.parts) {
        if (auto* e = std::get_if<ExprPtr>(&part.content)) {
            auto type = check_expr(*e);
            // Verify type has Display or can be converted to string
            if (!can_display(type)) {
                error("Expression type cannot be displayed in template");
            }
        }
    }
    return make_text_type();
}
```

### 3.5 Codegen

```cpp
// Generate code for template literal
auto Codegen::emit_template_literal(const TemplateLiteralExpr& expr) -> llvm::Value* {
    // Calculate total estimated size
    size_t estimated_size = 0;
    for (const auto& part : expr.parts) {
        if (auto* s = std::get_if<std::string>(&part.content)) {
            estimated_size += s->size();
        } else {
            estimated_size += 16;  // Estimate for expression
        }
    }

    // Create Text with estimated capacity
    auto* text = call_runtime("tml_text_with_capacity", estimated_size);

    // Append each part
    for (const auto& part : expr.parts) {
        if (auto* s = std::get_if<std::string>(&part.content)) {
            // Append string literal
            call_runtime("tml_text_push_str", text, *s);
        } else if (auto* e = std::get_if<ExprPtr>(&part.content)) {
            // Evaluate expression and convert to string
            auto* value = emit_expr(*e);
            auto* str = call_to_string(value, e->type);
            call_runtime("tml_text_push_str", text, str);
        }
    }

    return text;
}
```

## 4. Method Implementation Examples

### 4.1 trim()

```tml
func trim(this) -> Text {
    let start = 0u64
    let end = this.len()

    // Find first non-whitespace
    loop i in 0 to this.len() {
        if not this.byte_at(i).unwrap().is_whitespace() {
            start = i
            break
        }
    }

    // Find last non-whitespace
    loop i in (this.len() - 1) through 0 by -1 {
        if not this.byte_at(i as U64).unwrap().is_whitespace() {
            end = i as U64 + 1
            break
        }
    }

    return this.substring(start, end)
}
```

### 4.2 split()

```tml
func split(this, separator: Str) -> List[Text] {
    let result = List[Text]::new()
    let start = 0u64

    loop {
        when this.index_of_from(separator, start) {
            Just(idx) => {
                result.push(this.substring(start, idx))
                start = idx + separator.len()
            }
            Nothing => {
                result.push(this.substring(start, this.len()))
                break
            }
        }
    }

    return result
}
```

### 4.3 replace_all()

```tml
func replace_all(this, search: Str, replacement: Str) -> Text {
    if search.is_empty() {
        return this.clone()
    }

    let result = Text::with_capacity(this.len())
    let start = 0u64

    loop {
        when this.index_of_from(search, start) {
            Just(idx) => {
                result.push_str(this.substring(start, idx).as_str())
                result.push_str(replacement)
                start = idx + search.len()
            }
            Nothing => {
                result.push_str(this.substring(start, this.len()).as_str())
                break
            }
        }
    }

    return result
}
```

## 5. Operator Implementation

### 5.1 Concatenation (+)

```cpp
// In type checker - register operator
register_binary_op("+", TextType, TextType, TextType);
register_binary_op("+", TextType, StrType, TextType);
register_binary_op("+", StrType, TextType, TextType);

// In codegen
if (is_text_concat(op, left_type, right_type)) {
    auto* result = call_runtime("tml_text_clone", left);
    call_runtime("tml_text_append", result, right);
    return result;
}
```

### 5.2 Comparison (==)

```cpp
// In codegen
auto* len_eq = builder.CreateICmpEQ(
    call_runtime("tml_text_len", left),
    call_runtime("tml_text_len", right)
);

auto* then_block = create_block("cmp.then");
auto* else_block = create_block("cmp.else");
auto* merge_block = create_block("cmp.merge");

builder.CreateCondBr(len_eq, then_block, else_block);

// Then block: compare data
builder.SetInsertPoint(then_block);
auto* memcmp_result = call_runtime("tml_text_compare", left, right);
auto* is_equal = builder.CreateICmpEQ(memcmp_result, i32(0));
builder.CreateBr(merge_block);

// Else block: not equal
builder.SetInsertPoint(else_block);
builder.CreateBr(merge_block);

// Merge
builder.SetInsertPoint(merge_block);
auto* phi = builder.CreatePHI(i1_type, 2);
phi->addIncoming(is_equal, then_block);
phi->addIncoming(i1(false), else_block);
return phi;
```

## 6. Integration with Existing Types

### 6.1 Str to Text Coercion

```cpp
// In type checker
if (target_type->is_text() && source_type->is_str()) {
    // Allow implicit coercion
    return CoercionKind::StrToText;
}

// In codegen
if (coercion == CoercionKind::StrToText) {
    auto* str_ptr = extract_str_ptr(value);
    auto* str_len = extract_str_len(value);
    return call_runtime("tml_text_from_str", str_ptr, str_len);
}
```

### 6.2 Text to Str Conversion

```tml
// Explicit via as_str() - borrows, no allocation
func as_str(this) -> Str {
    // Returns a Str that borrows from this Text
    // Only valid while Text is alive
    return Str::from_raw(this.data, this.len)
}

// Explicit via to_str() - copies, allocates
func to_str(this) -> Str {
    // Returns a new Str with copied data
    return Str::from_bytes(this.as_bytes())
}
```

## 7. Performance Considerations

### 7.1 SSO Threshold

- 23 bytes chosen to fit in 3 pointers (24 bytes) minus 1 for length
- Common strings (variable names, short messages) fit inline
- No heap allocation for small strings = faster allocation

### 7.2 Amortized Growth

- Starting capacity: 8 bytes
- Growth factor: 2x until 64, then 1.5x
- Amortized O(1) for push operations
- Avoids excessive memory use for large strings

### 7.3 Template Literal Optimization

- Pre-calculate total size estimate
- Single allocation for result
- Avoid intermediate Text objects

## 8. Error Handling

### 8.1 Out of Memory

```c
TmlText* tml_text_reserve(TmlText* text, uint64_t additional) {
    uint64_t new_cap = text->cap + additional;
    uint8_t* new_data = realloc(text->data, new_cap);
    if (!new_data) {
        // Call TML panic handler
        tml_panic("out of memory");
        return NULL;  // Never reached
    }
    text->data = new_data;
    text->cap = new_cap;
    return text;
}
```

### 8.2 Index Out of Bounds

```tml
func char_at(this, idx: U64) -> Maybe[U8] {
    if idx >= this.len() {
        return Nothing
    }
    return Just(this.byte_at_unchecked(idx))
}
```

## 9. Thread Safety

- Text is NOT thread-safe by default
- Use `Sync[Text]` for shared ownership
- Clone before sending to other threads
- Future: Add `TextAtomic` for lock-free concurrent access
