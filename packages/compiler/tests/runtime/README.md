# TML Core/Runtime Tests

Test suite for TML core runtime functions (strings, memory, atomics, concurrency).

## Overview

These tests verify low-level runtime functions implemented in C (`runtime/tml_core.c`, `runtime/tml_collections.c`). These are core primitives, not standard library features.

## Test Files

| File | Functions Tested | Status | Tests |
|------|------------------|--------|-------|
| **strings.test.tml** | str_len, str_hash, str_eq | ❌ **BLOCKED** | 0/9 |
| **memory.test.tml** | alloc, dealloc, read_i32, write_i32, ptr_offset | ❌ **CRASH** | 0/7 |
| **atomic.test.tml** | atomic_*, fence_*, spin_* | ❌ **CRASH** | 0/17 |

**Total:** 0/33 runtime tests passing (0%)

## Running Tests

From `packages/compiler/build`:

```bash
# String tests (will fail - I64 comparison bug)
./Debug/tml.exe run ../tests/runtime/strings.test.tml

# Memory tests (will crash - pointer bug)
./Debug/tml.exe run ../tests/runtime/memory.test.tml

# Atomic tests (will crash - pointer bug)
./Debug/tml.exe run ../tests/runtime/atomic.test.tml
```

## Test Status

### ❌ String Functions (9 tests - Blocked by I64 Comparison Bug)

**Error:** `'%t0' defined with type 'i32' but expected 'i64'`

**Functions affected:**
- `str_len(s: Str) -> I64` - String length
- `str_hash(s: Str) -> I64` - String hash (FNV-1a)
- `str_eq(a: Str, b: Str) -> Bool` - String equality

**Problem:** Comparison operations with I64 values generate incorrect LLVM IR. The codegen treats comparison results as i32 but tries to store as i64.

**Tests created (9 tests):**
1. `test_str_len_empty` - Empty string length = 0
2. `test_str_len_short` - "hello" length = 5
3. `test_str_len_long` - Long string has correct length
4. `test_str_eq_same` - Equal strings return true
5. `test_str_eq_different` - Different strings return false
6. `test_str_eq_empty` - Empty strings are equal
7. `test_str_eq_case_sensitive` - "Hello" != "hello"
8. `test_str_hash_consistent` - Same string = same hash
9. `test_str_hash_different` - Different strings = different hashes

**Fix needed:** Update `packages/compiler/src/codegen/llvm_ir_gen_expr.cpp` to handle I64 comparisons correctly.

### ❌ Memory Functions (7 tests - Segmentation Fault)

**Error:** Exit code 1 (segmentation fault)

**Functions affected:**
- `alloc(size: I32) -> mut ref I32` - Allocate memory
- `dealloc(ptr: mut ref I32)` - Free memory
- `read_i32(ptr: mut ref I32) -> I32` - Read from pointer
- `write_i32(ptr: mut ref I32, value: I32)` - Write to pointer
- `ptr_offset(ptr: mut ref I32, offset: I32) -> mut ref I32` - Pointer arithmetic

**Problem:** Pointer reference types (`mut ref I32`) cause crashes. Likely issues:
- Incorrect codegen for pointer dereferencing
- Memory allocation implementation bugs
- Type mismatch in LLVM IR generation

**Tests created (7 tests):**
1. `test_alloc_dealloc_basic` - Basic allocation/deallocation
2. `test_write_read_i32` - Write 42, read it back
3. `test_write_read_negative` - Write -100, read it back
4. `test_write_read_zero` - Write 0, read it back
5. `test_multiple_writes` - Overwrite multiple times
6. `test_ptr_offset_basic` - Pointer arithmetic with offsets
7. `test_array_simulation` - Simulate array with pointers

**Fix needed:** Debug pointer codegen in `llvm_ir_gen_expr.cpp` and verify runtime memory functions.

### ❌ Atomic Functions (17 tests - Segmentation Fault)

**Error:** Exit code 1 (segmentation fault)

**Functions affected:**
- `atomic_load(ptr: mut ref I32) -> I32`
- `atomic_store(ptr: mut ref I32, value: I32)`
- `atomic_add(ptr: mut ref I32, value: I32) -> I32`
- `atomic_sub(ptr: mut ref I32, value: I32) -> I32`
- `atomic_exchange(ptr: mut ref I32, value: I32) -> I32`
- `atomic_cas(ptr: mut ref I32, expected: I32, desired: I32) -> Bool`
- `atomic_cas_val(ptr: mut ref I32, expected: I32, desired: I32) -> I32`
- `atomic_and(ptr: mut ref I32, value: I32) -> I32`
- `atomic_or(ptr: mut ref I32, value: I32) -> I32`
- `fence()`, `fence_acquire()`, `fence_release()`
- `spin_lock(lock: mut ref I32)`, `spin_unlock(lock: mut ref I32)`
- `spin_trylock(lock: mut ref I32) -> Bool`

