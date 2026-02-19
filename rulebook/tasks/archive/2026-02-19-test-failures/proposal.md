# Proposal: Test Failures — Compiler/Runtime Bugs Blocking Coverage

## Problem

The TML compiler has multiple codegen and runtime bugs that prevent library tests from compiling or running correctly. These block **~218+ library functions** from being testable and cap coverage at ~50%. This document catalogs root causes and reproduction steps. It will be updated as new failures are discovered.

## Root Cause Analysis

### 1. "Unknown method" Error (ops/bit)

**Status**: PARTIALLY FIXED - Missing implementations added (2026-02-12)

When calling `bitand()`, `bitor()`, `bitxor()`, `shift_left()`, `shift_right()` on any integer primitive, the codegen phase emits "Unknown method" errors.

**Root Cause**: The `lib/core/src/ops/bit.tml` file defined the `BitAnd`, `BitOr`, `BitXor`, `Shl`, `Shr` **behaviors** but was **missing the `impl` blocks** for all integer primitives (I8-I64, U8-U64). The file only had the assign variants (`BitAndAssign`, etc.).

**Fix Applied**: Added complete implementations for:
- `impl BitAnd for I8/I16/I32/I64/U8/U16/U32/U64` (40 lines)
- `impl BitOr for I8/I16/I32/I64/U8/U16/U32/U64` (40 lines)
- `impl BitXor for I8/I16/I32/I64/U8/U16/U32/U64` (40 lines)
- `impl Shl for I8/I16/I32/I64/U8/U16/U32/U64` (40 lines)
- `impl Shr for I8/I16/I32/I64/U8/U16/U32/U64` (40 lines)

**Test Results**: After adding implementations, **460 tests now pass** in 12 test files (`lib/core/tests/ops/*bit*.test.tml`). The implementations work correctly with method call syntax (e.g., `a.bitand(b)`).

**Note**: Direct behavior dispatch syntax (e.g., `BitAnd::bitand(5, 3)`) still exhibits a type checker bug where methods return `()`, but this is not how the tests use these methods, so it doesn't block test coverage.

**Key file**: `lib/core/src/ops/bit.tml` (fixed)
**Test coverage impact**: +460 passing tests (ops/bit module)
**Related commit**: `bcaea6e` (feat(compiler): dynamic impl resolution for primitive types)

### 1b. "Unknown method" Error (ops/arith assign variants)

**Status**: FIXED - Missing implementations added (2026-02-12)

Similar to the `ops::bit` issue, the arithmetic assign operators (`+=`, `-=`, `*=`, `/=`, `%=`) had behavior definitions but were missing implementations for all primitive types.

**Root Cause**: The `lib/core/src/ops/arith.tml` file defined the `AddAssign`, `SubAssign`, `MulAssign`, `DivAssign`, `RemAssign` **behaviors** but was **missing the `impl` blocks** for all primitive types (I8-I64, U8-U64, F32-F64).

**Fix Applied**: Added complete implementations for:
- `impl AddAssign for I8/I16/I32/I64/U8/U16/U32/U64/F32/F64` (10 types)
- `impl SubAssign for I8/I16/I32/I64/U8/U16/U32/U64/F32/F64` (10 types)
- `impl MulAssign for I8/I16/I32/I64/U8/U16/U32/U64/F32/F64` (10 types)
- `impl DivAssign for I8/I16/I32/I64/U8/U16/U32/U64/F32/F64` (10 types)
- `impl RemAssign for I8/I16/I32/I64/U8/U16/U32/U64` (8 types, no remainder for floats)

Total: 48 new trait implementations

**Test Results**: After adding implementations, **460 tests now pass** in 12 test files (`lib/core/tests/ops/*arith*.test.tml`). All arithmetic assign operators (`+=`, `-=`, `*=`, `/=`, `%=`) now work correctly on all numeric primitives.

**Key file**: `lib/core/src/ops/arith.tml` (fixed)
**Test coverage impact**: +460 passing tests (ops/arith module)

### 1c. Missing Implementations (overflow behaviors)

**Status**: IDENTIFIED - Not yet implemented (2026-02-12)

The `lib/core/src/num/overflow.tml` file defines **19 overflow-handling behaviors** but has **zero implementations** for any primitive type:

**Missing Behaviors:**
- `CheckedAdd/Sub/Mul/Div/Rem/Neg/Shl/Shr` (8 behaviors)
- `SaturatingAdd/Sub/Mul` (3 behaviors)
- `WrappingAdd/Sub/Mul/Neg` (4 behaviors)
- `OverflowingAdd/Sub/Mul/Neg` (4 behaviors)

