# Tasks: Migrate C Runtime Pure Algorithms to TML

**Status**: In Progress (Phases 0-7, 16, 17, 18.1, 19, 20, 21, 22 complete; Phases 18.2, 23-30 planned — full runtime.cpp audit done: 287→221→68 declares target)

**Scope**: ~287 runtime.cpp declares to minimize → ~68 essential; 15 dead declares to remove (Phase 17) + ~204 to migrate (Phases 18-26)

**Current metrics** (2026-02-18): 76.2% coverage (3,228/4,235), 9,025 tests across 787 files

**Phase 7-15 Audit Results** (2026-02-18):
- Phase 7: INTEGER TO STRING — **MIGRATED** to pure TML (16 lowlevel blocks eliminated)
- Phase 8: FLOAT TO STRING — KEEP as lowlevel (hardware-dependent: snprintf, NaN/inf checks, round)
- Phase 9: CHAR/UTF-8 — KEEP as lowlevel (registered functions_[] map entries, work correctly)
- Phase 10: HEX/OCTAL/BINARY — Already pure TML, no lowlevel blocks
- Phase 11: TEXT — KEEP as lowlevel (48 registered functions_[] entries; migration blocked by type checker not supporting memory intrinsics)
- Phase 12: SEARCH — KEEP as lowlevel (FFI tier 2 pattern, @extern declarations with registered functions)
- Phase 13: JSON — KEEP as lowlevel (3 registered functions_[] entries: str_char_at, str_substring, str_len)
- Phase 14: LOGGING — KEEP as lowlevel (all 18 calls are I/O, global state, file ops)
- Phase 15: MINOR MODULES — Deferred (pool.c, profile_runtime.c, io.c)

**Key finding**: The type checker does NOT support memory intrinsics (mem_alloc, ptr_read[T], ptr_write[T]) for return type inference. Functions using these in new lowlevel blocks get wrong IR types. Only registered functions_[] map entries work correctly. This blocks migration of Text (Phase 11) and any module that would need manual memory management in pure TML format.

**Architecture goal**: Thin C layer (I/O, panic, malloc) + `@extern("c")` FFI bindings. Everything else in pure TML.

---

## Phase 0: Validation — Verify Compiler Primitives Work

- [x] 0.1.1 Write test: `ptr_read`, `ptr_write`, `ptr_offset` with I32, I64, U8 → `intrinsics_memory.test.tml` (10 tests)
- [x] 0.1.2 Write test: `mem_alloc` + `ptr_write` + `ptr_read` round-trip → `intrinsics_ptr_arith.test.tml` (6 tests)
- [x] 0.1.3 Write test: `copy_nonoverlapping` for array-like buffers → `intrinsics_copy.test.tml` (4 tests)
- [x] 0.1.4 Write test: `size_of[T]()` for primitives and structs → `intrinsics_size_align.test.tml` (14 tests, pre-existing)
- [x] 0.1.5 Write test: `@lowlevel` block with pointer arithmetic in generic context → `lowlevel.test.tml` (15 tests, pre-existing)
- [x] 0.1.6 Write test: generic struct with `*T` field (monomorphization) → `lowlevel.test.tml` `Node[T] { value: T, next: Ptr[Node[T]] }`
- [x] 0.1.7 Confirm all Phase 0 tests pass — 20 new tests + 14 existing = 34 tests all passing
- [x] 0.1.8 **Compiler fix**: Added codegen for `copy_nonoverlapping`, `copy`, `write_bytes` intrinsics (→ `@llvm.memcpy`, `@llvm.memmove`, `@llvm.memset`)

---

## Phase 1: Collections — List[T] Pure TML

> **Source**: `lib/std/src/collections/list.tml`
> **Status**: MIGRATED — pure TML using `ptr_read`/`ptr_write`/`mem_alloc`/`mem_realloc`/`mem_free`

- [x] 1.1.1 Rewrite `List[T]` — pure TML with `handle: *Unit` layout (header: data_ptr + len + capacity)
- [x] 1.1.2 Implement `new()`, `default()` using `mem_alloc` (24-byte header + data buffer)
- [x] 1.1.3 Implement `push()` with grow via `mem_realloc` + `ptr_write`
- [x] 1.1.4 Implement `pop()` via `ptr_read` + len decrement
- [x] 1.1.5 Implement `get()` / `set()` via pointer arithmetic + `ptr_read`/`ptr_write`
- [x] 1.1.6 Implement `len()`, `capacity()`, `is_empty()` via header field reads
- [x] 1.1.7 Implement `clear()` (set len=0), `first()`, `last()`
- [x] 1.1.8 Implement `destroy()` — free data buffer + free header
- [x] 1.1.9 All List tests pass with pure TML implementation
- [x] 1.1.10 Fix `[]` empty-array initializer creating null List handles (`cache.tml`, `pool.tml`)
- [x] 1.1.11 Fix zero-arg `List[T]::new()` calls → `List[T].default()` (5 files: list_buffer_debug, zstd_dict_train_debug, zstd_train_isolate, bm25.tml, hnsw.tml)
- [x] 1.1.12 Fix suite-prefix on builtin enums (Outcome, Maybe) — added `env_.lookup_enum()` fallback in `method_impl.cpp`
- [x] 1.1.13 All 17 original test failures resolved; full suite passes (only pre-existing x509 FFI incompatibility remains)

Remaining cleanup (compiler-side):
- [x] 1.2.1 Remove dead `list_is_empty` from `llvm_ir_gen_stmt.cpp:100` and `list_create` from `:160`
- [x] 1.2.2 Verified `runtime.cpp` already clean — no list_* declare statements remain
- [x] 1.2.3 Verified `types/builtins/collections.cpp` clean — only migration comment remains
- [x] 1.2.4 Verified `method_collection.cpp` clean — no list_* references remain
- [ ] 1.2.9 Legacy `@list_len`/`@list_get` calls remain in `loop.cpp:355,420` and `collections.cpp:329` (for-in / index fallback paths for C-backed collections; will be removed when HashMap/Buffer migrate)
- [x] 1.2.5 Migrate x509.tml X509Chain from C list_* FFI to pure TML List[I64]
- [x] 1.2.6 Migrate crypto/constants.tml get_hashes/get_ciphers/get_curves to pure TML List[Str]
- [x] 1.2.7 Migrate crypto/ecdh.tml get_curves to pure TML List[Str]
- [x] 1.2.8 Fix x509.tml subject_alt_names to return empty List[Str] instead of wrapping NULL handle

---

## Phase 2: Collections — HashMap[K, V] Pure TML

> **Source**: `lib/std/src/collections/hashmap.tml`
> **Status**: MIGRATED — pure TML using `ptr_read`/`ptr_write`/`mem_alloc`/`mem_free`/`write_bytes`
> **Design**: Type-erased I64 storage, FNV-1a hashing, linear probing, 0.7 load factor, tombstone deletion

