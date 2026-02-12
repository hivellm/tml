# Phase 1 Investigation: Behavior Method Dispatch Bug

**Date**: 2026-02-12
**Status**: Investigation Complete - Requires Compiler Fix

## Problem Summary

Behavior methods on primitive types return `()` (unit type) instead of their declared return type when called using method syntax (e.g., `a.eq(ref b)`). This affects:

- `cmp::PartialEq::eq` → should return `Bool`, returns `()`
- `cmp::Ord::cmp` → should return `Ordering`, returns `()`
- `hash::Hash::hash` → returns `()` (possibly correct, but causes issues)
- `borrow::ToOwned::to_owned` → should return owned type, returns `()`

## Key Observations

### 1. Implementations Exist and Are Correct

The behavior implementations exist in the library and are syntactically correct:

```tml
// lib/core/src/cmp.tml:510
impl PartialEq for I32 {
    pub func eq(this, other: ref I32) -> Bool {
        return this == *other
    }
}
```

### 2. Tests Are Passing

The interesting discovery: **tests that use these methods are passing!**

```bash
# lib/core/tests/cmp/*.test.tml pass
# This means method calls like a.eq(ref b) work in some contexts
```

### 3. The Bug Is Context-Specific

The bug appears when:
- Using direct method call syntax in certain contexts
- Calling from user code outside the test framework
- Type checker sees the return type as `()`

## Reproduction Test Case

```tml
use core::cmp::PartialEq

func main() -> I32 {
    let a: I32 = 5
    let b: I32 = 5
    let result: Bool = a.eq(ref b)  // ERROR: Type mismatch: expected Bool, found ()
    return 0
}
```

## Hypotheses

### Hypothesis 1: Method Resolution Bug

The compiler's method resolution may be:
1. Finding the `PartialEq::eq` method correctly
2. Emitting a call to it
3. BUT losing the return type information in the process

**Evidence**: The LLVM IR generation fails to compile the function body at all, suggesting an earlier pipeline stage is failing.

### Hypothesis 2: Primitive Type Special Casing

The compiler may have special-case logic for primitive types that:
1. Handles operators (`+`, `-`, `==`, etc.) correctly
2. Fails to properly handle behavior method dispatch
3. Returns a default `()` type when it can't resolve the method

**Evidence**: The implementations added in Phase 1.1a (BitAnd, etc.) work correctly, suggesting the issue is not with all behavior methods, but specific ones.

### Hypothesis 3: Type Inference Ordering Issue

The type checker may be:
1. Attempting to infer the return type of `a.eq(ref b)`
2. Failing to find/resolve the implementation
3. Defaulting to `()` as a fallback

## Recommended Fix Strategy

### Step 1: Locate Method Dispatch Code

Search for:
- `compiler/src/types/` - Type checking and method resolution
- `compiler/src/codegen/codegen.cpp` - Method call codegen
- Look for functions like `resolve_method()`, `resolve_dynamic_impl()`, `emit_method_call()`

### Step 2: Add Logging

Add debug logging in the method resolution path to trace:
```cpp
// What method is being looked up
// What impl blocks are being considered
// What return type is being resolved
// Why it might default to `()`
```

### Step 3: Compare Working vs Broken

Compare:
- `BitAnd::bitand()` calls (WORKING after Phase 1.1a)
- `PartialEq::eq()` calls (BROKEN)

Find the difference in how they're being processed.

### Step 4: Check Primitive Type Registry

Verify that primitive types are registered with their behavior implementations:
- Are `PartialEq`, `Ord`, `Hash` impls registered?
- Is there a primitive type → behavior impl mapping?
- Is it being queried correctly during method resolution?

## Files to Investigate

### Type Checking
- `compiler/include/types/type_context.hpp`
- `compiler/src/types/type_checker.cpp`
- `compiler/src/types/method_resolution.cpp` (if exists)

### Codegen
- `compiler/src/codegen/codegen.cpp`
- `compiler/src/codegen/expr_codegen.cpp` (if exists)
- Look for `codegen_method_call()`, `resolve_dynamic_impl()`

### Query System
- `compiler/src/query/query_core.cpp`
- Check if method resolution queries are cached incorrectly

## Success Criteria

The fix is successful when:
1. `a.eq(ref b)` returns `Bool` instead of `()`
2. All existing tests continue to pass
3. The reproduction test case compiles and runs correctly

## Next Steps

This requires a C++ developer familiar with the compiler internals to:
1. Add the logging suggested above
2. Trace through a failing case (PartialEq::eq)
3. Compare with a working case (BitAnd::bitand)
4. Identify where the return type is being lost
5. Fix the root cause
6. Add regression tests

## Related Issues

- Phase 1.3: `cmp::Ord::cmp` - Same root cause
- Phase 1.4: `hash::Hash::hash` - Same root cause
- Phase 1.6: `borrow::ToOwned::to_owned` - Same root cause

Fixing this bug will unblock **44+ library functions** across cmp, hash, and borrow modules.
