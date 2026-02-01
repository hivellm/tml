# Proposal: Codegen & Type System Test Blockers

## Overview

This task tracks all compiler and codegen issues discovered while creating tests for library coverage. These issues prevent tests from running and block coverage improvements.

## Source Files

Issues were discovered in these test files:
- `lib/core/tests/convert/convert.test.tml`
- `lib/core/tests/error/error.test.tml`
- `lib/core/tests/fmt/fmt_impls.test.tml`
- `lib/core/tests/ops/*.test.tml`
- `lib/core/tests/num/*.test.tml`
- `compiler/tests/borrow/*.test.tml`
- `compiler/tests/compiler/*.test.tml`

---

## Phase 1: Critical Codegen Bugs

### 1.1.1 Cross-type From conversions

**File**: `convert.test.tml:15`

From behavior implementations for type widening (I8→I16, I16→I32, etc) type-check but fail at runtime. The generated LLVM IR has incorrect type handling.

```tml
let x: I8 = 42
let y: I16 = I16::from(x)  // Crashes at runtime
```

### 1.1.2 U64 large literals

**File**: `fmt_impls.test.tml:86`

Large U64 literals cause LLVM IR generation errors.

```tml
let x: U64 = 10000000000  // Codegen error
```

### 1.1.3 Ordering enum to_string

**File**: `fmt_impls.test.tml:128`

Calling `to_string()` on Ordering enum generates invalid LLVM IR:
```
error: invalid cast opcode for cast from 'i64' to 'i64'
  %t104 = zext i64 %t102 to i64
```

### 1.1.4 I8/I16 MIN codegen bug

**Files**: `bit_manipulation.test.tml:2`, `num.test.tml:363`

I8::MIN (-128) and I16::MIN (-32768) constants generate incorrect code.

### 1.1.5 I8 negative return value

**File**: `num.test.tml:2`

Functions returning negative I8 values have codegen issues.

### 1.1.6 F32 float/double promotion

**File**: `num.test.tml:270`

F32 operations incorrectly promote to double causing test failures.

### 1.1.7 Bool variable codegen

**Files**: `strings.test.tml:117`, `memory.test.tml:5`

Bool variables in certain contexts generate incorrect code.

### 1.1.8 U8/U16 bitwise coercion

**File**: `operators.test.tml:266`

Bitwise operations on U8/U16 have type coercion limitations.

---

## Phase 2: Generic Type System Issues

### 2.1.1 Generic Range types

**File**: `range.test.tml:6`

Generic Range[T] types are blocked by a compiler bug in generic type instantiation.

### 2.1.2-2.1.3 Generic enum variants

**Files**: `ops_coroutine.test.tml:153,185,228,261`

Generic enum variants (Nothing, payload extraction) don't properly substitute type parameters:

```tml
// Nothing not typed correctly in generic context
func foo[T]() -> Maybe[T] {
    return Nothing  // Type error
}
```

### 2.1.4 Generic tuple return

**File**: `ops_range.test.tml:118`

Functions returning generic tuples don't work:
```tml
func into_inner[T](self) -> (T, T)  // Not supported
```

### 2.1.5 Generic methods on non-generic types

**File**: `ops_range.test.tml:219`

Can't define generic methods on concrete types.

### 2.1.6-2.1.7 Generic context issues

**Files**: `ops_drop.test.tml:35,60`

`T::default()` and generic return type inference don't work in generic contexts.

### 2.1.8 Generic .duplicate()

**Files**: `generic_bounds.test.tml:122,147`

`.duplicate()` method resolution fails for Maybe[T] and Outcome[T,E].

### 2.1.9 Bound enum variant

**File**: `ops_range.test.tml:13`

Bound enum (Included, Excluded, Unbounded) variant resolution fails.

### 2.1.10 Associated types codegen

**File**: `borrow_library.test.tml:128`

Cow and ToOwned associated types don't generate correct code.

### 2.1.11 Generic Maybe/Outcome inference

**File**: `enums_comprehensive.test.tml:64`

Type inference for generic enums fails in certain contexts.

---

## Phase 3: Display/Behavior Implementation

### 3.1.1 Custom type Display

**File**: `error.test.tml:75`

Display impl for custom types (SimpleError, ParseError, IoError, ChainedError, BoxedError) not resolved:

```tml
let err: SimpleError = SimpleError::new("msg")
err.to_string()  // "Unknown method: to_string"
```

### 3.1.2 impl Behavior returns

**File**: `impl_behavior_return.test.tml:21`

Runtime support for `-> impl Iterator` style returns not complete.

### 3.1.3 Clone behavior verification

**File**: `lifetime_bounds.test.tml:63`

No runtime check that types implement Clone behavior.

---

## Phase 4: Runtime Features

### 4.1.1-4.1.2 Async support

**Files**: `async_iter.test.tml:2`, `ops_async.test.tml:2`

Async iterators and Poll types blocked by generic enum issues.

### 4.1.3-4.1.4 Drop support

**Files**: `ops_drop.test.tml:99,106`

Drop behavior runtime support and drop_in_place lowlevel function.

### 4.1.5 Partial moves

**File**: `partial_move.test.tml:32`

Codegen for partial struct moves not implemented.

### 4.1.6 dyn return

**File**: `dyn_advanced.test.tml:100`

Returning `dyn Behavior` from functions requires Heap boxing.

### 4.1.7 Inherited fields

**File**: `oop.test.tml:397`

Can't initialize inherited fields in struct literals.

---

## Phase 5: Other Issues

### 5.1.1 Tuple literal types

**Files**: `generic_bounds.test.tml:83`, `partial_move.test.tml:111`

Tuple literals `(1, 2)` default to `(I64, I64)` not `(I32, I32)`.

### 5.1.2 Module constants

**File**: `ascii.test.tml:333`

Can't access module-level constants.

### 5.1.3 core::option xor

**File**: `option_result.test.tml:2`

`xor` keyword conflict in core::option module.

### 5.1.4 Lifetime bounds inference

**File**: `lifetime_bounds.test.tml:39`

Generic type inference with lifetime bounds not working.

---

## Priority

1. **Phase 1** (Codegen) - High priority, blocks basic tests
2. **Phase 2** (Generics) - High priority, blocks many library features
3. **Phase 3** (Display) - Medium priority, blocks error handling tests
4. **Phase 4** (Runtime) - Medium priority, blocks advanced features
5. **Phase 5** (Other) - Low priority, workarounds available