**Problem:** Same pointer handling issue as memory functions.

**Tests created (17 tests):**
1. `test_atomic_store_load` - Basic store/load
2. `test_atomic_add` - Atomic addition (returns old value)
3. `test_atomic_sub` - Atomic subtraction
4. `test_atomic_exchange` - Atomic exchange
5. `test_atomic_cas_success` - CAS when expected matches
6. `test_atomic_cas_failure` - CAS when expected doesn't match
7. `test_atomic_cas_val` - CAS returning old value
8. `test_atomic_and` - Atomic bitwise AND
9. `test_atomic_or` - Atomic bitwise OR
10. `test_atomic_increment_pattern` - Increment 10 times
11. `test_fence_basic` - Memory fence
12. `test_fence_acquire` - Acquire fence
13. `test_fence_release` - Release fence
14. `test_spinlock_basic` - Lock/unlock spinlock
15. `test_spin_trylock_success` - Trylock on unlocked lock

**Fix needed:** Same as memory functions - fix pointer codegen.

## Critical Issues

### 1. ❌ I64 Comparison Bug (HIGH PRIORITY)
**File:** `packages/compiler/src/codegen/llvm_ir_gen_expr.cpp`
**Impact:** Blocks all string operations

When comparing I64 values:
```llvm
%t0 = icmp eq i64 %left, %right  ; Result is i1
store i64 %t0, ptr %t1           ; ERROR: trying to store i1 as i64
```

**Fix:** Update comparison codegen to properly handle I64 result types.

### 2. ❌ Pointer Reference Type Crashes (CRITICAL)
**Files:**
- `packages/compiler/src/codegen/llvm_ir_gen_expr.cpp`
- `packages/compiler/runtime/tml_core.c`

**Impact:** Blocks ALL pointer operations (memory, atomics, concurrency)

All operations with `mut ref I32` cause segmentation faults.

**Potential causes:**
- Incorrect pointer dereferencing in codegen
- Type mismatch in load/store operations
- Null pointer dereferences
- Memory allocation returning invalid pointers

**Fix:**
1. Debug codegen for reference types
2. Verify alloc/dealloc implementations
3. Add null pointer checks
4. Test basic pointer operations in isolation

## Not Tested

### Concurrency Functions
- `thread_spawn()`, `thread_join()`, `thread_yield()`, `thread_sleep()`, `thread_id()`
- `channel_create()`, `channel_send()`, `channel_recv()`, `channel_close()`, etc.
- `mutex_create()`, `mutex_lock()`, `mutex_unlock()`, `mutex_try_lock()`, `mutex_destroy()`
- `waitgroup_create()`, `waitgroup_add()`, `waitgroup_done()`, `waitgroup_wait()`, `waitgroup_destroy()`

**Reason:** Require pointer fixes first. Also complex multi-threading scenarios.

### Collection Functions
- `list_create()`, `list_push()`, `list_pop()`, `list_get()`, etc.
- `hashmap_create()`, `hashmap_set()`, `hashmap_get()`, etc.
- `buffer_create()`, `buffer_write_byte()`, `buffer_read_byte()`, etc.

**Reason:** Already tested in `packages/compiler/tests/tml/runtime/collections.test.tml` (currently failing due to runtime bug).

## Priority Fixes

### Immediate (Critical)
1. **Fix Pointer Crashes** - Enables memory + atomic + concurrency
   - Debug reference type codegen
   - Fix alloc/dealloc
   - Test basic load/store operations

### High Priority
2. **Fix I64 Comparison** - Enables string operations
   - Update comparison codegen for I64
   - Test with str_len, str_hash

### Future
3. **Add Concurrency Tests** - After pointer fix
4. **Stress Test Atomics** - Multi-threaded scenarios
5. **Collection Integration** - Fix existing collections tests

## See Also

- `packages/std/tests/stdlib/` - Standard library tests (I/O, time, math)
- `packages/compiler/src/types/env_builtins.cpp` - Function type declarations
- `packages/compiler/runtime/tml_core.c` - Core runtime implementations
- `packages/compiler/runtime/tml_collections.c` - Collection implementations
- Root `STDLIB_TEST_RESULTS.md` - Complete test analysis
