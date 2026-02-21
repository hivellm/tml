# Tasks: Migrate C Runtime Pure Algorithms to TML

**Status**: In Progress (Phases 0-7, 11, 15-27, 29, 48-50 complete; Phases 28, 30 remaining — runtime.cpp: 287→77 declares, at target ~68)

**Scope**: ~287 runtime.cpp declares to minimize → ~68 essential; runtime C code actively shrinking via dead code removal

**Current metrics** (2026-02-20): 77.0% coverage, 9,035 tests across 785 files, 77 runtime.cpp declares remaining (at target)

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
- [x] 1.2.9 Legacy `@list_len`/`@list_get` calls — VERIFIED CLEAN: no patterns found in codegen (loop.cpp, collections.cpp)
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
- [x] 4.1.6 Implement `str_as_bytes()` — pure identity function in str.tml (Str and ref [U8] are both ptr in LLVM IR)
- [x] 4.1.7 Implement `str_hash()` in `lib/core/src/hash.tml` — FNV-1a in pure TML (impl Hash for Str)
- [x] 4.1.8 Update both standalone functions AND `impl Str` methods in `str.tml`
- [x] 4.1.9 Remove corresponding emitters from `builtins/string.cpp` — DONE (only float intrinsics remain; all str builtins removed in Phases 18/31/36)
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
- [x] 5.1.11 Remove corresponding emitters from `builtins/string.cpp` — DONE (Phases 18/31/36)
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
- [x] 6.1.5 Remove corresponding emitters from `builtins/string.cpp` — DONE (Phases 18/31/36)
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
> **C backing**: `compiler/runtime/text/text.c` (1,057 lines) — DELETED
> **Status**: COMPLETE — migrated to pure TML in Phase 22 (1,075 lines, 60+ methods, all using memory intrinsics)

- [x] 11.1.1 Fix type checker to infer correct return types for memory intrinsics in lowlevel blocks — DONE (Phase 22)
- [x] 11.1.2 Rewrite `Text` struct as `{ handle: *Unit }` with 24-byte header (data_ptr + len + capacity) — DONE (Phase 22)
- [x] 11.1.3-11.1.12 Implement all text operations in pure TML — DONE (Phase 22, 48 tests pass)

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

- [x] 15.1.1 pool.c — KEEP: actively used by @pool decorator (pool_acquire/release, tls_pool_acquire/release), compiled in CMakeLists.txt
- [x] 15.1.2 profile_runtime.c — ALREADY REMOVED (file does not exist on disk)
- [x] 15.1.3 io.c — ALREADY REMOVED (file does not exist on disk; I/O inline in runtime.cpp)
- [x] 15.1.4 No action needed — all files accounted for

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
- [x] 16.1.9 C runtime file removal — DONE: text.c, thread.c, async.c all deleted from disk (Phases 22, 24, 26)
- [x] 16.1.10 CMakeLists.txt cleanup — DONE: dead files removed from build in Phase 26

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

### 18.2 Char-to-string — DONE (migrated to pure TML)

- [x] 18.2.1 Implement `char_to_string` in pure TML using mem_alloc + ptr_write (char/methods.tml, char/decode.tml)
- [x] 18.2.2 Implement `utf8_2byte_to_string`, `utf8_3byte_to_string`, `utf8_4byte_to_string` in pure TML (replaced lowlevel blocks with mem_alloc + ptr_write in char/methods.tml, char/decode.tml)
- [x] 18.2.3 Remove 4 char/utf8 declare statements from `runtime.cpp`
- [x] 18.2.4 Remove 4 functions_[] entries + 4 FuncSig registrations from types/builtins/string.cpp
- [x] 18.2.5 Remove char_to_string emitter from `builtins/string.cpp`
- [x] 18.2.6 Update Char.to_string() in method_primitive.cpp to inline mem_alloc (no C runtime)
- [x] 18.2.7 Update 10 lowlevel blocks in 7 TML files (char/methods, char/decode, ascii/char, fmt/traits, fmt/helpers, fmt/formatter, fmt/impls) to pure TML
- [x] 18.2.8 Update fmt_unit.test.tml to use Char.to_string() method instead of internal function

