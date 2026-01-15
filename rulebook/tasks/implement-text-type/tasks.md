# Tasks: Text Type Implementation

**Status**: Not Started

**Priority**: High - Core language feature for string manipulation

## Phase 1: Core Type Definition

### 1.1 Type System
- [ ] 1.1.1 Add `Text` to primitive types in `types/type.hpp`
- [ ] 1.1.2 Add `TextType` variant to type kinds
- [ ] 1.1.3 Register Text type in type environment
- [ ] 1.1.4 Implement Text type checking rules

### 1.2 Memory Layout
- [ ] 1.2.1 Define Text struct in runtime (`data: *mut u8, len: u64, cap: u64`)
- [ ] 1.2.2 Implement LLVM struct type for Text
- [ ] 1.2.3 Add Text allocation function in runtime
- [ ] 1.2.4 Add Text deallocation function in runtime
- [ ] 1.2.5 Implement drop glue for Text

### 1.3 SSO (Small String Optimization)
- [ ] 1.3.1 Define SSO threshold (23 bytes)
- [ ] 1.3.2 Implement inline storage for small strings
- [ ] 1.3.3 Add is_inline() check
- [ ] 1.3.4 Handle SSO in allocation/deallocation

## Phase 2: Constructors and Conversion

### 2.1 Constructors
- [ ] 2.1.1 Implement `Text::new()` - empty text
- [ ] 2.1.2 Implement `Text::from(s: Str)` - from string literal
- [ ] 2.1.3 Implement `Text::with_capacity(cap: U64)` - pre-allocated
- [ ] 2.1.4 Implement `Text::from_utf8(bytes: ref [U8])` - from byte slice
- [ ] 2.1.5 Implement `Text::from_char(c: U8, count: U64)` - repeated char

### 2.2 Conversion Methods
- [ ] 2.2.1 Implement `as_str(this) -> Str` - borrow as Str
- [ ] 2.2.2 Implement `to_str(this) -> Str` - copy to new Str
- [ ] 2.2.3 Implement `clone(this) -> Text` - deep copy
- [ ] 2.2.4 Implement `as_bytes(this) -> ref [U8]` - view as bytes
- [ ] 2.2.5 Implement `into_bytes(this) -> [U8]` - consume into bytes

### 2.3 Implicit Conversion
- [ ] 2.3.1 Add Str -> Text coercion in type checker
- [ ] 2.3.2 Generate Str -> Text conversion code
- [ ] 2.3.3 Handle implicit conversion in function calls

## Phase 3: Basic Operations

### 3.1 Capacity and Length
- [ ] 3.1.1 Implement `len(this) -> U64`
- [ ] 3.1.2 Implement `capacity(this) -> U64`
- [ ] 3.1.3 Implement `is_empty(this) -> Bool`
- [ ] 3.1.4 Implement `reserve(mut this, additional: U64)`
- [ ] 3.1.5 Implement `reserve_exact(mut this, additional: U64)`
- [ ] 3.1.6 Implement `shrink_to_fit(mut this)`
- [ ] 3.1.7 Implement `shrink_to(mut this, min_capacity: U64)`

### 3.2 Modification
- [ ] 3.2.1 Implement `clear(mut this)`
- [ ] 3.2.2 Implement `push(mut this, c: U8)`
- [ ] 3.2.3 Implement `push_str(mut this, s: Str)`
- [ ] 3.2.4 Implement `append(mut this, other: ref Text)`
- [ ] 3.2.5 Implement `insert(mut this, idx: U64, c: U8)`
- [ ] 3.2.6 Implement `insert_str(mut this, idx: U64, s: Str)`
- [ ] 3.2.7 Implement `remove(mut this, idx: U64) -> U8`
- [ ] 3.2.8 Implement `pop(mut this) -> Maybe[U8]`
- [ ] 3.2.9 Implement `truncate(mut this, new_len: U64)`