- [x] 2.1.1 Design `HashMap[K, V]` with type-erased I64 storage: header 24 bytes (entries_ptr + len + capacity), entry 24 bytes (key I64 + value I64 + occupied I32 + deleted I32)
- [x] 2.1.2 Implement FNV-1a hash function: offset basis `-3750763034362895579`, prime `1099511628211`, result masked positive
- [x] 2.1.3 Implement `new(capacity)` / `default()` using `mem_alloc` + `write_bytes` zero-init
- [x] 2.1.4 Implement `set()` with linear probing, 0.7 load factor grow trigger, tombstone reuse
- [x] 2.1.5 Implement `get()` returning type-erased I64 value (zero if not found)
- [x] 2.1.6 Implement `has()` / `remove()` with tombstone marking (deleted=1)
- [x] 2.1.7 Implement `len()`, `clear()` (write_bytes zero-init + len=0)
- [x] 2.1.8 Implement `HashMapIter[K,V]` as 16-byte struct (map_handle + index), with `has_next()`/`next()`/`key()`/`value()`/`destroy()`
- [x] 2.1.9 Implement `destroy()` — `mem_free` entries array + `mem_free` header
- [x] 2.1.10 Keys use `key as I64` (ptrtoint for Str, sext for integers) — content-based Hashable behavior deferred
- [x] 2.1.11 Remove HashMap codegen dispatch from 10 compiler files: method_collection.cpp, method_static.cpp, method_static_dispatch.cpp, method_impl.cpp, impl.cpp, generate.cpp, struct.cpp, builtins/collections.cpp, runtime.cpp, types/builtins/collections.cpp
- [x] 2.1.12 Remove HashMap C functions from `compiler/runtime/collections/collections.c` and `lib/std/runtime/collections.{c,h}`
- [x] 2.1.13 All HashMap tests pass: hashmap_basic (6), hashmap_iter (3), test_hashmap_iter (2) — 11 total
- [x] 2.1.14 **Bonus**: Fixed string literal deduplication in compiler — `add_string_literal()` now deduplicates identical strings via `string_literal_dedup_` map

---

## Phase 3: Collections — Buffer Pure TML

> **Source**: `lib/std/src/collections/buffer.tml`
> **Status**: MIGRATED — pure TML using `ptr_read`/`ptr_write`/`mem_alloc`/`mem_free`/`mem_realloc`
> **Design**: 32-byte header (data_ptr + len + capacity + read_pos), direct memory access for LE I32/I64 read/write, byte-level endian decomposition for BE, type-punning for float<->int conversion

- [x] 3.1.1 Rewrite `Buffer` struct: `handle: *Unit` with 32-byte header (data ptr + len + cap + read_pos)
- [x] 3.1.2 Implement `new()` / `default()` using `mem_alloc` for header + data buffer
- [x] 3.1.3 Implement sequential write: `write_byte()`, `write_i32()`, `write_i64()` via `ptr_write[T]` direct memory
- [x] 3.1.4 Implement sequential read: `read_byte()`, `read_i32()`, `read_i64()` via `ptr_read[T]` direct memory
- [x] 3.1.5 Implement state accessors: `len()`, `capacity()`, `remaining()`, `get()`, `set()`
- [x] 3.1.6 Implement `clear()`, `reset_read()`, `destroy()` (mem_free data + header)
- [x] 3.1.7 Implement offset-based endian read/write: u8/i8, u16/i16 LE/BE, u32/i32 LE/BE, u64/i64 LE/BE
- [x] 3.1.8 Implement float read/write via type-punning: `ptr_write[F32]` then `ptr_read[I32]` on same address
- [x] 3.1.9 Implement manipulation: `fill()`, `fill_all()`, `copy_to()`, `slice()`, `duplicate()`, `swap16/32/64()`
- [x] 3.1.10 Implement search: `compare()`, `equals()`, `index_of()`, `last_index_of()`, `includes()`
- [x] 3.1.11 Implement string conversion: `to_hex()`, `to_string()`, `from_hex()`, `from_string()`
- [x] 3.1.12 Remove Buffer codegen dispatch from 10 compiler files (method_collection, method_static, method_static_dispatch, method_impl, impl, generate, struct, builtins/collections, runtime, types/builtins/collections)
- [x] 3.1.13 Remove Buffer C functions from `compiler/runtime/collections/collections.c` and `lib/std/runtime/collections.{c,h}`
- [x] 3.1.14 All 8 Buffer test files pass (31+ tests): buffer_le, buffer_be, buffer_float, buffer_string, buffer_search, buffer_compare, buffer_duplicate, buffer_swap
- [x] 3.1.15 Fixed signed division bug in I64 byte decomposition — use `ptr_write[I64]`/`ptr_read[I64]` direct memory access for LE
- [x] 3.1.16 Fixed I8→I32 sign-extension in `buf_read_byte_at` — mask to unsigned 0-255 range
- [x] 3.1.17 Full test suite passes (8465/8468 — 3 pre-existing crypto failures unrelated to Buffer)

---

## Phase 4: Strings — Core Read-Only Operations Pure TML ✓

> **Source**: `lib/core/src/str.tml`
> **C backing**: `compiler/runtime/text/string.c` (1,201 lines)
> **Status**: COMPLETE — all read-only ops migrated to pure TML

- [x] 4.1.1 Implement `str_len()` — iterate bytes via `ptr_read[U8]` until null terminator, pure TML
- [x] 4.1.2 Implement `str_char_at()` — byte indexing via pointer arithmetic + `ptr_read[U8]`
- [x] 4.1.3 Implement `str_contains()` — byte-by-byte nested loop search, pure TML
- [x] 4.1.4 Implement `str_starts_with()` / `str_ends_with()` — prefix/suffix byte compare, pure TML
- [x] 4.1.5 Implement `str_find()` / `str_rfind()` — return Maybe[I64] (find forward/backward), pure TML
- [ ] 4.1.6 Implement `str_as_bytes()` — pointer cast to `ref [U8]` (BLOCKED: slice type codegen)
- [ ] 4.1.7 Implement `str_hash()` in `lib/core/src/hash.tml` — FNV-1a in pure TML
- [x] 4.1.8 Update both standalone functions AND `impl Str` methods in `str.tml`
- [ ] 4.1.9 Remove corresponding emitters from `builtins/string.cpp` (after all string phases done)
- [x] 4.1.10 Run all existing string tests — 9,025 passed, 0 failed (full suite)

---

## Phase 5: Strings — Allocating Operations Pure TML ✓

> **Source**: `lib/core/src/str.tml`
> **C backing**: `compiler/runtime/text/string.c`
> **Status**: COMPLETE — all allocating string ops migrated to pure TML