### 18.3 Verify

- [x] 18.3.1 Rebuild compiler and run full test suite — 105 char, 404 fmt, 67 ascii, 28 unicode tests pass
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
- [x] 20.3.4 Remove `@char_to_string` codegen — Phase 51: added Char to is_primitive_type lambda, lazy-lib now resolves correctly
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
- [x] 20.6.2 All string tests pass through TML dispatch (241 str tests, 413 fmt tests — all via TML impls except Char inline codegen)

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

## Phase 23: Migrate Float Math to LLVM Intrinsics — DONE

> **Source**: `builtins/math.cpp` emits calls to C runtime for float operations
> **Status**: COMPLETE — 16 C runtime calls replaced with LLVM intrinsics/instructions, 16 declares removed
> **Impact**: All math (43 tests), intrinsics (86 tests), float (6 tests), fmt (404 tests) pass

### 23.1 LLVM intrinsic replacements

- [x] 23.1.1 Replace `@float_abs` → `@llvm.fabs.f64`
- [x] 23.1.2 Replace `@float_sqrt` → `@llvm.sqrt.f64`
- [x] 23.1.3 Replace `@float_pow` → `@llvm.pow.f64` (also updated method_primitive.cpp integer `.pow()`)
- [x] 23.1.4 Replace `@float_round` → `@llvm.round.f64` + fptosi
- [x] 23.1.5 Replace `@float_floor` → `@llvm.floor.f64` + fptosi
- [x] 23.1.6 Replace `@float_ceil` → `@llvm.ceil.f64` + fptosi
- [x] 23.1.7 Replace `@int_to_float` → `sitofp <type> to double` (handles all int types)
- [x] 23.1.8 Replace `@float_to_int` → `fptosi double to i32`

### 23.2 Bit manipulation (LLVM cast instructions)

- [x] 23.2.1 Replace `@float32_bits` → `bitcast float to i32`
- [x] 23.2.2 Replace `@float32_from_bits` → `bitcast i32 to float`
- [x] 23.2.3 Replace `@float64_bits` → `bitcast double to i64`
- [x] 23.2.4 Replace `@float64_from_bits` → `bitcast i64 to double`

### 23.3 NaN/Infinity (LLVM constants + fcmp)

- [x] 23.3.1 Replace `@is_nan` → `fcmp uno double %val, 0.0`
- [x] 23.3.2 Replace `@is_inf` → `fcmp oeq` against `0x7FF0000000000000`/`0xFFF0000000000000` constants
- [x] 23.3.3 Replace `@nan()` → `0x7FF8000000000000` constant
- [x] 23.3.4 Replace `@infinity` → `select` between `0x7FF0000000000000`/`0xFFF0000000000000`

### 23.4 Nextafter (keep as C — no LLVM intrinsic)

- [x] 23.4.1 Keep `@nextafter` and `@nextafter32` as C runtime (no LLVM equivalent)

### 23.5 Float to string (keep as C — hardware-dependent snprintf)

- [x] 23.5.1 Keep all float_to_string/precision/exp functions as C runtime
- [x] 23.5.2 Keep f32/f64_to_string, f32/f64_to_string_precision, f32/f64_to_exp_string
- [x] 23.5.3 Keep f64/f32_is_nan, f64/f32_is_infinite (used by fmt/float.tml lowlevel blocks)

### 23.6 Cleanup

- [x] 23.6.1 Remove 16 float declare statements from runtime.cpp (8 math + 4 bitcast + 4 NaN/inf)
- [x] 23.6.2 Replace emitters in builtins/math.cpp with inline LLVM IR
- [x] 23.6.3 Update method_primitive.cpp `.pow()` to use `@llvm.pow.f64`
- [x] 23.6.4 Rebuild and run full test suite — all pass