### 3.3 Growth Strategy
- [ ] 3.3.1 Implement capacity growth (2x strategy)
- [ ] 3.3.2 Handle reallocation in push operations
- [ ] 3.3.3 Copy data on reallocation
- [ ] 3.3.4 Free old buffer after reallocation

## Phase 4: Character Access

### 4.1 Basic Access
- [ ] 4.1.1 Implement `char_at(this, idx: U64) -> Maybe[U8]`
- [ ] 4.1.2 Implement `char_code_at(this, idx: U64) -> Maybe[U32]`
- [ ] 4.1.3 Implement `byte_at(this, idx: U64) -> Maybe[U8]`
- [ ] 4.1.4 Implement `first(this) -> Maybe[U8]`
- [ ] 4.1.5 Implement `last(this) -> Maybe[U8]`

### 4.2 Indexing Operator
- [ ] 4.2.1 Implement `[]` operator for Text (read-only)
- [ ] 4.2.2 Return `Maybe[U8]` for bounds safety
- [ ] 4.2.3 Handle out-of-bounds access

## Phase 5: Search Methods

### 5.1 Index Search
- [ ] 5.1.1 Implement `index_of(this, search: Str) -> Maybe[U64]`
- [ ] 5.1.2 Implement `index_of_from(this, search: Str, from: U64) -> Maybe[U64]`
- [ ] 5.1.3 Implement `last_index_of(this, search: Str) -> Maybe[U64]`
- [ ] 5.1.4 Implement `last_index_of_from(this, search: Str, from: U64) -> Maybe[U64]`

### 5.2 Contains
- [ ] 5.2.1 Implement `includes(this, search: Str) -> Bool`
- [ ] 5.2.2 Implement `starts_with(this, prefix: Str) -> Bool`
- [ ] 5.2.3 Implement `ends_with(this, suffix: Str) -> Bool`
- [ ] 5.2.4 Implement `contains_char(this, c: U8) -> Bool`

### 5.3 Pattern Matching
- [ ] 5.3.1 Implement `matches(this, pattern: Str) -> List[U64]` (all match indices)
- [ ] 5.3.2 Implement `count(this, search: Str) -> U64`

## Phase 6: Extraction Methods

### 6.1 Slicing
- [ ] 6.1.1 Implement `slice(this, start: I64, end: I64) -> Text`
- [ ] 6.1.2 Handle negative indices (from end)
- [ ] 6.1.3 Implement `substring(this, start: U64, end: U64) -> Text`
- [ ] 6.1.4 Implement `substr(this, start: U64, length: U64) -> Text`

### 6.2 Character Extraction
- [ ] 6.2.1 Implement `chars(this) -> Iter[U8]` (byte iterator)
- [ ] 6.2.2 Implement `bytes(this) -> Iter[U8]` (byte iterator)

## Phase 7: Transformation Methods

### 7.1 Case Transformation
- [ ] 7.1.1 Implement `to_upper_case(this) -> Text`
- [ ] 7.1.2 Implement `to_lower_case(this) -> Text`
- [ ] 7.1.3 Implement `capitalize(this) -> Text` (first char upper)
- [ ] 7.1.4 Implement `to_title_case(this) -> Text`

### 7.2 Whitespace
- [ ] 7.2.1 Implement `trim(this) -> Text`
- [ ] 7.2.2 Implement `trim_start(this) -> Text`
- [ ] 7.2.3 Implement `trim_end(this) -> Text`
- [ ] 7.2.4 Implement `trim_char(this, c: U8) -> Text`
- [ ] 7.2.5 Implement `normalize_whitespace(this) -> Text` (collapse multiple spaces)

### 7.3 Padding
- [ ] 7.3.1 Implement `pad_start(this, length: U64, pad: Str) -> Text`
- [ ] 7.3.2 Implement `pad_end(this, length: U64, pad: Str) -> Text`
- [ ] 7.3.3 Implement `center(this, length: U64, pad: Str) -> Text`