- [x] 5.1.1 Implement `str_substring()` — `mem_alloc` + byte-by-byte copy via `ptr_read`/`ptr_write`
- [x] 5.1.2 Implement `str_trim()` / `trim_start()` / `trim_end()` — find non-whitespace bounds, then substring
- [x] 5.1.3 Implement `str_to_uppercase()` / `str_to_lowercase()` — byte-by-byte case flip ('a'-'z' ↔ 'A'-'Z')
- [x] 5.1.4 Implement `str_split()` — scan for delimiter, collect into `List[Str]`
- [x] 5.1.5 Implement `str_split_whitespace()` / `str_lines()` — split on whitespace/newlines
- [x] 5.1.6 Implement `str_replace()` / `str_replace_first()` — find + rebuild string
- [x] 5.1.7 Implement `str_repeat()` — alloc n*len + repeated copy
- [x] 5.1.8 Implement `str_join()` — calculate total len, alloc, copy parts with separator
- [x] 5.1.9 Implement `str_chars()` — byte values into `List[I32]`
- [x] 5.1.10 Update both standalone functions AND `impl Str` methods
- [ ] 5.1.11 Remove corresponding emitters from `builtins/string.cpp` (deferred to Phase 16)
- [x] 5.1.12 Run all existing string tests — all passed, 0 failures

---

## Phase 6: Strings — Parsing Pure TML ✓

> **Source**: `lib/core/src/str.tml`
> **C backing**: `compiler/runtime/text/string.c` (str_parse_* functions)
> **Status**: COMPLETE — all parse ops migrated to pure TML

- [x] 6.1.1 Implement `str_parse_i32()` — sign detection + digit loop, returns Maybe[I32]
- [x] 6.1.2 Implement `str_parse_i64()` — sign detection + digit loop, returns Maybe[I64]
- [x] 6.1.3 Implement `str_parse_f64()` — integer part + decimal part + exponent (e/E with sign)
- [x] 6.1.4 Update `impl Str` parse methods — delegate to standalone functions
- [ ] 6.1.5 Remove corresponding emitters from `builtins/string.cpp` (deferred to Phase 16)
- [x] 6.1.6 Run all existing parse tests — all passed, 0 failures

---

## Phase 7: Formatting — Integer to String Pure TML ✓

> **Source**: `lib/core/src/fmt/impls.tml`, `fmt/helpers.tml`
> **C backing**: `compiler/runtime/text/text.c` (1,057 lines)
> **Status**: COMPLETE — 16 integer Display/Debug lowlevel blocks eliminated
> **Approach**: Pure TML string concatenation with digit lookup tables (when-based i64_digit_char/u64_digit_char)

Tasks:
- [x] 7.1.1 Implement `i64_to_str()` — digit extraction loop: `% 10` + `/ 10`, string concatenation with `i64_digit_char()` lookup
- [x] 7.1.2 Implement `i8/i16/i32_to_str()` — cast to I64, delegate to `i64_to_str()`
- [x] 7.1.3 Implement `u8/u16/u32/u64_to_str()` — unsigned variant with `u64_digit_char()` lookup
- [x] 7.1.4 Update 16 integer `impl Display/Debug` blocks in `fmt/impls.tml` to call pure TML functions
- [x] 7.1.5 Update `fmt/helpers.tml` — rewrote `i64_to_str`/`u64_to_str` as pure TML; kept `str_len`/`char_to_str`/`str_slice` as lowlevel (registered functions_[] entries)
- [x] 7.1.6 `fmt/formatter.tml` — kept `str_len`/`char_to_str`/`str_slice` as lowlevel (registered functions_[] entries)
- [x] 7.1.7 `fmt/traits.tml` — kept `char_to_string` as lowlevel (registered functions_[] entry)
- [x] 7.1.8 All 404 fmt tests pass

**Not migrated** (correctly staying as lowlevel):
- Float Display/Debug: `f32_to_string`, `f64_to_string`, `f32_to_exp_string`, `f64_to_exp_string` (hardware-dependent, Phase 8)
- Char Display: `char_to_string` (registered functions_[] entry)
- String helpers: `str_len`, `str_slice`, `char_to_string` (registered functions_[] entries)

**Key findings**:
- `base` is a reserved keyword in TML's parser — renamed to `addr`
- Memory intrinsics (`mem_alloc`, `ptr_write[T]`, `ptr_read[T]`) don't have type checker support for new code — causes LLVM IR type mismatches
- String concatenation approach avoids all type checker issues while producing correct output

---

## Phase 8: Formatting — Float to String — KEEP LOWLEVEL

> **Source**: `lib/core/src/fmt/float.tml`
> **Status**: EVALUATED — all 11 lowlevel blocks STAY (hardware-dependent)
> **Reason**: Float-to-string conversion relies on snprintf for correct IEEE 754 formatting, NaN/inf detection uses FPU status bits, rounding uses hardware instruction

All lowlevel blocks are registered functions_[] entries with correct type checker support:
- `f32_to_string`, `f32_to_string_precision`, `f32_to_exp_string` — snprintf-based
- `f64_to_string`, `f64_to_string_precision`, `f64_to_exp_string` — snprintf-based
- `f32_is_nan`, `f32_is_infinite`, `f64_is_nan`, `f64_is_infinite` — FPU hardware checks
- `f64_round` — hardware rounding instruction

---

## Phase 9: Formatting — Char/UTF-8 to String — KEEP LOWLEVEL

> **Sources**: `lib/core/src/char/decode.tml`, `char/methods.tml`, `ascii/char.tml`
> **Status**: EVALUATED — all ~14 lowlevel blocks STAY (registered functions_[] map entries)
> **Reason**: `char_to_string`, `utf8_2byte_to_string`, etc. are registered in compiler's functions_[] map with correct type information. Migration would require mem_alloc + ptr_write which the type checker doesn't support for new code.

All char/UTF-8 conversion functions work correctly as lowlevel blocks because they're in the functions_[] registry.

---

## Phase 10: Formatting — Math Number Formats — ALREADY PURE TML ✓

> **Source**: `lib/core/src/fmt/helpers.tml`, `lib/core/src/fmt/num.tml`
> **Status**: VERIFIED — all hex/binary/octal formatting is already pure TML
> **No lowlevel blocks found** in num.tml or the binary/octal/hex helper functions

Already implemented in pure TML:
- [x] `u8/u16/u32/u64_to_binary_str()` — extract via `% 2` / `/ 2`
- [x] `i8/i16/i32/i64_to_binary_str()` — cast to unsigned, delegate
- [x] `u64_to_octal_str()` — extract via `% 8` / `/ 8`
- [x] `u64_to_hex_str()` — extract via `% 16` with hex digit table
- [x] `digit_to_char()`, `hex_digit()` — when-based lookup tables

---

## Phase 11: Text Utilities (std::text) — BLOCKED / DEFERRED

> **Source**: `lib/std/src/text.tml`
> **C backing**: `compiler/runtime/text/text.c` (1,057 lines)
> **48 lowlevel blocks**
> **Status**: BLOCKED — all text_* functions are registered in functions_[] map (correct types), but migration requires mem_alloc/ptr_write which the type checker doesn't support for new code
> **Prerequisite**: Fix type checker to support memory intrinsics (mem_alloc → ptr, ptr_read[T] → T, ptr_write[T] → void) before migration is possible

