# Spec: Nested String Return Corruption

## Problem

Functions that return a Str value received from another function have stack corruption.

### Current Behavior

```tml
func inner() -> Str {
    return "hello"
}

func outer() -> Str {
    let s = inner()  // Works
    return s         // Corrupted!
}

func test() {
    let result = outer()  // Contains garbage
    print(result)         // Prints garbage or crashes
}
```

### Expected Behavior

```tml
func test() {
    let result = outer()  // Should be "hello"
    print(result)         // Should print "hello"
}
```

## Root Cause

String literals in TML are allocated on the stack or as global constants. When returning through multiple call frames:

1. `inner()` returns pointer to string data
2. `outer()` receives pointer, stores in local `s`
3. `outer()` returns `s`, but the stack frame is about to be destroyed
4. Caller receives dangling pointer

### Analysis

In `compiler/src/codegen/llvm_ir_gen_stmt.cpp`, string returns use stack allocation:

```llvm
; inner() stores string on its stack
%str = alloca [6 x i8]
store [6 x i8] c"hello\00", ptr %str
ret ptr %str  ; Returning pointer to stack memory!
```

When `inner()` returns, its stack frame is deallocated, making the pointer invalid.

## Fix Options

### Option A: Copy on Return

Always copy string data when returning:

```llvm
; Allocate in caller's frame via sret
define void @inner(ptr sret(%String) %retval) {
  ; Copy string to caller-provided buffer
  call void @llvm.memcpy.p0.p0.i64(ptr %retval, ptr @.str.hello, i64 6, i1 false)
  ret void
}
```

### Option B: Heap Allocation

Allocate returned strings on heap:

```llvm
define ptr @inner() {
  %str = call ptr @malloc(i64 6)
  call void @llvm.memcpy.p0.p0.i64(ptr %str, ptr @.str.hello, i64 6, i1 false)
  ret ptr %str
}
```

### Option C: String Interning

Use global string pool for literals:

```llvm
@.str.hello = private constant [6 x i8] c"hello\00"

define ptr @inner() {
  ret ptr @.str.hello  ; Return pointer to global, always valid
}
```

## Recommended Fix

**Option C (String Interning)** for string literals, combined with **Option B (Heap)** for dynamic strings.

String literals should already be global constants. The bug is likely that we're copying them to the stack unnecessarily.

### Implementation

1. Check if string is a literal → use global constant directly
2. Check if string is from concatenation → already heap-allocated, just return
3. Only copy when necessary (e.g., substring operations)

## Test Cases

```tml
@test
func test_nested_string_return() -> I32 {
    func inner() -> Str { return "hello" }
    func outer() -> Str { return inner() }

    let result = outer()
    assert_eq(result, "hello", "nested return should work")
    return 0
}

@test
func test_string_concat_return() -> Str {
    func make_greeting(name: Str) -> Str {
        return "Hello, " + name + "!"
    }

    let greeting = make_greeting("World")
    assert_eq(greeting, "Hello, World!", "concat return should work")
    return 0
}
```
