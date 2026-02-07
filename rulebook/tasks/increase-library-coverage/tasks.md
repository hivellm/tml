# Tasks: Increase Library Test Coverage

**Status**: Proposed (0%)

## Phase 1: Core Type Conversions & Formatting (CRITICAL)

- [ ] 1.1.1 Test `convert` module (0/6): From/Into impls for primitive types
- [ ] 1.1.2 Test `fmt/impls` to_string for integer types (I8, I16, I32, I64, U8, U16, U32, U64)
- [ ] 1.1.3 Test `fmt/impls` to_string for float types (F32, F64)
- [ ] 1.1.4 Test `fmt/impls` to_string for Bool, Char, Str
- [ ] 1.1.5 Test `fmt/impls` debug_string for all primitive types
- [ ] 1.1.6 Test `fmt/impls` Display/Debug for compound types (Maybe, Outcome, tuples)
- [ ] 1.1.7 Test `fmt/float` module (0/28): float formatting (fixed, scientific, precision)
- [ ] 1.1.8 Run coverage, verify Phase 1 targets reached

## Phase 2: Arithmetic & Bitwise Operators

- [ ] 2.1.1 Test `ops/arith` Add for all integer types (I8-I64, U8-U64)
- [ ] 2.1.2 Test `ops/arith` Sub for all integer types
- [ ] 2.1.3 Test `ops/arith` Mul, Div, Rem for all integer types
- [ ] 2.1.4 Test `ops/arith` Add/Sub/Mul/Div for float types (F32, F64)
- [ ] 2.1.5 Test `ops/arith` Neg for signed types
- [ ] 2.1.6 Test `ops/arith` compound assignment operators (AddAssign, SubAssign, etc.)
- [ ] 2.1.7 Test `ops/bit` BitAnd, BitOr, BitXor for all integer types
- [ ] 2.1.8 Test `ops/bit` Shl, Shr for all integer types
- [ ] 2.1.9 Test `ops/bit` Not (bitwise negation) and compound assignments
- [ ] 2.1.10 Run coverage, verify Phase 2 targets reached

## Phase 3: Comparison & Ordering

- [ ] 3.1.1 Test `cmp` Ordering methods (is_less, is_equal, is_greater, reverse, then_cmp)
- [ ] 3.1.2 Test `cmp` PartialOrd (lt, le, gt, ge) for numeric types
- [ ] 3.1.3 Test `cmp` Ord (partial_cmp, cmp) for numeric types
- [ ] 3.1.4 Test `cmp` min, max, clamp utility functions
- [ ] 3.1.5 Test `cmp` comparison for Str, Bool, Char types
- [ ] 3.1.6 Test `cmp` PartialEq/Eq for compound types (Maybe, Outcome, tuples)
- [ ] 3.1.7 Run coverage, verify Phase 3 targets reached

## Phase 4: String Operations

- [ ] 4.1.1 Test `str` basic ops (len, is_empty, char_at, first_char, last_char)
- [ ] 4.1.2 Test `str` substring operations (substring, substring_from, substring_to)
- [ ] 4.1.3 Test `str` trim operations (trim, trim_start, trim_end)
- [ ] 4.1.4 Test `str` search operations (contains, starts_with, ends_with, find, rfind)
- [ ] 4.1.5 Test `str` transformation (to_uppercase, to_lowercase, replace, repeat)
- [ ] 4.1.6 Test `str` split and join operations
- [ ] 4.1.7 Test `str` comparison and ordering
- [ ] 4.1.8 Run coverage, verify Phase 4 targets reached

## Phase 5: Array & Iteration

- [ ] 5.1.1 Test `array` mutation (get_mut, first_mut, last_mut, set)
- [ ] 5.1.2 Test `array` slice operations (as_slice, as_mut_slice, split_array_ref)
- [ ] 5.1.3 Test `array` search (contains, find, position)
- [ ] 5.1.4 Test `array` transformation (reverse, sort, try_map, each_ref, each_mut)
- [ ] 5.1.5 Test `array/iter` module (0/19): ArrayIter, into_iter, iter, iter_mut
- [ ] 5.1.6 Test `iter/range` Step trait impls for all integer types
- [ ] 5.1.7 Test `iter/range` forward_checked, backward_checked, steps_between
- [ ] 5.1.8 Test `iter/traits/accumulators` (0/22): sum, product, fold, reduce
- [ ] 5.1.9 Run coverage, verify Phase 5 targets reached