### 7.4 Replacement
- [ ] 7.4.1 Implement `replace(this, search: Str, replacement: Str) -> Text`
- [ ] 7.4.2 Implement `replace_all(this, search: Str, replacement: Str) -> Text`
- [ ] 7.4.3 Implement `replace_n(this, search: Str, replacement: Str, count: U64) -> Text`
- [ ] 7.4.4 Implement `remove_prefix(this, prefix: Str) -> Text`
- [ ] 7.4.5 Implement `remove_suffix(this, suffix: Str) -> Text`

### 7.5 Other Transformations
- [ ] 7.5.1 Implement `repeat(this, count: U64) -> Text`
- [ ] 7.5.2 Implement `reverse(this) -> Text`

## Phase 8: Split and Join

### 8.1 Splitting
- [ ] 8.1.1 Implement `split(this, separator: Str) -> List[Text]`
- [ ] 8.1.2 Implement `split_n(this, separator: Str, limit: U64) -> List[Text]`
- [ ] 8.1.3 Implement `split_whitespace(this) -> List[Text]`
- [ ] 8.1.4 Implement `lines(this) -> List[Text]`
- [ ] 8.1.5 Implement `split_at(this, idx: U64) -> (Text, Text)`

### 8.2 Joining
- [ ] 8.2.1 Implement `Text::join(parts: List[Text], separator: Str) -> Text`
- [ ] 8.2.2 Implement `Text::concat(parts: List[Text]) -> Text`

## Phase 9: Comparison

### 9.1 Basic Comparison
- [ ] 9.1.1 Implement `compare(this, other: ref Text) -> I32`
- [ ] 9.1.2 Implement `equals(this, other: ref Text) -> Bool`
- [ ] 9.1.3 Implement `compare_ignore_case(this, other: ref Text) -> I32`
- [ ] 9.1.4 Implement `equals_ignore_case(this, other: ref Text) -> Bool`

### 9.2 Operators
- [ ] 9.2.1 Implement `==` operator for Text
- [ ] 9.2.2 Implement `!=` operator for Text
- [ ] 9.2.3 Implement `<`, `<=`, `>`, `>=` operators for Text

## Phase 10: Concatenation Operator

### 10.1 + Operator
- [ ] 10.1.1 Implement `Text + Text -> Text`
- [ ] 10.1.2 Implement `Text + Str -> Text`
- [ ] 10.1.3 Implement `Str + Text -> Text`
- [ ] 10.1.4 Implement `+=` compound assignment

## Phase 11: Formatting

### 11.1 Format Methods
- [ ] 11.1.1 Implement `format(template: Str, args: ...Any) -> Text`
- [ ] 11.1.2 Support `{}` placeholder (positional)
- [ ] 11.1.3 Support `{0}`, `{1}` numbered placeholders
- [ ] 11.1.4 Support `{name}` named placeholders
- [ ] 11.1.5 Support format specifiers `{:.2f}`, `{:08x}`

### 11.2 Number Conversion
- [ ] 11.2.1 Implement `Text::from_int(n: I64) -> Text`
- [ ] 11.2.2 Implement `Text::from_float(n: F64, precision: U32) -> Text`
- [ ] 11.2.3 Implement `Text::from_bool(b: Bool) -> Text`
- [ ] 11.2.4 Implement `parse_int(this) -> Maybe[I64]`
- [ ] 11.2.5 Implement `parse_float(this) -> Maybe[F64]`

## Phase 12: Template Literals

### 12.1 Lexer
- [ ] 12.1.1 Add backtick token type `TemplateLiteral`
- [ ] 12.1.2 Scan template literal content
- [ ] 12.1.3 Parse `{expression}` within template
- [ ] 12.1.4 Handle escape sequences (`\{`, `\n`, `\\`)
- [ ] 12.1.5 Support multi-line template literals
- [ ] 12.1.6 Handle nested templates

### 12.2 Parser
- [ ] 12.2.1 Create `TemplateLiteralExpr` AST node
- [ ] 12.2.2 Parse template parts (string, expression, string, ...)
- [ ] 12.2.3 Parse expressions within `{}`
- [ ] 12.2.4 Handle empty expressions `{}`

