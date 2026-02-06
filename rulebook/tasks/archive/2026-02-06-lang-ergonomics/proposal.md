# Language Ergonomics Improvements

**Task ID:** lang-ergonomics
**Status:** Proposed
**Priority:** High
**Created:** 2026-02-04

## Summary

This task addresses several language ergonomics issues discovered during the implementation of `std::crypto::cipher`. These issues required extensive workarounds and significantly increased code verbosity. Implementing these features will make TML more practical for real-world use.

## Background

During the cipher module implementation, several language limitations were encountered:

1. Error propagation required 6+ lines of boilerplate per fallible call
2. Pattern matching on strings generated invalid LLVM IR
3. Field access on reference types had codegen bugs
4. Method calls on references didn't auto-dereference
5. The `!` (never) type didn't coerce properly in expressions

## Proposed Changes

### 1. Try Operator `?` for Error Propagation

**Priority:** Critical
**Impact:** Eliminates ~80% of error handling boilerplate

#### Current Behavior (Not Implemented)
```tml
// Documentation shows this syntax, but it doesn't work
let algorithm = CipherAlgorithm::aes_cbc_for_key_len(key_len)?
```

#### Required Workaround (35 lines for 5 fallible calls)
```tml
let algorithm_result: Outcome[CipherAlgorithm, CryptoError] = CipherAlgorithm::aes_cbc_for_key_len(key_len)
let mut algorithm: CipherAlgorithm = CipherAlgorithm::Aes128Cbc  // placeholder
when algorithm_result {
    Err(e) => return Err(e),
    Ok(a) => algorithm = a
}
```

#### Proposed Implementation

**Syntax:**
```tml
let value = fallible_call()?
```

**Semantics:**
- `expr?` on `Outcome[T, E]` returns `T` if `Ok`, early-returns `Err(e)` if `Err`
- `expr?` on `Maybe[T]` returns `T` if `Just`, early-returns `Nothing` if `Nothing`
- Only valid in functions returning `Outcome[_, E]` or `Maybe[_]`

**Codegen Strategy:**
```
; For: let x = foo()?
%result = call %Outcome @foo()
%tag = extractvalue %Outcome %result, 0
%is_err = icmp eq i32 %tag, 1
br i1 %is_err, label %early_return, label %continue

early_return:
  %err = extractvalue %Outcome %result, 1
  ; construct Err variant for return type
  ret %ReturnType { tag: 1, value: %err }

continue:
  %x = extractvalue %Outcome %result, 1
```

**Files to Modify:**
- `compiler/src/parser/parser.cpp` - Parse `?` as postfix operator
- `compiler/src/types/checker.cpp` - Type check try expressions
- `compiler/src/hir/hir.hpp` - Add `TryExpr` HIR node
- `compiler/src/codegen/expr/try.cpp` - New file for codegen

---

### 2. Never Type (`!`) Coercion in Expressions

**Priority:** High
**Impact:** Enables `when` expressions with early returns

#### Current Behavior (Bug)
```tml
// Error: Type mismatch: expected CipherAlgorithm, found !
let algorithm: CipherAlgorithm = when result {
    Err(e) => return Err(e),  // type: !
    Ok(a) => a                 // type: CipherAlgorithm
}
```

#### Expected Behavior
The `!` type should coerce to any type since the branch never produces a value.

#### Proposed Implementation

**Type System Rule:**
- `!` is a subtype of all types (bottom type)
- In type unification: `unify(!, T) = T` for any `T`

**Files to Modify:**
- `compiler/src/types/checker.cpp` - Add never type coercion
- `compiler/src/types/unify.cpp` - Handle `!` in unification

**Implementation:**
```cpp
// In type unification
Type* unify(Type* a, Type* b) {
    if (a->is_never()) return b;
    if (b->is_never()) return a;
    // ... existing unification logic
}
```

---

### 3. String Pattern Matching in `when`

**Priority:** Medium
**Impact:** Cleaner parsing/matching code

#### Current Behavior (Bug)
```tml
// Generates invalid LLVM IR - treats string as enum discriminant
when name {
    "aes-128-cbc" => return Just(CipherAlgorithm::Aes128Cbc)
    _ => return Nothing
}
```

**Error:** `invalid getelementptr indices` - compiler tries to access discriminant field on string pointer

#### Required Workaround
```tml
if name == "aes-128-cbc" { return Just(CipherAlgorithm::Aes128Cbc) }
if name == "aes-192-cbc" { return Just(CipherAlgorithm::Aes192Cbc) }
// ... 20 more if statements
return Nothing
```

#### Proposed Implementation

**Strategy:** Desugar string `when` to chained `if-else`:

```tml
// Source
when name {
    "foo" => expr1,
    "bar" => expr2,
    _ => expr3
}

// Desugared to
if name == "foo" { expr1 }
else if name == "bar" { expr2 }
else { expr3 }
```

**Files to Modify:**
- `compiler/src/hir/lower.cpp` - Desugar string when in HIR lowering
- `compiler/src/types/checker.cpp` - Detect string when patterns

**Alternative:** Generate jump table with string hash comparison for performance.

---