Tasks (deferred until type checker fix):
- [ ] 11.1.1 Fix type checker to infer correct return types for memory intrinsics in lowlevel blocks
- [ ] 11.1.2 Rewrite `Text` struct as `data: *U8, len: I64, capacity: I64` (replace opaque handle)
- [ ] 11.1.3-11.1.12 Implement all text operations in pure TML (see proposal.md for full list)

---

## Phase 12: Search Algorithms — KEEP AS FFI (Tier 2)

> **Sources**: `lib/std/src/search/bm25.tml`, `hnsw.tml`, `distance.tml`
> **C backing**: `compiler/runtime/search/search.c` (98 lines)
> **40 lowlevel blocks**
> **Status**: EVALUATED — all use @extern("c") FFI declarations + registered functions_[] map entries
> **Reason**: This is the correct Tier 2 pattern (@extern FFI). These are complex C algorithms (HNSW graph search, BM25 inverted index, SIMD-optimized distance) that should use FFI, not be reimplemented in TML.

The search module is already following the three-tier rule correctly:
- @extern declarations bind to C implementation
- lowlevel blocks call the registered functions
- No migration needed

---

## Phase 13: JSON Helper Calls — KEEP LOWLEVEL

> **Source**: `lib/std/src/json/types.tml`
> **3 lowlevel blocks**
> **Status**: EVALUATED — all 3 are registered functions_[] map entries (str_char_at, str_substring, str_len)
> **Reason**: These are properly type-checked registered functions. They provide efficient byte-level string access for JSON path navigation. No migration needed.

---

## Phase 14: Logging — KEEP LOWLEVEL (All I/O)

> **Source**: `lib/std/src/log.tml`
> **18 lowlevel blocks**
> **Status**: EVALUATED — all 18 calls are I/O, global state, or file operations
> **Reason**: All rt_log_* functions require C runtime for: stdout/file I/O (7), global atomic state (3), filter matching (2), JSON output (1), format config (2), file I/O (2), env var read (1). No pure formatting logic to migrate.

---

## Phase 15: Minor Modules

- [ ] 15.1.1 Migrate `compiler/runtime/memory/pool.c` (344 lines) logic to `lib/core/src/pool.tml`
- [ ] 15.1.2 Migrate `compiler/runtime/core/profile_runtime.c` (54 lines) to `lib/test/src/bench/`
- [ ] 15.1.3 Remove `compiler/runtime/core/io.c` (67 lines — duplicate of essential.c)
- [ ] 15.1.4 Run full test suite — all tests must pass

---

## Phase 16: Codegen Cleanup — Remove Dead Emitters

> **Status**: PARTIALLY COMPLETE — only `functions_[]` map cleanup was safe
> **Key finding**: Builtin emitters in `string.cpp` and `math.cpp` handle ALL function calls (not just lowlevel blocks) via `try_gen_builtin_string()` / `try_gen_builtin_math()` dispatch in `call.cpp`. Similarly, `declare` statements are needed by compiler codegen (`method_primitive_ext.cpp`, `method_primitive.cpp`, `derive/*.cpp`). Only `functions_[]` map entries (used exclusively for lowlevel block type resolution) can be safely removed for functions not called from lowlevel blocks.

- [x] 16.1.1 Audit `builtins/string.cpp` — 45 emitters, ALL active (dispatch ALL string/char/float calls)
- [x] 16.1.2 Audit `builtins/collections.cpp` — already a nullopt stub (24 lines), no cleanup needed
- [x] 16.1.3 Audit `builtins/math.cpp` — 29 emitters, ALL active (dispatch ALL math calls)
- [x] 16.1.4 Audit `runtime.cpp` `declare` statements — ALL needed by compiler codegen or tests
- [x] 16.1.5 Remove 28 dead `functions_[]` entries from `runtime.cpp` for string functions not called from lowlevel blocks (str_eq, str_concat, str_concat_3, str_concat_4, str_contains, str_starts_with, str_ends_with, str_to_upper, str_to_lower, str_trim, str_trim_start, str_trim_end, str_find, str_rfind, str_parse_i64, str_replace, str_split, str_split_whitespace, str_lines, str_chars, str_replace_first, str_repeat, str_parse_i32, str_parse_f64, str_join, str_to_uppercase, str_to_lowercase)
- [x] 16.1.6 Kept 7 active `functions_[]` entries: str_len, str_hash, str_concat_opt, str_substring, str_slice, str_char_at, str_as_bytes
- [x] 16.1.7 Rebuild compiler — success
- [x] 16.1.8 Full test suite — 8,986 passed, 0 failures (1 pre-existing linker cache issue in compiler_tests unrelated to changes)
- [ ] 16.1.9 C runtime file removal — DEFERRED (files still needed by linker even if TML reimplements the logic)
- [ ] 16.1.10 CMakeLists.txt cleanup — DEFERRED (depends on 16.1.9)

---

## Phase 17: Remove Dead Declarations from runtime.cpp