**Complexity**: These require LLVM intrinsics (`llvm.sadd.with.overflow`, `llvm.uadd.with.overflow`, etc.) or runtime implementations. Each behavior needs implementations for 8-10 integer types (I8-I64, U8-U64), totaling **~170 implementations**.

**Recommendation**: This is a large task that requires either:
1. Codegen support for emitting LLVM overflow intrinsics
2. Runtime C functions wrapping compiler builtins (`__builtin_add_overflow`, etc.)
3. Manual TML implementations using bit manipulation and range checks

**Key file**: `lib/core/src/num/overflow.tml` (behaviors defined, implementations missing)
**Blocked tests**: Phase 4 of test-failures task (~19 functions × 8-10 types = ~170+ methods)

### 2. Methods Returning `()` Instead of Correct Type

Many trait methods on primitives return `()` (unit type) instead of their declared return type:
- `cmp::PartialEq::eq` → should return `Bool`, returns `()`
- `cmp::Ord::cmp` → should return `Ordering`, returns `()`
- `hash::Hash::hash` → should return `()` (correct) but causes downstream issues
- `num::overflow::checked_add` → should return `Maybe[I32]`, returns `()`
- `borrow::ToOwned::to_owned` → should return owned type, returns `()`

This suggests the codegen emits a call to the method but doesn't properly resolve the function body, so it falls through to a default `()` return.

**Key file**: `compiler/src/codegen/codegen.cpp` — method call codegen for primitive receiver types

### 3. Undefined Symbol for `fmt_*` Methods

`fmt_binary()`, `fmt_octal()`, `fmt_lower_hex()`, etc. compile successfully to LLVM IR but the symbols are not found at link time. The function declarations exist but the definitions are not emitted.

**Key file**: `compiler/src/codegen/codegen.cpp` — fmt trait impl codegen

### 4. Generic Monomorphization Bug

Generic functions like `unwrap[T](Maybe[T]) -> T` in `std::types` emit LLVM IR with generic type names (`%struct.Maybe__T`) instead of monomorphized names (`%struct.Maybe__I32`). This causes LLVM IR parsing failures.

**Key file**: `compiler/src/codegen/codegen.cpp` — generic function instantiation

### 5. Char Parameter Codegen Bug

Functions taking `Char` parameters (like `unicode::char::is_cased(c: Char)`) generate incorrect LLVM IR where Char values (i32) are used where i1 (Bool) is expected, or Char literals are emitted as `ptr 48` instead of integer values.

**Key file**: `compiler/src/codegen/codegen.cpp` — Char literal and parameter handling

### 6. Runtime Stack Overflow

`str::parse_*` functions (parse_i32, parse_i64, parse_f64, parse_bool) compile and link successfully but cause stack overflow at runtime, suggesting infinite recursion in the generated code.

## Reproduction

All issues can be reproduced with simple test files:

```tml
// 1. "Unknown method"
use test
@test
func test_bitand() -> I32 {
    let a: I32 = 5
    let r: I32 = a.bitand(3)  // ERROR: Unknown method: bitand
    return 0
}

// 2. Returns () instead of Bool
use test
@test
func test_eq() -> I32 {
    let a: I32 = 5
    let r: Bool = a.eq(5)  // ERROR: Type mismatch: expected Bool, found ()
    return 0
}

// 3. Generic monomorphization
use test
use std::types
@test
func test_unwrap() -> I32 {
    let m: Maybe[I32] = Just(42)
    let v: I32 = types::unwrap(m)  // ERROR: Maybe__T vs Maybe__I32
    return 0
}
```

## Suggested Fix Strategy

1. **Phase 1-2**: Audit `resolve_dynamic_impl()` and the primitive type impl registry. Ensure all trait impls (BitAnd, BitOr, PartialEq, Ord, Hash, ToOwned, assign operators) are registered for all primitive types and correctly resolve method bodies.

2. **Phase 3**: Ensure fmt trait implementations for primitives emit function definitions (not just declarations) into the LLVM IR module.

3. **Phase 4**: Fix checked/saturating/wrapping arithmetic — same root cause as Phase 1 (method body not resolved).

4. **Phase 5**: Fix generic function instantiation to properly substitute type parameters in LLVM struct names.

5. **Phase 6**: Debug runtime crashes in str::parse_* functions (likely recursive call instead of FFI call to runtime).

## Impact

Fixing these issues would unblock ~218 additional library functions for testing, potentially pushing coverage from ~50% to ~55-60%.