---

## Phase 24: Migrate Sync/Threading Codegen to @extern FFI

> **Source**: `builtins/sync.cpp` emits hardcoded calls for thread/channel/mutex/waitgroup/atomics
> **Key finding**: TML library already uses @extern("tml_thread_*"), @extern("tml_mutex_*") — different names from codegen
> **Goal**: Remove hardcoded codegen dispatch, let TML @extern declarations handle everything

### 24.1 Threading (already @extern in std::thread)

- [x] 24.1.1 Remove 5 thread_* declare statements from runtime.cpp
- [x] 24.1.2 Remove thread_* emitters from builtins/sync.cpp
- [x] 24.1.3 Verify std::thread uses @extern("tml_thread_*") for everything

### 24.2 Channel

- [x] 24.2.1 Remove 8 channel_* declare statements from runtime.cpp (dead code — TML uses MPSC pattern)
- [x] 24.2.2 Remove channel_* emitters from builtins/sync.cpp

### 24.3 Mutex (already @extern in std::sync::mutex)

- [x] 24.3.1 Remove 5 mutex_* declare statements from runtime.cpp
- [x] 24.3.2 Remove mutex_* emitters from builtins/sync.cpp
- [x] 24.3.3 Verify std::sync::mutex uses @extern("tml_mutex_*") for everything

### 24.4 WaitGroup

- [x] 24.4.1 Remove 5 waitgroup_* declare statements from runtime.cpp (dead code — no WaitGroup in TML)
- [x] 24.4.2 Remove waitgroup_* emitters from builtins/sync.cpp

### 24.5 Typed atomics

- [x] 24.5.1 Remove typed atomic emitters from builtins/atomic.cpp
- [x] 24.5.2 Remove typed atomic FuncSig from types/builtins/atomic.cpp
- [x] 24.5.3 Remove thread/channel/mutex/waitgroup FuncSig from types/builtins/sync.cpp (keep spinlock)
- [x] 24.5.4 Add 9 @extern typed atomic declarations to core::sync.tml module
- [x] 24.5.5 Update ops_arith_mul_div.test.tml to import from core::sync (no @extern in tests)
- [x] 24.5.6 Keep 9 typed atomic declares in runtime.cpp (required for module-level IR placement)

### 24.6 Verify

- [x] 24.6.1 Rebuild compiler — success
- [x] 24.6.2 sync tests: 699 passed, 0 failed
- [x] 24.6.3 thread tests: 38 passed, 0 failed
- [x] 24.6.4 core alloc tests: all passed
- [x] 24.6.5 ops_arith_mul_div tests: 48 passed, 0 failed
- [x] 24.6.6 Net reduction: -23 declares (5 thread + 8 channel + 5 mutex + 5 waitgroup), codegen emitters and FuncSig entries removed

---

## Phase 24b: String.c Dead Code Removal — DONE

> **Source**: `compiler/runtime/text/string.c` — 1,202 lines → ~490 lines
> **Goal**: Remove dead C functions whose TML equivalents exist in str.tml, break string.c→collections.c dependency
> **Impact**: -18 declares from runtime.cpp, -720 lines from string.c

### 24b.1 Remove dead declares from runtime.cpp

- [x] 24b.1.1 Remove 18 dead string declares (str_concat/3/4, str_trim_start/end, str_find/rfind, str_parse_i64/i32/f64, str_replace/first, str_split/whitespace, str_lines/chars, str_repeat, str_join)

### 24b.2 Remove dead functions from string.c