## Phase 6: Hash

- [ ] 6.1.1 Test `hash` for primitive types (I8-I64, U8-U64, Bool, Str)
- [ ] 6.1.2 Test `hash` Hasher trait methods (write, write_u8..write_u64, finish)
- [ ] 6.1.3 Test `hash` DefaultHasher creation and hashing
- [ ] 6.1.4 Test `hash` BuildHasher trait
- [ ] 6.1.5 Test `hash` consistency (same value produces same hash)
- [ ] 6.1.6 Run coverage, verify Phase 6 targets reached

## Phase 7: Collections

- [ ] 7.1.1 Test `collections/class_collections` ArrayList (create, add, get, set, remove)
- [ ] 7.1.2 Test `collections/class_collections` ArrayList (insert, clear, count, is_empty)
- [ ] 7.1.3 Test `collections/class_collections` ArrayList iteration and search
- [ ] 7.1.4 Test `collections/class_collections` LinkedList basic ops
- [ ] 7.1.5 Test `collections/class_collections` TreeMap basic ops
- [ ] 7.1.6 Test `collections/class_collections` edge cases (empty, single element, large)
- [ ] 7.1.7 Run coverage, verify Phase 7 targets reached

## Phase 8: Smart Pointers & Allocators

- [ ] 8.1.1 Test `alloc/heap` remaining (into_inner, into_raw, from_raw, leak, duplicate, fmt)
- [ ] 8.1.2 Test `alloc/shared` remaining (get_mut, try_unwrap, as_ptr, from_raw, fmt)
- [ ] 8.1.3 Test `alloc/sync` remaining (get_mut, try_unwrap, as_ptr, from_raw, fmt)
- [ ] 8.1.4 Test `alloc` Allocator interface (allocate, deallocate, grow, shrink)
- [ ] 8.1.5 Test `alloc/global` remaining (realloc, alloc_array, dealloc_array, alloc_single)
- [ ] 8.1.6 Run coverage, verify Phase 8 targets reached

## Phase 9: Concurrency & Sync

- [ ] 9.1.1 Test `sync/atomic` compare_exchange_weak for AtomicBool, AtomicI32, AtomicU32
- [ ] 9.1.2 Test `sync/atomic` fetch_and, fetch_or, fetch_xor, fetch_nand
- [ ] 9.1.3 Test `sync/atomic` fetch_update and into_inner for all atomic types
- [ ] 9.1.4 Test `sync/atomic` AtomicI64, AtomicU64, AtomicIsize, AtomicUsize remaining ops
- [ ] 9.1.5 Test `task` module (1/26): task creation, scheduling, completion
- [ ] 9.1.6 Run coverage, verify Phase 9 targets reached

## Phase 10: Data Modules

- [ ] 10.1.1 Test `json/serialize` (0/23): serialize primitives, arrays, objects
- [ ] 10.1.2 Test `json/types` (0/42): JsonNumber, JsonObject, JsonArray operations
- [ ] 10.1.3 Test `pool` (4/29): ObjectPool create, acquire, release, stats
- [ ] 10.1.4 Test `cache` (1/36): CacheAligned, Padded, CacheAlignedBox ops
- [ ] 10.1.5 Test `arena` (1/16): Arena new, alloc_raw, reset, stats, clear
- [ ] 10.1.6 Test `intrinsics` remaining uncovered LLVM intrinsics
- [ ] 10.1.7 Run coverage, verify Phase 10 targets reached

## Phase 11: Final Coverage Validation

- [ ] 11.1.1 Run full test suite with coverage
- [ ] 11.1.2 Verify overall coverage >= 70%
- [ ] 11.1.3 Document any modules that could not reach target and why
- [ ] 11.1.4 Update coverage baselines