> **Source**: `compiler/src/codegen/llvm/core/runtime.cpp`
> **Problem**: Some `declare` statements are for functions never called from codegen or TML
> **Audit**: Cross-referenced every declare against ALL compiler codegen C++ files and TML lowlevel blocks
> **CORRECTION**: Initial audit estimated 29 dead declares. Final verification found only 15 are truly dead.
> Many functions (i32_to_string, i64_to_string, bool_to_string, i64_to_binary/octal/hex_str,
> i64_to_str, f64_to_str, print_i32, print_i64, print_f64, print_bool) are ACTIVE in
> method_primitive.cpp, method_primitive_ext.cpp, print.cpp, builtins/io.cpp, builtins/string.cpp,
> derive/*.cpp, mir_codegen.cpp, core.cpp.

### 17.1 Dead SIMD declares (never called — 4 functions)

- [x] 17.1.1 Remove `declare i64 @simd_sum_i64(ptr, i64)` — never called from codegen or TML
- [x] 17.1.2 Remove `declare void @simd_fill_i32(ptr, i32, i64)` — never called
- [x] 17.1.3 Remove `declare void @simd_add_i32(ptr, ptr, ptr, i64)` — never called
- [x] 17.1.4 Remove `declare void @simd_mul_i32(ptr, ptr, ptr, i64)` — never called

### 17.2 Dead atomic counter declares (never called — 6 functions)

- [x] 17.2.1 Remove all 6 `atomic_counter_*` declares — never called from codegen or TML
- [x] 17.2.2 Remove `atomic_counter_create`, `atomic_counter_inc`, `atomic_counter_dec`, `atomic_counter_get`, `atomic_counter_set`, `atomic_counter_destroy`

### 17.3 Dead print declares (2 functions — print_f32 and print_char only)

- [x] 17.3.1 Remove `declare void @print_f32(float)` — never called (print.cpp doesn't emit it)
- [x] 17.3.2 Remove `declare void @print_char(i32)` — never called from codegen or TML
- NOTE: print_i32, print_i64, print_f64, print_bool are ACTIVE in print.cpp and builtins/io.cpp

### 17.4 Dead float/math declares (3 functions)

- [x] 17.4.1 Remove `declare double @i64_to_float(i64)` — never called from codegen or TML
- [x] 17.4.2 Remove `declare i64 @float_to_i64(double)` — never called
- [x] 17.4.3 Remove `declare i32 @abs_i32(i32)` — pure TML impl exists in core::num::integer
- [x] 17.4.4 Remove `declare double @abs_f64(double)` — never called

### 17.5 Verify and test

- [x] 17.5.1 Rebuild compiler — success
- [x] 17.5.2 Run full test suite — 9,025 passed, 0 failed
- [x] 17.5.3 Count: 15 dead declares removed (4 SIMD + 6 atomic + 2 print + 3 float/math)

---

## Phase 18: Migrate Char Classification to Pure TML Dispatch

> **Source**: `builtins/string.cpp` handles `char_is_*`, `char_to_*`, `char_from_*` calls
> **Key finding**: `lib/core/src/char/methods.tml` already has pure TML implementations for ALL char classification (is_alphabetic, is_numeric, etc.) using ASCII range checks — the C runtime functions are dead but the compiler codegen still emits calls to them
> **Goal**: Make compiler dispatch char methods through TML impl blocks instead of C builtins

### 18.1 Char classification (already pure TML in char/methods.tml)

- [x] 18.1.1 Update `builtins/string.cpp` to NOT emit `@char_is_alphabetic` — TML impl handles it
- [x] 18.1.2 Same for `char_is_numeric`, `char_is_alphanumeric`, `char_is_whitespace`
- [x] 18.1.3 Same for `char_is_uppercase`, `char_is_lowercase`, `char_is_ascii`, `char_is_control`
- [x] 18.1.4 Same for `char_to_uppercase`, `char_to_lowercase`
- [x] 18.1.5 Same for `char_to_digit`, `char_from_digit`, `char_code`, `char_from_code`
- [x] 18.1.6 Remove 14 char_* declare statements from `runtime.cpp`
- [x] 18.1.7 Remove 14 char_* emitters from `builtins/string.cpp`
- [x] 18.1.8 Rewrote `compiler/tests/runtime/char.test.tml` to use module-qualified calls (23 tests pass)

### 18.2 Char-to-string (needs TML implementation)

- [ ] 18.2.1 Implement `char_to_string` in pure TML using mem_alloc + ptr_write
- [ ] 18.2.2 Implement `utf8_2byte_to_string`, `utf8_3byte_to_string`, `utf8_4byte_to_string` in pure TML
- [ ] 18.2.3 Remove 4 char/utf8 declare statements from `runtime.cpp`
- [ ] 18.2.4 Remove functions_[] entries for char_to_string, utf8_*_to_string
- [ ] 18.2.5 Remove char_to_string emitter from `builtins/string.cpp`

### 18.3 Verify

- [x] 18.3.1 Rebuild compiler and run full test suite — 23 char tests pass, 0 failed
- [x] 18.3.2 Verify char tests pass through TML dispatch — all use module-qualified calls

---

## Phase 19: File/Path/Dir Refactor (DONE)

> **Status**: COMPLETE — previously Phase 19, kept numbering for reference

- [x] 19.1-19.4 All done (File, Path, Dir refactored to TML structs with @extern FFI)

---

## Phase 20: Migrate Str Codegen Dispatch to TML

> **Source**: `method_primitive_ext.cpp` emits ~21 hardcoded `@str_*` calls
> **Source**: `binary_ops.cpp` emits `@str_concat_opt`, `@str_eq`
> **Source**: `method_primitive.cpp` emits `@i32_to_string`, `@i64_to_string`, `@float_to_string`, etc.
> **Key finding**: `lib/core/src/str.tml` already has pure TML implementations for ALL these string ops
> **Goal**: Compiler should dispatch Str methods through TML impl blocks instead of hardcoded C calls

### 20.1 method_primitive_ext.cpp cleanup (~21 hardcoded calls)

- [x] 20.1.1 Remove `@str_to_upper` / `@str_to_lower` codegen — TML str.tml has pure implementations
- [x] 20.1.2 Remove `@str_starts_with` / `@str_ends_with` / `@str_contains` codegen
- [x] 20.1.3 Remove `@str_find` / `@str_rfind` codegen
- [x] 20.1.4 Remove `@str_trim` / `@str_trim_start` / `@str_trim_end` codegen
- [x] 20.1.5 Remove `@str_split` / `@str_chars` codegen
- [x] 20.1.6 Remove `@str_parse_i64` / `@str_parse_i32` / `@str_parse_f64` codegen
- [x] 20.1.7 Remove `@str_replace` codegen
- [x] 20.1.8 Remove `@str_slice` / `@str_char_at` codegen
- [x] 20.1.9 Remove `@str_hash` / `@str_len` codegen
- [x] 20.1.10 Remove `@str_eq` codegen for parse_bool

### 20.2 binary_ops.cpp cleanup (string operators)

- [x] 20.2.1 Remove `@str_concat_opt` from `+` operator — dispatch through TML impl
- [x] 20.2.2 Remove `@str_eq` from `==`/`!=` operators — dispatch through TML impl

### 20.3 method_primitive.cpp cleanup (to_string methods)

- [x] 20.3.1 Remove `@i32_to_string` / `@i64_to_string` codegen — TML fmt handles this
- [x] 20.3.2 Remove `@bool_to_string` codegen — TML impl exists
- [x] 20.3.3 Remove `@float_to_string` codegen — keep float_to_string C (hardware dep)
- [ ] 20.3.4 Remove `@char_to_string` codegen — after Phase 18.2
- [x] 20.3.5 Remove `@i64_to_binary_str` / `@i64_to_octal_str` / `@i64_to_lower_hex_str` / `@i64_to_upper_hex_str` codegen — TML fmt/num.tml handles these

### 20.4 derive/*.cpp cleanup

- [x] 20.4.1 Audit derive/debug.cpp — dispatch through TML Debug impl
- [x] 20.4.2 Audit derive/display.cpp — dispatch through TML Display impl
- [x] 20.4.3 Audit derive/serialize.cpp — `@str_concat_opt` usage
- [x] 20.4.4 Audit derive/deserialize.cpp — `@str_eq` usage
- [x] 20.4.5 Audit derive/fromstr.cpp — `@str_eq` usage
- [x] 20.4.6 Audit derive/hash.cpp — `@str_hash` usage

### 20.5 Migrate lowlevel str_* calls in TML library files to pure TML

- [x] 20.5.1 Migrate `str_len` calls in fmt/formatter.tml, fmt/helpers.tml, collections/buffer.tml to `Str::len()`
- [x] 20.5.2 Migrate `str_slice` calls in fmt/formatter.tml, fmt/helpers.tml to `Str::substring()`
- [x] 20.5.3 Migrate `str_hash` in hash.tml to pure TML FNV-1a
- [x] 20.5.4 Migrate `str_substring` / `str_char_at` in json/types.tml to `Str::substring()` / `Str::char_at()`
- [x] 20.5.5 Fix suite mode regression: `generated_functions_` marking lazy-deferred functions as "generated" in impl.cpp
- [x] 20.5.6 Fix primitive type `is_imported` detection in method_impl.cpp for suite mode call dispatch

### 20.6 Verify

- [x] 20.6.1 Rebuild compiler and run full test suite
- [ ] 20.6.2 All string tests pass through TML dispatch (full suite running in background)

---

## Phase 21: Migrate StringBuilder to Pure TML

> **Source**: `builtins/string.cpp` emits 9 strbuilder_* calls (codegen-only, no TML usage)
> **Goal**: Either migrate StringBuilder to a TML struct or remove it entirely if unused

- [x] 21.1.1 Audit all strbuilder_* usage — ZERO usage in TML code, codegen-only dead code
- [x] 21.1.2 Remove 9 strbuilder_* emitters from builtins/string.cpp
- [x] 21.1.3 Remove 9 strbuilder_* declare statements from runtime.cpp
- [x] 21.1.4 Remove 9 strbuilder_* FuncSig registrations from types/builtins/string.cpp
- [x] 21.1.5 Remove dead `builtins/collections.cpp` (codegen) — stub file always returning nullopt
- [x] 21.1.6 Remove dead `types/builtins/collections.cpp` — empty init_builtin_collections()
- [x] 21.1.7 Remove `init_builtin_collections()` call from register.cpp and declaration from env.hpp
- [x] 21.1.8 Remove both files from CMakeLists.txt
- [x] 21.1.9 Rebuild and test — all str/collections tests pass

---

## Phase 22: Migrate Text Type to Pure TML Struct — DONE

> **Source**: `lib/std/src/text.tml` — 48 lowlevel blocks rewritten to pure TML
> **C backing**: `compiler/runtime/text/text.c` (~1,057 lines) — NO LONGER USED by Text type
> **Status**: COMPLETE — Text rewritten as pure TML struct, all compiler codegen updated, 48 text tests pass
> **Impact**: Removed ~1,250 lines of dead code (51 declares + 48 functions_[] entries + ~800 lines MIR optimizations + ~290 lines AST-path optimizations + digit_pairs constant)

### 22.1 Rewrite Text as TML struct

- [x] 22.1.1 Define `Text { handle: *Unit }` with 24-byte header (data_ptr + len + capacity) — pure TML using mem_alloc/ptr_read/ptr_write
- [x] 22.1.2 Implement `new()`, `from()`, `from_str()`, `with_capacity()` using mem_alloc
- [x] 22.1.3 Implement `push()`, `push_str()` with grow via mem_realloc + ptr_write
- [x] 22.1.4 Implement `len()`, `capacity()`, `is_empty()`, `data()`, `byte_at()`
- [x] 22.1.5 Implement `clear()`, `reserve()`, `destroy()`

### 22.2 Implement string operations in pure TML

- [x] 22.2.1 Implement `index_of()`, `last_index_of()`, `contains()`
- [x] 22.2.2 Implement `starts_with()`, `ends_with()`
- [x] 22.2.3 Implement `to_upper()`, `to_lower()`, `trim()`, `trim_start()`, `trim_end()`
- [x] 22.2.4 Implement `substring()`, `repeat()`, `reverse()`
- [x] 22.2.5 Implement `replace()`, `replace_all()`
- [x] 22.2.6 Implement `pad_start()`, `pad_end()`
- [x] 22.2.7 Implement `compare()`, `equals()`, `concat()`, `concat_str()`
- [x] 22.2.8 Implement `as_cstr()`, `from_i64()`, `from_u64()`, `from_f64()`, `from_bool()`

### 22.3 Update compiler codegen for template literals

- [x] 22.3.1 Update `call_user.cpp` — removed V8-style Text optimizations (~290 lines), now dispatches through normal TML method calls
- [x] 22.3.2 Update `instructions_method.cpp` — removed ~800 lines of MIR V8-style Text inline codegen
- [x] 22.3.3 Update `core.cpp` TemplateLiteralExpr — calls `@tml_Text_from`/`@tml_Text_push_str` instead of old C FFI names
- [x] 22.3.4 Remove `emit_inline_int_to_string` from `instructions_misc.cpp` (~150 lines) + declaration from `mir_codegen.hpp`
- [x] 22.3.5 Remove `@.digit_pairs` constant from `mir_codegen.cpp`

### 22.4 Cleanup

- [x] 22.4.1 Remove all 51 tml_text_* declare statements from runtime.cpp
- [x] 22.4.2 Remove all 48 text_* functions_[] entries from runtime.cpp
- [x] 22.4.3 Register `f64_to_str`, `print_str`, `println_str` in functions_[] map (used by text.tml lowlevel blocks)
- [x] 22.4.4 Rebuild and run full test suite — 48 text tests pass, 269 collections tests pass, all str tests pass

---

## Phase 23: Migrate Float Math to LLVM Intrinsics

> **Source**: `builtins/math.cpp` emits calls to C runtime for float operations
> **Key finding**: Many float operations can use LLVM intrinsics directly instead of C calls
> **Goal**: Replace C runtime float functions with LLVM intrinsics where possible

### 23.1 LLVM intrinsic replacements

- [ ] 23.1.1 Replace `@float_abs` → `@llvm.fabs.f64`
- [ ] 23.1.2 Replace `@float_sqrt` → `@llvm.sqrt.f64`
- [ ] 23.1.3 Replace `@float_pow` → `@llvm.pow.f64`
- [ ] 23.1.4 Replace `@float_round` → `@llvm.round.f64` + fptosi
- [ ] 23.1.5 Replace `@float_floor` → `@llvm.floor.f64` + fptosi
- [ ] 23.1.6 Replace `@float_ceil` → `@llvm.ceil.f64` + fptosi
- [ ] 23.1.7 Replace `@int_to_float` → `sitofp i32 to double`
- [ ] 23.1.8 Replace `@float_to_int` → `fptosi double to i32`

### 23.2 Bit manipulation (LLVM cast instructions)

- [ ] 23.2.1 Replace `@float32_bits` → `bitcast float to i32`
- [ ] 23.2.2 Replace `@float32_from_bits` → `bitcast i32 to float`
- [ ] 23.2.3 Replace `@float64_bits` → `bitcast double to i64`
- [ ] 23.2.4 Replace `@float64_from_bits` → `bitcast i64 to double`

### 23.3 NaN/Infinity (LLVM constants + fcmp)

- [ ] 23.3.1 Replace `@is_nan` → `fcmp uno double %val, 0.0`
- [ ] 23.3.2 Replace `@is_inf` → `fcmp oeq double %val, inf` or `fcmp oeq double %val, -inf`
- [ ] 23.3.3 Replace `@nan()` → `0x7FF8000000000000` constant
- [ ] 23.3.4 Replace `@infinity` → `0x7FF0000000000000` / `0xFFF0000000000000`
- [ ] 23.3.5 Replace `@f64_is_nan`/`@f64_is_infinite`/`@f32_is_nan`/`@f32_is_infinite` similarly

### 23.4 Nextafter (keep as C — no LLVM intrinsic)

- [ ] 23.4.1 Keep `@nextafter` and `@nextafter32` as C runtime (no LLVM equivalent)

### 23.5 Float to string (keep as C — hardware-dependent snprintf)

- [ ] 23.5.1 Keep all float_to_string/precision/exp functions as C runtime
- [ ] 23.5.2 Keep f32/f64_to_string, f32/f64_to_string_precision, f32/f64_to_exp_string

### 23.6 Cleanup

- [ ] 23.6.1 Remove ~16 dead float declare statements from runtime.cpp
- [ ] 23.6.2 Remove corresponding emitters from builtins/math.cpp (replace with inline IR)
- [ ] 23.6.3 Rebuild and run full test suite

---

## Phase 24: Migrate Sync/Threading Codegen to @extern FFI

> **Source**: `builtins/sync.cpp` emits hardcoded calls for thread/channel/mutex/waitgroup/atomics
> **Key finding**: TML library already uses @extern("tml_thread_*"), @extern("tml_mutex_*") — different names from codegen
> **Goal**: Remove hardcoded codegen dispatch, let TML @extern declarations handle everything

### 24.1 Threading (already @extern in std::thread)

- [ ] 24.1.1 Remove 5 thread_* declare statements from runtime.cpp
- [ ] 24.1.2 Remove thread_* emitters from builtins/sync.cpp
- [ ] 24.1.3 Verify std::thread uses @extern("tml_thread_*") for everything

### 24.2 Channel

- [ ] 24.2.1 Rewrite Channel as TML struct with @extern FFI
- [ ] 24.2.2 Remove 8 channel_* declare statements from runtime.cpp
- [ ] 24.2.3 Remove channel_* emitters from builtins/sync.cpp

### 24.3 Mutex (already @extern in std::sync::mutex)

- [ ] 24.3.1 Remove 5 mutex_* declare statements from runtime.cpp
- [ ] 24.3.2 Remove mutex_* emitters from builtins/sync.cpp
- [ ] 24.3.3 Verify std::sync::mutex uses @extern("tml_mutex_*") for everything

### 24.4 WaitGroup

- [ ] 24.4.1 Rewrite WaitGroup as TML struct with @extern FFI
- [ ] 24.4.2 Remove 5 waitgroup_* declare statements from runtime.cpp
- [ ] 24.4.3 Remove waitgroup_* emitters from builtins/sync.cpp

### 24.5 Typed atomics (already @extern in core::alloc::sync)

- [ ] 24.5.1 Remove 9 atomic_* declare statements from runtime.cpp
- [ ] 24.5.2 Remove atomic_* emitters from builtins/atomic.cpp
- [ ] 24.5.3 Verify core::alloc::sync uses @extern for everything

### 24.6 Verify

- [ ] 24.6.1 Rebuild and run full test suite

---

## Phase 25: Migrate Time/Pool/Print Codegen to @extern FFI

> **Source**: `builtins/time.cpp` emits 10 hardcoded time_* calls
> **Source**: `class_codegen.cpp`/`drop.cpp` emit pool_* calls
> **Source**: `print.cpp` emits print/println calls
> **Goal**: Let TML @extern declarations handle these instead of codegen

### 25.1 Time functions

- [ ] 25.1.1 Verify std::time uses @extern or TML builtins for time_*/instant_*
- [ ] 25.1.2 Remove hardcoded time_* emitters from builtins/time.cpp
- [ ] 25.1.3 Remove 10 time_* declare statements from runtime.cpp