- [x] 24b.2.1 Remove str_split, str_chars, str_split_whitespace, str_lines, str_join (list_*-dependent)
- [x] 24b.2.2 Remove str_find, str_rfind, str_replace, str_replace_first, str_repeat
- [x] 24b.2.3 Remove str_parse_i32, str_parse_i64, str_parse_f64
- [x] 24b.2.4 Remove str_trim_start, str_trim_end
- [x] 24b.2.5 Remove str_concat (legacy), str_concat_3, str_concat_4
- [x] 24b.2.6 Remove char_is_* (8 functions), char_to_*/char_from_* (6 functions), char_code, char_from_code
- [x] 24b.2.7 Remove strbuilder_* (9 functions) + StringBuilder typedef
- [x] 24b.2.8 Remove static buffers: str_buffer2, str_repeat_buffer, str_join_buffer
- [x] 24b.2.9 Remove extern TmlList* forward declarations
- [x] 24b.2.10 Keep str_as_bytes (used by Str::as_bytes() lowlevel block in str.tml)

### 24b.3 Verify

- [x] 24b.3.1 Rebuild compiler — success
- [x] 24b.3.2 str tests: 241 passed, 0 failed (20 files)
- [x] 24b.3.3 fmt tests: 404 passed, 0 failed (32 files)
- [x] 24b.3.4 crypto tests: 476 passed, 0 failed (20 files)
- [x] 24b.3.5 sync tests: 699 passed, 0 failed (54 files)
- [x] 24b.3.6 thread tests: 38 passed, 0 failed (2 files)

---

## Phase 25: Migrate Time Builtins to @extern FFI (DONE — 2026-02-18)

> Commit: `1003f7f`

- [x] 25.1.1 Remove hardcoded time_ms, time_us, time_ns, elapsed_ms, elapsed_us, sleep_us emitters from builtins/time.cpp
- [x] 25.1.2 Remove 10 time_* declare statements from runtime.cpp
- [x] 25.1.3 Verify std::time::Instant and std::time::sleep use @extern("c") FFI
- [x] 25.1.4 Tests: 8334 passed

---

## Phase 26: Remove Dead C Files from Build (DONE — 2026-02-18)

> Commit: `b75a394`

- [x] 26.1.1 Remove text.c from CMakeLists.txt (dead since Phase 22)
- [x] 26.1.2 Remove thread.c from CMakeLists.txt (dead since Phase 24)
- [x] 26.1.3 Remove async.c from CMakeLists.txt (dead since Phase 24)
- [x] 26.1.4 Rebuild compiler — success
- [x] 26.1.5 Tests: 8334 passed

---

## Phase 27: Float NaN/Inf → LLVM IR + Dead Math.c Removal (DONE — 2026-02-18)

> Commit: `45db68a`

- [x] 27.1.1 Replace is_nan codegen with LLVM `fcmp uno` instruction
- [x] 27.1.2 Replace is_infinite codegen with LLVM `fabs` + `fcmp oeq` with infinity
- [x] 27.1.3 Remove 16 dead math.c functions (is_nan, is_inf, float_abs, float_sqrt, etc.)
- [x] 27.1.4 Remove corresponding declares from runtime.cpp
- [x] 27.1.5 math.c: 412 → 236 lines
- [x] 27.1.6 Tests: 8334 passed

---

## Phase 48: Dead Time/Header Cleanup (DONE — 2026-02-20)

> Commit: `b601024`

- [x] 48.1.1 Remove 9 dead functions from time.c (time_ms, time_us, sleep_us, elapsed_ms/us/ns, elapsed_secs, duration_as_millis_f64, duration_format_secs)
- [x] 48.1.2 Remove 8 stale declarations from essential.h (print_f32, str_concat_opt, 6 time functions)
- [x] 48.1.3 Remove 5 dead lowlevel bindings from core/time.tml (time_ms, time_us, elapsed_ms, duration_as_millis, duration_as_secs)
- [x] 48.1.4 Tests: 9,045 passed

---

## Phase 49: Ghost String/Assert Declarations Removed (DONE — 2026-02-20)

> Commit: `93655e6`