### 12.3 Type Checking
- [ ] 12.3.1 Type check embedded expressions
- [ ] 12.3.2 Verify expressions can be converted to string
- [ ] 12.3.3 Infer template literal type as Text

### 12.4 Codegen
- [ ] 12.4.1 Generate Text construction code
- [ ] 12.4.2 Generate expression evaluation code
- [ ] 12.4.3 Generate formatting/conversion calls
- [ ] 12.4.4 Optimize consecutive string parts
- [ ] 12.4.5 Handle expression results (call to_string or equivalent)

## Phase 13: Behavior Implementations

### 13.1 Standard Behaviors
- [ ] 13.1.1 Implement `Display` for Text (printing)
- [ ] 13.1.2 Implement `Debug` for Text (debug output)
- [ ] 13.1.3 Implement `Eq` for Text
- [ ] 13.1.4 Implement `Ord` for Text
- [ ] 13.1.5 Implement `Hash` for Text
- [ ] 13.1.6 Implement `Default` for Text (empty string)
- [ ] 13.1.7 Implement `Duplicate` for Text (deep copy)

### 13.2 Iterator Support
- [ ] 13.2.1 Implement `Iterable[U8]` for Text
- [ ] 13.2.2 Allow Text in for loops

## Phase 14: Runtime Functions

### 14.1 C Runtime Functions
- [ ] 14.1.1 `tml_text_new() -> Text*`
- [ ] 14.1.2 `tml_text_from_str(ptr: *const u8, len: u64) -> Text*`
- [ ] 14.1.3 `tml_text_with_capacity(cap: u64) -> Text*`
- [ ] 14.1.4 `tml_text_drop(text: *mut Text)`
- [ ] 14.1.5 `tml_text_clone(text: *const Text) -> Text*`
- [ ] 14.1.6 `tml_text_push(text: *mut Text, c: u8)`
- [ ] 14.1.7 `tml_text_push_str(text: *mut Text, ptr: *const u8, len: u64)`
- [ ] 14.1.8 `tml_text_reserve(text: *mut Text, additional: u64)`
- [ ] 14.1.9 `tml_text_len(text: *const Text) -> u64`
- [ ] 14.1.10 `tml_text_data(text: *const Text) -> *const u8`

## Phase 15: Standard Library Module

### 15.1 Module Structure
- [ ] 15.1.1 Create `lib/std/src/text/mod.tml`
- [ ] 15.1.2 Create `lib/std/src/text/text.tml` (main implementation)
- [ ] 15.1.3 Create `lib/std/src/text/builder.tml` (TextBuilder helper)
- [ ] 15.1.4 Export Text from `std::text`
- [ ] 15.1.5 Re-export Text from `std` prelude

### 15.2 TextBuilder (Optional)
- [ ] 15.2.1 Implement `TextBuilder` class for efficient concatenation
- [ ] 15.2.2 Implement `append()` method
- [ ] 15.2.3 Implement `build() -> Text` method
- [ ] 15.2.4 Implement `clear()` method

## Phase 16: Testing

### 16.1 Unit Tests
- [ ] 16.1.1 Test constructors
- [ ] 16.1.2 Test basic operations (len, capacity, is_empty)
- [ ] 16.1.3 Test push operations
- [ ] 16.1.4 Test search methods
- [ ] 16.1.5 Test transformation methods
- [ ] 16.1.6 Test split/join
- [ ] 16.1.7 Test comparison
- [ ] 16.1.8 Test operators (+, ==, [])

### 16.2 Template Literal Tests
- [ ] 16.2.1 Test basic interpolation `{x}`
- [ ] 16.2.2 Test expression interpolation `{x + y}`
- [ ] 16.2.3 Test method call interpolation `{x.to_upper_case()}`
- [ ] 16.2.4 Test multi-line templates
- [ ] 16.2.5 Test escape sequences
- [ ] 16.2.6 Test nested templates