### 4. Auto-Dereference for Method Calls on References

**Priority:** Medium
**Impact:** Natural method call syntax on references

#### Current Behavior (Bug)
```tml
// Buffer.len(this) takes ownership, but key is ref Buffer
pub func new(key: ref Buffer) {
    if key.len() != 32 { ... }  // Error: returns () instead of I64
}
```

#### Required Workaround
```tml
let key_len: I64 = ffi_buffer_len(key.handle)
if key_len != 32 { ... }
```

#### Proposed Implementation

**Rule:** When calling `obj.method()` where:
- `obj` has type `ref T`
- `method` is defined as `func method(this: T)` or `func method(this: ref T)`

The compiler should automatically dereference or pass the reference.

**Priority for method resolution:**
1. Exact match: `ref T` has method taking `ref this`
2. Auto-deref: `ref T` → `T`, call method taking `this`
3. Auto-ref: `T` → `ref T`, call method taking `ref this`

**Files to Modify:**
- `compiler/src/types/method_resolution.cpp` - Add auto-deref logic
- `compiler/src/codegen/expr/method.cpp` - Generate deref if needed

---

### 5. Chained Field Access on Reference Types

**Priority:** Medium
**Impact:** Fix codegen bug for nested field access

#### Current Behavior (Bug)
```tml
// tag: ref AuthTag, AuthTag has field data: Buffer, Buffer has field handle: *Unit
ffi_set_tag(tag.data.handle)  // Generates invalid LLVM IR
```

**Error:** `invalid getelementptr indices` - incorrect pointer arithmetic

#### Required Workaround
```tml
let tag_data: ref Buffer = ref tag.data
ffi_set_tag(tag_data.handle)
```

#### Root Cause Analysis

The codegen for `tag.data.handle` where `tag: ref AuthTag`:
1. `tag` is `ptr` to `AuthTag`
2. `tag.data` should: load ptr, GEP to `data` field
3. `tag.data.handle` should: GEP to `handle` field of `Buffer`

Current bug: Trying to GEP on the loaded pointer as if it's a struct.

**Files to Modify:**
- `compiler/src/codegen/expr/field.cpp` - Fix chained field access on refs

---

## Implementation Order

| Phase | Feature | Effort | Dependencies |
|-------|---------|--------|--------------|
| 1 | Never type coercion | Small | None |
| 2 | Chained ref field access | Small | None |
| 3 | Auto-deref methods | Medium | None |
| 4 | String when patterns | Medium | None |
| 5 | Try operator `?` | Large | Never type coercion |

## Testing Strategy

Each feature should include:
1. Unit tests in `compiler/tests/`
2. Integration tests in `lib/test/tests/`
3. Verify existing crypto tests still pass

## Success Criteria

After implementation, the cipher.tml file should be refactorable to:
```tml
pub func aes_encrypt(key: ref Buffer, iv: ref Buffer, plaintext: ref Buffer) -> Outcome[Buffer, CryptoError] {
    let algorithm = CipherAlgorithm::aes_cbc_for_key_len(key.len())?
    let mut cipher = Cipher::new(algorithm, key, iv)?
    cipher.update_bytes(plaintext)
    return cipher.finalize()
}
```

Down from current 20+ lines with workarounds.

---

## Additional Proposed Features

### 6. `if let` Pattern Matching

**Priority:** Medium
**Impact:** Concise single-pattern matching without full `when`

#### Current Behavior
```tml
// Must use full when for single pattern
let result: Maybe[I64] = get_value()
when result {
    Just(v) => use_value(v),
    Nothing => {}  // empty branch required
}
```

#### Proposed Syntax
```tml
// Concise pattern matching
if let Just(v) = get_value() {
    use_value(v)
}

// With else
if let Ok(data) = fetch_data() {
    process(data)
} else {
    handle_error()
}
```

#### Proposed `let else` for Early Exit
```tml
// Extract or early return
let Just(config) = load_config() else {
    return Err(ConfigError::NotFound)
}
// config is now available
```

**Files to Modify:**
- `compiler/src/parser/parser.cpp` - Parse `if let` and `let else`
- `compiler/src/hir/hir.hpp` - Add `IfLetExpr` and `LetElseStmt` nodes
- `compiler/src/types/checker.cpp` - Type check pattern bindings
- `compiler/src/codegen/expr/` - Generate match code

---

### 7. Default Struct Field Values

**Priority:** Low
**Impact:** Reduce boilerplate in struct initialization

#### Current Behavior
```tml
// Must provide all fields, even for common defaults
type Config {
    timeout: I64
    retries: I64
    debug: Bool
}

let cfg: Config = Config { timeout: 30, retries: 3, debug: false }
```

#### Proposed Syntax
```tml
type Config {
    timeout: I64 = 30
    retries: I64 = 3
    debug: Bool = false
}

// Only specify non-default values
let cfg: Config = Config { debug: true }
```

**Files to Modify:**
- `compiler/src/parser/parser.cpp` - Parse default values in struct fields
- `compiler/src/types/checker.cpp` - Validate defaults match field types
- `compiler/src/codegen/expr/struct_literal.cpp` - Fill in default values

---