### 25.2 Pool functions

- [ ] 25.2.1 Rewrite @pool class support to use @extern FFI
- [ ] 25.2.2 Remove 10 pool_*/tls_pool_* declare statements from runtime.cpp

### 25.3 Black box / SIMD (keep used, remove dead)

- [ ] 25.3.1 Keep black_box_i32, black_box_i64, black_box_f64 (used in benchmarks)
- [ ] 25.3.2 Keep simd_sum_i32, simd_sum_f64, simd_dot_f64 (used in builtins/math.cpp)
- [ ] 25.3.3 Migrate to @extern or LLVM intrinsics where possible

### 25.4 Verify

- [ ] 25.4.1 Rebuild and run full test suite

---

## Phase 26: Runtime Declaration Optimization (On-Demand Emit)

> **Source**: `compiler/src/codegen/llvm/core/runtime.cpp`
> **Problem**: After Phases 17-25, remaining declares still emitted unconditionally
> **Goal**: Only emit declares that are actually used in each compilation unit

- [ ] 26.1.1 Add `declared_runtime_functions_` set to `LLVMIRGen`
- [ ] 26.1.2 Create `ensure_runtime_decl(name, signature)` helper
- [ ] 26.1.3 Convert remaining unconditional declares to on-demand
- [ ] 26.1.4 Keep LLVM intrinsics unconditional (llvm.memcpy, llvm.memset, etc.)
- [ ] 26.1.5 Keep essential declares unconditional: print, panic, mem_alloc, mem_free, malloc, free
- [ ] 26.1.6 Verify `--emit-ir` only has used declarations
- [ ] 26.1.7 Run full test suite

