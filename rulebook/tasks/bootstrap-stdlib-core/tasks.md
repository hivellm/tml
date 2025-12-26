# Tasks: Bootstrap Stdlib Core

## Progress: 66% (25/38 tasks complete)

**Latest Update (2025-12-26):** ✅ CRITICAL BUG FIX: Generic functions + closures now work! This unblocks stdlib functional programming patterns (map, filter, fold, etc). See [BUGS.md](../../../BUGS.md) for details.

## 1. Setup Phase
- [x] 1.1 Create `runtime/` directory structure (C runtime)
- [x] 1.2 Create runtime core (tml_runtime.c)
- [x] 1.3 Create collections runtime
- [x] 1.4 Create string runtime

## 2. Core Traits Phase
- [ ] 2.1 Implement Copy trait
- [ ] 2.2 Implement Clone trait (duplicate)
- [ ] 2.3 Implement Drop trait
- [ ] 2.4 Implement Eq and PartialEq traits
- [ ] 2.5 Implement Ord and PartialOrd traits
- [ ] 2.6 Implement Hash trait
- [ ] 2.7 Implement Display trait
- [ ] 2.8 Implement Debug trait
- [ ] 2.9 Implement Default trait
- [ ] 2.10 Implement From and Into traits

## 3. Primitive Types Phase
- [x] 3.1 Implement I8, I16, I32, I64 operations
- [x] 3.2 Implement U8, U16, U32, U64 operations
- [x] 3.3 Implement F32, F64 operations
- [x] 3.4 Implement Bool operations
- [ ] 3.5 Implement Char operations (Unicode)
- [ ] 3.6 Implement trait impls for all primitives

## 4. Option Type Phase (Maybe[T]) ✅ PARTIAL
- [x] 4.1 Define Maybe[T] enum (Just, Nothing)
- [x] 4.2 Implement is_just, is_nothing
- [ ] 4.3 Implement unwrap, expect (requires Never type for panic)
- [x] 4.4 Implement unwrap_or, unwrap_or_else
- [x] 4.5 Implement map, and_then, filter, or_else (**FIXED 2025-12-26**: Generic closures bug resolved!)
- [ ] 4.6 Implement ok_or, ok_or_else

## 5. Result Type Phase (Outcome[T, E]) ✅ PARTIAL
- [x] 5.1 Define Outcome[T, E] enum (Ok, Err)
- [x] 5.2 Implement is_ok, is_err
- [ ] 5.3 Implement unwrap, expect, unwrap_err (requires Never type)
- [x] 5.4 Implement map_ok, map_err (**FIXED 2025-12-26**: Generic closures bug resolved!)
- [x] 5.5 Implement and_then_ok, or_else_ok
- [ ] 5.6 Implement ! operator support

## 6. List Type Phase
- [x] 6.1 Implement List runtime with heap allocation
- [x] 6.2 Implement list_create, with_capacity
- [x] 6.3 Implement list_push, list_pop
- [x] 6.4 Implement list_len, list_is_empty, list_capacity
- [x] 6.5 Implement list_get, list_set, index access
- [ ] 6.6 Implement iter, iter_mut (PARTIAL: Range iterator with basic combinators working)
- [x] 6.7 Implement list_destroy for deallocation
- [x] 6.8 Implement method syntax (.len(), .push(), .pop(), .get(), .set())
- [x] 6.9 Implement array literal syntax `[1, 2, 3]`
- [x] 6.10 Implement repeat syntax `[0; 10]`
- [x] 6.11 Implement generic type annotation `List[I32]`

## 7. String Type Phase
- [x] 7.1 Implement String struct (tml_str)
- [x] 7.2 Implement str_create, str_from_cstr
- [ ] 7.3 Implement str_push, str_push_str
- [x] 7.4 Implement str_len, str_is_empty
- [ ] 7.5 Implement chars iterator
- [ ] 7.6 Implement bytes iterator
- [x] 7.7 Implement println for String

## 8. HashMap Type Phase
- [x] 8.1 Implement HashMap runtime struct
- [x] 8.2 Implement hashmap_create, with_capacity
- [x] 8.3 Implement hashmap_set, hashmap_get, hashmap_remove
- [x] 8.4 Implement hashmap_has, hashmap_len
- [ ] 8.5 Implement iter, keys, values
- [x] 8.6 Implement hashmap_destroy, hashmap_clear
- [x] 8.7 Implement generic type annotation `HashMap[I32, I32]`

## 9. Buffer Type Phase
- [x] 9.1 Implement Buffer runtime struct
- [x] 9.2 Implement buffer_create, buffer_destroy
- [x] 9.3 Implement buffer_write_byte, buffer_write_i32
- [x] 9.4 Implement buffer_read_byte, buffer_read_i32
- [x] 9.5 Implement buffer_len, buffer_capacity, buffer_remaining
- [x] 9.6 Implement buffer_clear, buffer_reset_read

## 10. Testing Phase
- [ ] 10.1 Write tests for core traits
- [x] 10.2 Write tests for primitives
- [x] 10.3 Write tests for Maybe (packages/std/tests/types.test.tml)
- [x] 10.4 Write tests for Outcome (packages/std/tests/types.test.tml)
- [x] 10.5 Write tests for List (test_19, test_20, test_22, test_23)
- [x] 10.6 Write tests for String
- [x] 10.7 Write tests for HashMap (test_19, test_23)
- [x] 10.8 Write tests for Buffer (test_19, test_23)
- [ ] 10.9 Verify test coverage ≥95%

## 11. Documentation Phase
- [x] 11.1 Document collections in user docs (ch08-00-collections.md)
- [x] 11.2 Update CHANGELOG.md with stdlib implementation (2025-12-26: Iterator combinators documented)
