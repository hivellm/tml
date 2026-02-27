# Phase 1 Analysis - stdlib-essentials (2026-02-26)

## Status Summary

| Item | Status | Evidence |
|------|--------|----------|
| **1.1 Generic Iterator** | ✅ COMPLETE | Commit 605d54a7, working `for x in iter` |
| **1.2 Slice Parameters** | ✅ COMPLETE | Verified Feb 26 - `func f(data: [T])` generates correct `{ ptr, i64 }` |
| **1.3 Compound Constraints** | ✅ COMPLETE | Verified Feb 26 - `T: Behavior1 + Behavior2` works |
| **1.4.1 Function Pointers** | ✅ COMPLETE | Commit b99de4f2, FuncType in type system |
| **1.4.2 Lambda Conversion** | ❌ BLOCKING | Lambda codegen works, but conversion to func param fails |
| **1.4.3 Higher-order Calls** | ✅ COMPLETE | FuncType calls implemented in codegen |

## Deep Dive: Phase 1.4.2 Lambda Conversion Issue

### The Problem
When passing a lambda `do(x) x + 1` as an argument to a function expecting `func(I32) -> I32`, the compiler generates invalid LLVM IR:

```
error: void type only allowed for function results
    %v2 = call i32 @"apply"(void %v0, i32 5)
                            ^
```

The lambda is being compiled as `void` instead of `{ ptr, ptr }` (fat pointer).

### Root Cause Analysis

1. **Closure codegen works correctly** (`closure.cpp:117-358`)
   - Generates `{ ptr, ptr }` fat pointer correctly
   - Sets `last_expr_type_ = "{ ptr, ptr }"`
   - Function `gen_closure()` returns the SSA value correctly

2. **Closure type inference works** (`infer.cpp:1218-1246`)
   - Returns `FuncType` with correct signature
   - Should be `func(I32) -> I32`

3. **Function call handling is incomplete** (`call.cpp:124+`)
   - Handles closures in **struct fields** (`FieldExpr`, lines 150-273)
   - Extracts fat pointer, checks env_ptr, calls appropriately
   - **BUT**: Does NOT handle closures in **direct arguments**
   - When `apply(do(x) ..., 5)` is called, the lambda arg is not specially processed
   - Argument type inference fails, defaults to something that becomes `void`

### Where to Fix

**File**: `compiler/src/codegen/llvm/expr/call.cpp`
**Function**: `gen_call()` main dispatch (line 124)
**Lines**: Somewhere after "build user argument list" (around line 176-181)

**Fix needed**: When building arguments, check if the argument is a **ClosureExpr** or if its inferred type is **FuncType**. If so:
1. Generate it as normal (returns fat pointer `{ ptr, ptr }`)
2. Pass the fat pointer directly (not extract fields yet - that happens at call site)
3. Mark the argument type as `{ ptr, ptr }` in the function signature

### Why This Blocks Phase 2

Methods that need lambda support:
- `Vec::retain(pred: func(ref T) -> Bool)` — predicate for filtering
- `Array::map(f: func(T) -> U)` — transformation (wait, this uses behaviors in practice)
- Custom iterators with closures as callbacks

Most stdlib items can work with explicit functions, but some (like `retain`, `drain`) specifically need lambda support for idiomatic usage.

## Recommended Next Steps

1. **Fix 1.4.2** (1-2 hours)
   - Locate where user arguments are added in `gen_call()`
   - Check if arg type is FuncType when building call signature
   - Pass fat pointer directly without extracting fields initially

2. **Test 1.4.2** (30 min)
   - Compile `test_lambda_funcptr.tml` → should work
   - Add integration tests with actual Vec::retain usage

3. **Then start Phase 2** (unblocked after 1.4.2)
   - Begin with Vec iterators (uses 1.1)
   - Then HashSet iterators
   - Then optional features (BTreeMap, BufReader, etc.)