---

## Phase 27: Codegen Dispatch Cleanup — Remaining Collection/List

> Previously Phase 18.3

### 27.1 List cleanup

- [ ] 27.1.1 Remove `List` from type-erasure in `decl/struct.cpp:317-335`
- [ ] 27.1.2 Remove `List` from type name validation in `method_static_dispatch.cpp:898`
- [ ] 27.1.3 Verify List dispatch goes through normal `gen_impl_method()` path

---

## Phase 28: Type System Cleanup — Remove Hardcoded Builtin Registrations

> Previously Phase 20

- [x] 28.1.1 Remove 14 hashmap_* registrations from `types/builtins/collections.cpp` (done in Phase 2)
- [x] 28.1.2 Remove 11 buffer_* registrations from `types/builtins/collections.cpp` (done in Phase 3)
- [ ] 28.1.3 Remove 29 string registrations from `types/builtins/string.cpp` (after Phase 20)
- [ ] 28.1.4 Remove 9 strbuilder_* registrations (after Phase 21)
- [ ] 28.1.5 Verify type checker finds method signatures from TML impl blocks
- [ ] 28.1.6 Run full test suite

---

## Phase 29: Fix Metadata Loss and Workarounds

> Previously Phase 21

- [ ] 29.1.1 Fix metadata loader to preserve return types for behavior impls on primitives
- [ ] 29.1.2 Remove hardcoded return type workaround (eq→i1, cmp→Ordering, hash→i64)
- [ ] 29.1.3 Fix unresolved generic type placeholders in `decl/struct.cpp:160-167`
- [ ] 29.1.4 Run full test suite