### 16.3 Integration Tests
- [ ] 16.3.1 HTML builder example
- [ ] 16.3.2 JSON builder example
- [ ] 16.3.3 Log message formatting example
- [ ] 16.3.4 CSV parser/builder example

### 16.4 Performance Tests
- [ ] 16.4.1 Benchmark concatenation vs Str
- [ ] 16.4.2 Benchmark template literals vs format()
- [ ] 16.4.3 Benchmark SSO performance
- [ ] 16.4.4 Benchmark memory usage

## Phase 17: Documentation

### 17.1 API Reference
- [ ] 17.1.1 Document all constructors
- [ ] 17.1.2 Document all methods with examples
- [ ] 17.1.3 Document operator usage
- [ ] 17.1.4 Document template literal syntax

### 17.2 Guide
- [ ] 17.2.1 When to use Str vs Text
- [ ] 17.2.2 Performance considerations
- [ ] 17.2.3 Common patterns and idioms
- [ ] 17.2.4 Migration from Str to Text

## Validation

- [ ] V.1 Text type recognized by type system
- [ ] V.2 All constructors work correctly
- [ ] V.3 All methods produce correct results
- [ ] V.4 Template literals compile and run correctly
- [ ] V.5 Operators work correctly
- [ ] V.6 Memory is properly managed (no leaks)
- [ ] V.7 SSO works for small strings
- [ ] V.8 Existing Str code still works unchanged
- [ ] V.9 Performance is acceptable
- [ ] V.10 All tests pass

## Summary

| Phase | Description | Status | Progress |
|-------|-------------|--------|----------|
| 1 | Core Type Definition | Not Started | 0/13 |
| 2 | Constructors and Conversion | Not Started | 0/13 |
| 3 | Basic Operations | Not Started | 0/16 |
| 4 | Character Access | Not Started | 0/8 |
| 5 | Search Methods | Not Started | 0/11 |
| 6 | Extraction Methods | Not Started | 0/6 |
| 7 | Transformation Methods | Not Started | 0/16 |
| 8 | Split and Join | Not Started | 0/7 |
| 9 | Comparison | Not Started | 0/8 |
| 10 | Concatenation Operator | Not Started | 0/4 |
| 11 | Formatting | Not Started | 0/10 |
| 12 | Template Literals | Not Started | 0/14 |
| 13 | Behavior Implementations | Not Started | 0/9 |
| 14 | Runtime Functions | Not Started | 0/10 |
| 15 | Standard Library Module | Not Started | 0/7 |
| 16 | Testing | Not Started | 0/16 |
| 17 | Documentation | Not Started | 0/8 |
| **Total** | | **Not Started** | **0/166** |

## Files to Create/Modify

### New Files
- `lib/std/src/text/mod.tml` - Text module
- `lib/std/src/text/text.tml` - Text implementation
- `lib/std/src/text/builder.tml` - TextBuilder helper
- `lib/std/tests/text.test.tml` - Text tests
- `compiler/runtime/text.c` - Text runtime functions
- `docs/rfcs/RFC-0015-TEXT-TYPE.md` - Text specification

### Modified Files
- `compiler/include/lexer/token.hpp` - Add template literal token
- `compiler/src/lexer/lexer_core.cpp` - Lex template literals
- `compiler/include/parser/ast.hpp` - Add TemplateLiteralExpr
- `compiler/src/parser/parser_expr.cpp` - Parse template literals
- `compiler/include/types/type.hpp` - Add TextType
- `compiler/src/types/checker/expr.cpp` - Type check template literals
- `compiler/src/codegen/expr/literal.cpp` - Generate template literal code
- `compiler/runtime/essential.c` - Add text runtime functions
- `lib/std/src/mod.tml` - Export text module

## Dependencies

- None (standalone feature)

## Estimated Complexity

- **Lexer**: Medium - template literal parsing with interpolation
- **Parser**: Medium - expression parsing within templates
- **Type System**: Low - Text is similar to existing types
- **Codegen**: Medium - template expansion and formatting
- **Runtime**: Medium - memory management for growable strings
- **Standard Library**: High - many methods to implement