- [x] 49.1.1 Remove 17 ghost string declarations from essential.h (str_len, str_eq, str_hash, str_concat/_3/_4/_n, str_substring, str_slice, str_contains, str_starts_with, str_ends_with, str_to_upper, str_to_lower, str_trim, str_char_at, char_to_string — none had C implementations)
- [x] 49.1.2 Remove dead assert_tml() 2-arg function from essential.c (~60 lines, codegen only emits assert_tml_loc)
- [x] 49.1.3 Remove 3 dead str_concat declares from mir_codegen.cpp (MIR uses str_concat_opt inline)
- [x] 49.1.4 Update essential.h header comment (remove string section, update assert reference)
- [x] 49.1.5 Tests: 9,035 passed

---

## Phase 50: Remove Hardcoded Str to_string/debug_string Codegen (DONE — 2026-02-20)

> **Source**: `method_primitive.cpp` had hardcoded inline IR for `Str::to_string` (identity) and `Str::debug_string` (quote wrapping)
> **Goal**: Dispatch through TML Display/Debug behavior impls in `fmt/impls.tml`
> **Finding**: Str dispatch works via lazy-lib; Char dispatch fails (lazy-lib cannot resolve char_to_str dependency chain)

- [x] 50.1.1 Remove hardcoded `Str::to_string` codegen (was identity return, ~5 lines)
- [x] 50.1.2 Remove hardcoded `Str::debug_string` codegen (was str_concat_opt quote wrapping, ~10 lines)
- [x] 50.1.3 Keep Char hardcoded block (lazy-lib UNRESOLVED: @tml_Char_to_string, @tml_Char_debug_string)
- [x] 50.1.4 Tests: 9,035 passed (char 105, fmt 413, str 241 all clean)

---

## Phase 28: On-Demand Declaration Emit

> **Goal**: Only emit runtime declares that are actually used in each compilation unit

- [x] 28.1.1 Make `%struct.HashMapIter` conditional on `needs_collections` (imports std::collections)
- [x] 28.1.2 Make `%struct.RawThread`/`%struct.RawPtr` conditional on `needs_thread` (imports std::thread)
- [x] 28.1.3 Keep LLVM intrinsics, C stdlib, memory, I/O, string utils, random_seed unconditional
- [x] 28.1.4 Use `library_ir_only` mode to force all flags true for test DLLs
- [x] 28.1.5 Run full test suite — 9035/9035 pass

---

## Phase 29: Type System Cleanup — Remove Hardcoded Builtin Registrations

- [x] 29.1.1 Remove 14 hashmap_* registrations from `types/builtins/collections.cpp` (done in Phase 2)
- [x] 29.1.2 Remove 11 buffer_* registrations from `types/builtins/collections.cpp` (done in Phase 3)
- [x] 29.1.3 Remove 29 string registrations from `types/builtins/string.cpp` (done — file is now a no-op stub)
- [x] 29.1.4 Remove 9 strbuilder_* registrations (done — removed with Phase 21)
- [x] 29.1.5 Verify type checker finds method signatures from TML impl blocks (verified — all str/fmt/char tests pass)
- [x] 29.1.6 Run full test suite (9,035 passed)

---

## Phase 30: Final Cleanup and Validation

