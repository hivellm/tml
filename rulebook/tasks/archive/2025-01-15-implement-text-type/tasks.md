# Tasks: Text Type Implementation

**Status**: ✅ Complete

**Priority**: High - Core language feature for string manipulation

## Phase 1: Core Type Definition ✅ COMPLETE

### 1.1 Type System ✅
- [x] 1.1.1 Add `Text` to primitive types in `types/type.hpp`
- [x] 1.1.2 Add `TextType` variant to type kinds
- [x] 1.1.3 Register Text type in type environment
- [x] 1.1.4 Implement Text type checking rules

### 1.2 Memory Layout ✅
- [x] 1.2.1 Define Text struct in runtime (`data: *mut u8, len: u64, cap: u64`)
- [x] 1.2.2 Implement LLVM struct type for Text
- [x] 1.2.3 Add Text allocation function in runtime
- [x] 1.2.4 Add Text deallocation function in runtime
- [x] 1.2.5 Implement drop glue for Text

### 1.3 SSO (Small String Optimization) ✅
- [x] 1.3.1 Define SSO threshold (23 bytes)
- [x] 1.3.2 Implement inline storage for small strings
- [x] 1.3.3 Add is_inline() check
- [x] 1.3.4 Handle SSO in allocation/deallocation

## Phase 2: Constructors and Conversion ✅ COMPLETE

### 2.1 Constructors ✅
- [x] 2.1.1 Implement `Text::new()` - empty text
- [x] 2.1.2 Implement `Text::from(s: Str)` - from string literal
- [x] 2.1.3 Implement `Text::with_capacity(cap: I64)` - pre-allocated
- [x] 2.1.4 Implement `Text::from_i64(n: I64)` - from integer
- [x] 2.1.5 Implement `Text::from_f64(n: F64)` - from float
- [x] 2.1.6 Implement `Text::from_f64_precision(n: F64, p: I32)` - with precision
- [x] 2.1.7 Implement `Text::from_bool(b: Bool)` - from boolean

### 2.2 Conversion Methods ✅
- [x] 2.2.1 Implement `as_str(this) -> Str` - borrow as Str
- [x] 2.2.2 Implement `clone(this) -> Text` - deep copy
- [x] 2.2.3 Implement `drop(this)` - free memory

## Phase 3: Basic Operations ✅ COMPLETE

### 3.1 Capacity and Length ✅
- [x] 3.1.1 Implement `len(this) -> I64`
- [x] 3.1.2 Implement `capacity(this) -> I64`
- [x] 3.1.3 Implement `is_empty(this) -> Bool`
- [x] 3.1.4 Implement `reserve(this, additional: I64)`
- [x] 3.1.5 Implement `byte_at(this, idx: I64) -> I32`

### 3.2 Modification ✅
- [x] 3.2.1 Implement `clear(this)`
- [x] 3.2.2 Implement `push(this, c: I32)`
- [x] 3.2.3 Implement `push_str(this, s: Str)`

### 3.3 Growth Strategy ✅
- [x] 3.3.1 Implement capacity growth (2x strategy until 4KB, then 1.5x)
- [x] 3.3.2 Handle reallocation in push operations
- [x] 3.3.3 Copy data on reallocation
- [x] 3.3.4 Free old buffer after reallocation

## Phase 4: Search Methods ✅ COMPLETE

### 4.1 Index Search ✅
- [x] 4.1.1 Implement `index_of(this, search: Str) -> I64`
- [x] 4.1.2 Implement `last_index_of(this, search: Str) -> I64`

### 4.2 Contains ✅
- [x] 4.2.1 Implement `includes(this, search: Str) -> Bool`
- [x] 4.2.2 Implement `contains(this, search: Str) -> Bool` (alias)
- [x] 4.2.3 Implement `starts_with(this, prefix: Str) -> Bool`
- [x] 4.2.4 Implement `ends_with(this, suffix: Str) -> Bool`

## Phase 5: Transformation Methods ✅ COMPLETE

### 5.1 Case Transformation ✅
- [x] 5.1.1 Implement `to_upper_case(this) -> Text`
- [x] 5.1.2 Implement `to_lower_case(this) -> Text`