### 8. Implicit Returns (Last Expression)

**Priority:** Low
**Impact:** Reduce `return` keyword verbosity

#### Current Behavior
```tml
func add(a: I64, b: I64) -> I64 {
    return a + b  // return keyword required
}

func classify(n: I64) -> Str {
    if n < 0 {
        return "negative"
    } else {
        return "non-negative"
    }
}
```

#### Proposed Syntax
```tml
func add(a: I64, b: I64) -> I64 {
    a + b  // implicit return
}

func classify(n: I64) -> Str {
    if n < 0 {
        "negative"
    } else {
        "non-negative"
    }
}
```

**Semantics:**
- Last expression in a block becomes implicit return if no semicolon
- `return` keyword still works for early returns
- Only applies to expression-bodied functions

**Files to Modify:**
- `compiler/src/parser/parser.cpp` - Handle expression without semicolon as return
- `compiler/src/hir/lower.cpp` - Convert to explicit return in HIR
- `compiler/src/types/checker.cpp` - Infer return type from last expression

---

### 9. Better Type Inference for Variable Declarations

**Priority:** Medium
**Impact:** Reduce verbose type annotations

#### Current Behavior
```tml
// Type annotations required on nearly all declarations
let key_len: I64 = ffi_buffer_len(key.handle)
let algorithm_result: Outcome[CipherAlgorithm, CryptoError] = CipherAlgorithm::aes_cbc_for_key_len(key_len)
let mut algorithm: CipherAlgorithm = CipherAlgorithm::Aes128Cbc
```

#### Expected Behavior
```tml
// Types inferred from right-hand side
let key_len = ffi_buffer_len(key.handle)           // infers I64
let algorithm_result = CipherAlgorithm::aes_cbc_for_key_len(key_len)  // infers Outcome[...]
let mut algorithm = CipherAlgorithm::Aes128Cbc     // infers CipherAlgorithm
```

**Files to Modify:**
- `compiler/src/types/checker.cpp` - Improve bidirectional type inference
- `compiler/src/types/infer.cpp` - Add constraint-based inference

---

### 10. Late Initialization for Variables

**Priority:** Low
**Impact:** Cleaner code when all branches assign

#### Current Behavior
```tml
// Must use mutable with placeholder
let mut value: I64 = 0  // placeholder needed
if condition {
    value = compute_a()
} else {
    value = compute_b()
}
```

#### Proposed Syntax
```tml
// Declare without initialization
let value: I64
if condition {
    value = compute_a()
} else {
    value = compute_b()
}
// Compiler verifies all paths assign before use
```

**Semantics:**
- Variable declared without `=` is uninitialized
- Compiler performs definite assignment analysis
- Error if any path uses variable before assignment
- Error if any path doesn't assign (unless function returns early)

**Files to Modify:**
- `compiler/src/parser/parser.cpp` - Allow `let x: T` without initializer
- `compiler/src/types/checker.cpp` - Add definite assignment analysis
- `compiler/src/codegen/` - Handle uninitialized allocas

---

### 11. Method Chaining with Try Operator

**Priority:** Low (depends on Phase 5)
**Impact:** Fluent API patterns

#### Proposed Syntax
```tml
// Chain multiple fallible operations
let result = fetch_data()?
    .parse()?
    .validate()?
    .transform()

// Combined with builder pattern
let cipher = Cipher::builder()
    .algorithm(CipherAlgorithm::Aes256Gcm)?
    .key(key)?
    .iv(iv)?
    .build()?
```

This works naturally once `?` is implemented, but requires:
- Methods returning `Outcome[Self, E]` or `Outcome[ref Self, E]`
- Proper type inference through the chain

---

### 12. Tuple Destructuring in Patterns

**Priority:** Medium
**Impact:** Cleaner tuple handling

#### Current Behavior
```tml
let result: (I64, Str) = get_pair()
let count: I64 = result.0
let name: Str = result.1
```

#### Proposed Syntax
```tml
let (count, name) = get_pair()

// In when patterns
when get_result() {
    Ok((a, b)) => use_both(a, b),
    Err(e) => handle(e)
}

// In function parameters
func process_pair((x, y): (I64, I64)) -> I64 {
    x + y
}
```

**Files to Modify:**
- `compiler/src/parser/parser.cpp` - Parse tuple patterns
- `compiler/src/types/checker.cpp` - Type check destructuring
- `compiler/src/codegen/` - Generate extractvalue for each element

---

## Updated Implementation Order

| Phase | Feature | Effort | Dependencies |
|-------|---------|--------|--------------|
| 1 | Never type coercion | Small | None |
| 2 | Chained ref field access | Small | None |
| 3 | Auto-deref methods | Medium | None |
| 4 | String when patterns | Medium | None |
| 5 | Try operator `?` | Large | Never type coercion |
| 6 | `if let` / `let else` | Medium | None |
| 7 | Better type inference | Medium | None |
| 8 | Tuple destructuring | Medium | None |
| 9 | Default struct values | Small | None |
| 10 | Late initialization | Medium | None |
| 11 | Implicit returns | Small | None |
| 12 | Method chaining | Small | Phase 5 |