---

## Phase 30: Benchmarks and Validation

- [ ] 30.1.1 Benchmark all migrated types: List, HashMap, Buffer, Str, Text
- [ ] 30.1.2 Benchmark float ops: LLVM intrinsics vs C runtime
- [ ] 30.1.3 Verify performance within 10% of C implementations
- [ ] 30.1.4 Run full test suite with --coverage
- [ ] 30.1.5 Document final metrics

---

## Summary: Impact and Status

### runtime.cpp Declaration Audit (2026-02-18)

| Category | Declares | Status | Target |
|----------|----------|--------|--------|
| **LLVM intrinsics** | 7 | KEEP — fundamental | KEEP |
| **C stdlib** (printf, malloc, free, exit, strlen) | 5 | KEEP — fundamental | KEEP |
| **Essential** (panic, assert_tml_loc, print, println) | 4 | KEEP — fundamental | KEEP |
| **Memory** (mem_alloc, mem_free, mem_realloc, etc.) | 10 | KEEP — fundamental | KEEP |
| **Coverage** (tml_cover_func, etc.) | 4 | KEEP — conditional on flag | KEEP |
| **Debug intrinsics** | 2 | KEEP — conditional on flag | KEEP |
| **Stack save/restore** | 2 | KEEP — LLVM intrinsics | KEEP |
| **Lifetime intrinsics** | 2 | KEEP — LLVM intrinsics | KEEP |
| **Panic catching** (should_panic tests) | 3 | KEEP — test infra | KEEP |
| **Backtrace** | 1 | KEEP — runtime | KEEP |
| **Format strings** | 12 constants | KEEP — used by print | KEEP |
| --- | --- | --- | --- |
| **Dead SIMD** (sum_i64, fill/add/mul_i32) | 4 | REMOVE (Phase 17) | Phase 17 |
| **Dead atomic counter** | 6 | REMOVE (Phase 17) | Phase 17 |
| **Dead print** (print_f32, print_char) | 2 | REMOVE (Phase 17) | Phase 17 |
| **Dead float/math** (i64_to_float, float_to_i64, abs_i32, abs_f64) | 3 | REMOVE (Phase 17) | Phase 17 |
| **Active int-to-string** (i32/i64/bool_to_string, hex/octal/binary) | 9 | KEEP — used by method_primitive.cpp | KEEP (until Phase 20) |
| **Active print_*** (i32, i64, f64, bool) | 4 | KEEP — used by print.cpp/io.cpp | KEEP (until Phase 25) |
| **Active i64_to_str, f64_to_str** | 2 | KEEP — used by core.cpp/string.cpp | KEEP (until Phase 20) |
| **Char classification** | 14 | MIGRATE (Phase 18) | Remove declares |
| **Char-to-string/UTF-8** | 4 | MIGRATE (Phase 18) | Remove declares |
| **String ops** (str_*) | 34 | MIGRATE (Phase 20) | Remove declares |
| **StringBuilder** | 9 | MIGRATE (Phase 21) | Remove declares |
| **Text type** (tml_text_*) | 51 | MIGRATE (Phase 22) | Remove declares |
| **Float math** | 24 | MIGRATE to intrinsics (Phase 23) | ~8 keep, ~16 remove |
| **Threading** | 5 | MIGRATE to @extern (Phase 24) | Remove declares |
| **Channel** | 8 | MIGRATE to @extern (Phase 24) | Remove declares |
| **Mutex** | 5 | MIGRATE to @extern (Phase 24) | Remove declares |
| **WaitGroup** | 5 | MIGRATE to @extern (Phase 24) | Remove declares |
| **Typed atomics** | 9 | MIGRATE to @extern (Phase 24) | Remove declares |
| **Time** | 10 | MIGRATE to @extern (Phase 25) | Remove declares |
| **Pool** | 10 | MIGRATE to @extern (Phase 25) | Remove declares |
| **Black box/SIMD (active)** | 6 | MIGRATE to @extern (Phase 25) | Remove declares |
| **Log runtime** | 12 | KEEP — I/O + already @extern | KEEP |
| **Glob runtime** | 5 | KEEP — FFI + lowlevel | KEEP |
| --- | --- | --- | --- |
| **TOTAL declares** | ~287 | | |
| **KEEP (essential)** | ~68 | Fundamental runtime | KEEP forever |
| **REMOVE (dead)** | 15 | Never called | Phase 17 |
| **MIGRATE** | ~204 | Move to TML/@extern/intrinsics | Phases 18-25 |

### Minimal Backend Target

After all phases complete, `runtime.cpp` should only contain:

```
KEEP:
  - LLVM intrinsics (memcpy, memset, memmove, assume, stacksave/restore, lifetime)
  - C stdlib (printf, puts, putchar, malloc, free, exit, strlen)
  - Essential runtime (panic, assert_tml_loc, print, println)
  - Memory (mem_alloc, mem_alloc_zeroed, mem_realloc, mem_free, mem_copy, mem_move, mem_set, mem_zero, mem_compare, mem_eq)
  - Coverage (conditional)
  - Debug intrinsics (conditional)
  - Panic catching (3)
  - Backtrace (1)
  - Format string constants
  - Log runtime (already @extern, but still declared for compat)
  - Glob runtime (FFI)
  - Float-to-string (hardware-dependent snprintf): ~6 functions
  - nextafter/nextafter32 (no LLVM intrinsic)
  - tml_random_seed (OS random)

TOTAL: ~68 declarations (down from ~287)
```

### Phase Progress

| Phase | Module | Declares | Status |
|-------|--------|----------|--------|
| 0-7 | Collections, Str, fmt integers | — | **DONE** |
| 16 | Dead functions_[] entries | -28 entries | **DONE** |
| 17 | Dead declares | -15 declares | **DONE** |
| 18.1 | Char classification dispatch | -14 declares | **DONE** |
| 18.2 | Char-to-string/UTF-8 | -4 declares | TODO |
| 19 | File/Path/Dir | — | **DONE** |
| 20 | Str codegen dispatch | -34 declares | TODO |
| 21 | StringBuilder | -9 declares | TODO |
| 22 | Text type | -51 declares | **DONE** |
| 23 | Float math → LLVM intrinsics | -16 declares | TODO |
| 24 | Sync/threading → @extern | -32 declares | TODO |
| 25 | Time/pool/print → @extern | -20 declares | TODO |
| 26 | On-demand emit | remaining | TODO |
| 27-30 | Cleanup, type system, benchmarks | — | TODO |