### 5.2 Whitespace ✅
- [x] 5.2.1 Implement `trim(this) -> Text`
- [x] 5.2.2 Implement `trim_start(this) -> Text`
- [x] 5.2.3 Implement `trim_end(this) -> Text`

### 5.3 Slicing ✅
- [x] 5.3.1 Implement `substring(this, start: I64, end: I64) -> Text`
- [x] 5.3.2 Implement `slice(this, start: I64, end: I64) -> Text` (alias)

### 5.4 Padding ✅
- [x] 5.4.1 Implement `pad_start(this, length: I64, pad_char: I32) -> Text`
- [x] 5.4.2 Implement `pad_end(this, length: I64, pad_char: I32) -> Text`

### 5.5 Replacement ✅
- [x] 5.5.1 Implement `replace(this, search: Str, replacement: Str) -> Text`
- [x] 5.5.2 Implement `replace_all(this, search: Str, replacement: Str) -> Text`

### 5.6 Other Transformations ✅
- [x] 5.6.1 Implement `repeat(this, count: I64) -> Text`
- [x] 5.6.2 Implement `reverse(this) -> Text`

## Phase 6: Concatenation ✅ COMPLETE

### 6.1 Concatenation Methods ✅
- [x] 6.1.1 Implement `concat(this, other: ref Text) -> Text`
- [x] 6.1.2 Implement `concat_str(this, s: Str) -> Text`

## Phase 7: Comparison ✅ COMPLETE

### 7.1 Comparison Methods ✅
- [x] 7.1.1 Implement `compare(this, other: ref Text) -> I32`
- [x] 7.1.2 Implement `equals(this, other: ref Text) -> Bool`

## Phase 8: Output ✅ COMPLETE

### 8.1 Print Methods ✅
- [x] 8.1.1 Implement `print(this)` - no newline
- [x] 8.1.2 Implement `println(this)` - with newline

## Phase 9: Runtime Functions ✅ COMPLETE

### 9.1 C Runtime Functions (compiler/runtime/text.c) ✅
- [x] 9.1.1 `tml_text_new() -> TmlText*`
- [x] 9.1.2 `tml_text_from_str(data: *const u8, len: u64) -> TmlText*`
- [x] 9.1.3 `tml_text_with_capacity(cap: i64) -> TmlText*`
- [x] 9.1.4 `tml_text_drop(text: *mut TmlText)`
- [x] 9.1.5 `tml_text_clone(text: *const TmlText) -> TmlText*`
- [x] 9.1.6 `tml_text_push(text: *mut TmlText, c: i32)`
- [x] 9.1.7 `tml_text_push_str(text: *mut TmlText, data: *const u8, len: u64)`
- [x] 9.1.8 `tml_text_reserve(text: *mut TmlText, additional: i64)`
- [x] 9.1.9 `tml_text_len(text: *const TmlText) -> i64`
- [x] 9.1.10 `tml_text_capacity(text: *const TmlText) -> i64`
- [x] 9.1.11 `tml_text_is_empty(text: *const TmlText) -> i32`
- [x] 9.1.12 `tml_text_byte_at(text: *const TmlText, idx: i64) -> i32`
- [x] 9.1.13 `tml_text_clear(text: *mut TmlText)`
- [x] 9.1.14 `tml_text_as_cstr(text: *const TmlText) -> *const u8`
- [x] 9.1.15 `tml_text_index_of(text: *const TmlText, search: *const u8) -> i64`
- [x] 9.1.16 `tml_text_last_index_of(text: *const TmlText, search: *const u8) -> i64`
- [x] 9.1.17 `tml_text_starts_with(text: *const TmlText, prefix: *const u8) -> i32`
- [x] 9.1.18 `tml_text_ends_with(text: *const TmlText, suffix: *const u8) -> i32`
- [x] 9.1.19 `tml_text_contains(text: *const TmlText, search: *const u8) -> i32`
- [x] 9.1.20 `tml_text_to_upper(text: *const TmlText) -> TmlText*`
- [x] 9.1.21 `tml_text_to_lower(text: *const TmlText) -> TmlText*`
- [x] 9.1.22 `tml_text_trim(text: *const TmlText) -> TmlText*`
- [x] 9.1.23 `tml_text_trim_start(text: *const TmlText) -> TmlText*`
- [x] 9.1.24 `tml_text_trim_end(text: *const TmlText) -> TmlText*`
- [x] 9.1.25 `tml_text_substring(text: *const TmlText, start: i64, end: i64) -> TmlText*`
- [x] 9.1.26 `tml_text_repeat(text: *const TmlText, count: i64) -> TmlText*`
- [x] 9.1.27 `tml_text_replace(text: *const TmlText, search, replacement) -> TmlText*`
- [x] 9.1.28 `tml_text_replace_all(text: *const TmlText, search, replacement) -> TmlText*`
- [x] 9.1.29 `tml_text_reverse(text: *const TmlText) -> TmlText*`
- [x] 9.1.30 `tml_text_pad_start(text: *const TmlText, len: i64, pad: i32) -> TmlText*`
- [x] 9.1.31 `tml_text_pad_end(text: *const TmlText, len: i64, pad: i32) -> TmlText*`
- [x] 9.1.32 `tml_text_concat(t1: *const TmlText, t2: *const TmlText) -> TmlText*`
- [x] 9.1.33 `tml_text_concat_str(text: *const TmlText, s: *const u8) -> TmlText*`
- [x] 9.1.34 `tml_text_compare(t1: *const TmlText, t2: *const TmlText) -> i32`
- [x] 9.1.35 `tml_text_equals(t1: *const TmlText, t2: *const TmlText) -> i32`
- [x] 9.1.36 `tml_text_print(text: *const TmlText)`
- [x] 9.1.37 `tml_text_println(text: *const TmlText)`
- [x] 9.1.38 `tml_text_from_i64(value: i64) -> TmlText*`
- [x] 9.1.39 `tml_text_from_f64(value: f64, precision: i32) -> TmlText*`
- [x] 9.1.40 `tml_text_from_bool(value: i32) -> TmlText*`