- [x] 30.1.1 Delete dead text.c from disk — ALREADY DONE (directory doesn't exist)
- [x] 30.1.2 Return type workaround was dead code — never triggered (all methods inlined or dispatched via lazy-lib)
- [x] 30.1.3 Removed 40-line dead workaround from method_prim_behavior.cpp (Phase 51)
- [ ] 30.1.4 Benchmark all migrated types: List, HashMap, Buffer, Str, Text (DEFERRED — no perf regressions observed)
- [x] 30.1.5 Run full test suite with --coverage: 9,035 passed, 0 failed, 77.0% coverage
- [x] 30.1.6 Final metrics (2026-02-20): 77 runtime.cpp declares (49 unconditional + 28 conditional), 11 MIR declares, 15 compiled .c files, 21 .c files on disk, 5 inline IR functions, 22 functions_[] entries, 9,035 tests, 77.0% coverage

---

## Summary: Impact and Status

### runtime.cpp Declaration Audit (2026-02-20, updated Phase 50)

**Current state**: 77 declares remain in LLVM runtime.cpp (49 unconditional + 28 conditional), 11 in MIR mir_codegen.cpp (down from ~287 at start)

| Category | Declares | Status |
|----------|----------|--------|
| **C stdlib** (printf, puts, putchar, malloc, free, exit, strlen, strcmp, memcmp) | 9 | KEEP |
| **LLVM intrinsics** (memcpy, memmove, memset, assume, stacksave, stackrestore) | 6 | KEEP |
| **Lifetime intrinsics** (lifetime.start, lifetime.end) | 2 | KEEP |
| **Essential** (panic, assert_tml_loc) | 2 | KEEP |
| **I/O** (print, println, print_i32, print_i64, print_f64, print_bool) | 6 | KEEP |
| **Memory** (mem_alloc, alloc_zeroed, realloc, free, copy, move, set, zero, compare, eq) | 10 | KEEP |
| **Float formatting** (f64/f32_to_string, precision, exp) | 6 | KEEP |
| **Pool** (pool_acquire/release, tls_pool_acquire/release) | 4 | KEEP |
| **Panic catching** (should_panic, panic_message_contains, enable_backtrace) | 3 | KEEP |
| **Random** (tml_random_seed) | 1 | KEEP |
| **Conditional: Coverage** (tml_cover_func, etc.) | 4 | KEEP |
| **Conditional: Debug** | 2 | KEEP |
| **Conditional: Atomics** | 9 | KEEP |
| **Conditional: Logging** | 12 | KEEP |
| **Conditional: instrprof** | 1 | KEEP |
| --- | --- | --- |
| **TOTAL declares** | 77 | AT TARGET |

All 77 remaining declares are essential — no further migration candidates exist.
The inline IR functions (str_eq, str_concat_opt, 3x black_box) are embedded in runtime.cpp as `define internal`.

### Phase Progress

| Phase | Module | Declares | Status |
|-------|--------|----------|--------|
| 0-7 | Collections, Str, fmt integers | — | **DONE** |
| 16 | Dead functions_[] entries | -28 entries | **DONE** |
| 17 | Dead declares | -15 declares | **DONE** |
| 18.1 | Char classification dispatch | -14 declares | **DONE** |
| 18.2 | Char-to-string/UTF-8 | -4 declares | **DONE** |
| 19 | File/Path/Dir | — | **DONE** |
| 20 | Str codegen dispatch | -34 declares | **DONE** (except 20.3.4 Char — blocked by lazy-lib) |
| 21 | StringBuilder | -9 declares | **DONE** |
| 22 | Text type | -51 declares | **DONE** |
| 23 | Float math → LLVM intrinsics | -16 declares | **DONE** |
| 24 | Sync/threading → @extern | -23 declares | **DONE** |
| 24b | String.c dead code removal | -18 declares, -720 lines C | **DONE** |
| 25 | Time builtins → @extern FFI | -10 declares | **DONE** |
| 26 | Dead C files removed from build | 0 declares | **DONE** |
| 27 | Float NaN/Inf → LLVM IR | -16 declares, -176 lines C | **DONE** |
| 48 | Dead time/header cleanup | -8 .h decls, -9 .c funcs, -5 TML bindings | **DONE** |
| 49 | Ghost string/assert removal | -17 .h decls, -1 .c func, -3 MIR declares | **DONE** |
| 50 | Str to_string/debug_string → TML dispatch | -15 lines C++ codegen | **DONE** |
| 28 | On-demand emit | remaining | TODO |
| 29 | Type system cleanup | — | **DONE** |
| 30 | Benchmarks and validation | — | TODO |