## Phase 10: Standard Library Module ✅ COMPLETE

### 10.1 Module Structure ✅
- [x] 10.1.1 Create `lib/std/src/text.tml` - single-file module
- [x] 10.1.2 Export Text from `std::text`
- [x] 10.1.3 Add documentation following Rust standard

## Phase 11: Testing ✅ COMPLETE

### 11.1 TML Unit Tests (lib/std/tests/text.test.tml) ✅
- [x] 11.1.1 Constructor tests (18 tests)
- [x] 11.1.2 Conversion tests (4 tests)
- [x] 11.1.3 Properties tests (10 tests)
- [x] 11.1.4 Modification tests (8 tests)
- [x] 11.1.5 Search tests (15 tests)
- [x] 11.1.6 Transformation tests (44 tests)
- [x] 11.1.7 Concatenation tests (6 tests)
- [x] 11.1.8 Comparison tests (8 tests)
- [x] 11.1.9 SSO tests (5 tests)
- [x] 11.1.10 Chained operations tests (3 tests)
- [x] 11.1.11 Edge case tests (4 tests)
- [x] 11.1.12 Empty text tests (4 tests)
- [x] 11.1.13 Stress tests (3 tests)

**Total TML Tests: 134 tests**

### 11.2 C++ Codegen Tests (compiler/tests/text_test.cpp) ✅
- [x] 11.2.1 Constructor codegen tests (6 tests)
- [x] 11.2.2 Properties codegen tests (4 tests)
- [x] 11.2.3 Modification codegen tests (4 tests)
- [x] 11.2.4 Search codegen tests (5 tests)
- [x] 11.2.5 Transformation codegen tests (12 tests)
- [x] 11.2.6 Concatenation codegen tests (2 tests)
- [x] 11.2.7 Comparison codegen tests (2 tests)
- [x] 11.2.8 Clone codegen tests (1 test)
- [x] 11.2.9 Conversion codegen tests (1 test)
- [x] 11.2.10 Output codegen tests (2 tests)
- [x] 11.2.11 Integration codegen tests (3 tests)

**Total C++ Tests: 42 tests**

## Phase 12: Template Literals ✅ COMPLETE

### 12.1 Lexer ✅
- [x] 12.1.1 Add backtick token types (`TemplateLiteralStart`, `TemplateLiteralMiddle`, `TemplateLiteralEnd`)
- [x] 12.1.2 Scan template literal content
- [x] 12.1.3 Parse `{expression}` within template
- [x] 12.1.4 Handle escape sequences (`\{`, `\n`, `\\`)
- [x] 12.1.5 Support multi-line template literals
- [x] 12.1.6 Handle nested templates

### 12.2 Parser ✅
- [x] 12.2.1 Create `TemplateLiteralExpr` AST node
- [x] 12.2.2 Parse template parts (string, expression, string, ...)
- [x] 12.2.3 Parse expressions within `{}`
- [x] 12.2.4 Handle empty expressions `{}`

### 12.3 Type Checking ✅
- [x] 12.3.1 Type check embedded expressions
- [x] 12.3.2 Verify expressions can be converted to string
- [x] 12.3.3 Infer template literal type as Text

### 12.4 Codegen ✅
- [x] 12.4.1 Generate Text construction code
- [x] 12.4.2 Generate expression evaluation code
- [x] 12.4.3 Generate formatting/conversion calls
- [x] 12.4.4 Optimize consecutive string parts
- [x] 12.4.5 Handle expression results (call to_string or equivalent)

## Validation ✅ COMPLETE

- [x] V.1 Text type recognized by type system
- [x] V.2 All constructors work correctly
- [x] V.3 All methods produce correct results
- [x] V.4 Template literals compile and run correctly
- [x] V.5 Memory is properly managed (no leaks)
- [x] V.6 SSO works for small strings (<=23 bytes)
- [x] V.7 Existing Str code still works unchanged
- [x] V.8 All TML tests pass (134/134)
- [x] V.9 Performance is acceptable

## Summary

| Phase | Description | Status | Progress |
|-------|-------------|--------|----------|
| 1 | Core Type Definition | ✅ Complete | 13/13 |
| 2 | Constructors and Conversion | ✅ Complete | 10/10 |
| 3 | Basic Operations | ✅ Complete | 12/12 |
| 4 | Search Methods | ✅ Complete | 6/6 |
| 5 | Transformation Methods | ✅ Complete | 12/12 |
| 6 | Concatenation | ✅ Complete | 2/2 |
| 7 | Comparison | ✅ Complete | 2/2 |
| 8 | Output | ✅ Complete | 2/2 |
| 9 | Runtime Functions | ✅ Complete | 40/40 |
| 10 | Standard Library Module | ✅ Complete | 3/3 |
| 11 | Testing | ✅ Complete | 176/176 |
| 12 | Template Literals | ✅ Complete | 14/14 |
| **Total** | | **✅ Complete** | **132/132** |

## Files Created/Modified

### Created Files
- `compiler/runtime/text.c` - Text runtime implementation (40 functions)
- `lib/std/src/text.tml` - Text TML module
- `lib/std/tests/text.test.tml` - TML tests (134 tests)
- `compiler/tests/text_test.cpp` - C++ codegen tests (42 tests)

### Modified Files
- `compiler/include/codegen/llvm_ir_gen.hpp` - Text runtime declarations
- `compiler/src/codegen/llvm_ir_gen_builtins.cpp` - Text builtin codegen
- `lib/std/src/mod.tml` - Export text module

## Notes

### SSO (Small String Optimization)
- Strings <= 23 bytes are stored inline without heap allocation
- Threshold chosen to fit in 24 bytes (3 pointers) minus 1 for length
- Significant performance improvement for short strings

### Growth Strategy
- Initial capacity: 32 bytes
- Double capacity until 4KB
- Then grow by 50%
- Provides amortized O(1) for push operations

### Memory Safety
- All functions handle null/empty text gracefully
- Bounds checking on all index operations
- Proper cleanup on drop

### Remaining Work (Future Enhancements)
- Operator overloading (+ for concatenation)
- Additional string methods (split, join, etc.)
