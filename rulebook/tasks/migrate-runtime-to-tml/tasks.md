# Tasks: Migrate C Runtime Pure Algorithms to TML

**Status**: In Progress (Phases 0-6 complete — List, HashMap, Buffer migrated; str.tml 99.3% pure TML, only `as_bytes` blocked on slice codegen)

**Scope**: ~4,585 lines of C runtime to migrate + ~3,300 lines of hardcoded codegen dispatch to eliminate

**Current metrics** (2026-02-18): 76.2% coverage (3,228/4,235), 9,025 tests across 784 files

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

## Phase 7: Formatting — Integer to String Pure TML

> **Source**: `lib/core/src/fmt/impls.tml`
> **C backing**: `compiler/runtime/text/text.c` (1,057 lines)
> **26 lowlevel blocks to eliminate**

Calls to remove from `fmt/impls.tml`:
- `i8_to_string(this)` → lines 20, 111
- `i16_to_string(this)` → lines 26, 117
- `i32_to_string(this)` → lines 32, 123
- `i64_to_string(this)` → lines 38, 129
- `u8_to_string(this)` → lines 44, 135
- `u16_to_string(this)` → lines 50, 141
- `u32_to_string(this)` → lines 56, 147
- `u64_to_string(this)` → lines 62, 153
- `f32_to_string(this)` → lines 68, 159
- `f64_to_string(this)` → lines 74, 165
- `char_to_string(this)` → lines 95, 188
- `f32_to_exp_string(this, uppercase)` → lines 480, 492
- `f64_to_exp_string(this, uppercase)` → lines 486, 498

Also from `fmt/helpers.tml`:
- `str_len(s)` → line 180
- `char_to_string(c)` → lines 185, 290
- `str_slice(s, start, end)` → line 204
- `i64_to_string(abs_value)` → line 248

Also from `fmt/formatter.tml`:
- `str_len(s)` → line 396
- `char_to_string(c)` → line 401
- `str_slice(s, start, end)` → line 420

Also from `fmt/traits.tml`:
- `char_to_string(c)` → line 182

Tasks:
- [ ] 7.1.1 Implement `i64_to_string()` — digit extraction loop: `% 10` + `/ 10`, build reversed, then reverse
- [ ] 7.1.2 Implement `i8/i16/i32_to_string()` — cast to I64, delegate to i64_to_string
- [ ] 7.1.3 Implement `u8/u16/u32/u64_to_string()` — unsigned variant of digit extraction
- [ ] 7.1.4 Update all 26 `impl Display/Debug` blocks in `fmt/impls.tml`
- [ ] 7.1.5 Update `fmt/helpers.tml` — replace str_len/char_to_string/i64_to_string calls
- [ ] 7.1.6 Update `fmt/formatter.tml` — replace str_len/char_to_string/str_slice calls
- [ ] 7.1.7 Update `fmt/traits.tml:182` — replace char_to_string
- [ ] 7.1.8 Run all existing integer display tests — must pass

---

## Phase 8: Formatting — Float to String Pure TML

> **Source**: `lib/core/src/fmt/float.tml`
> **C backing**: `compiler/runtime/text/text.c` + `compiler/runtime/math/math.c`
> **11 lowlevel blocks to eliminate**

Calls to remove from `fmt/float.tml`:
- `f32_to_string(value)` → line 98
- `f32_to_string_precision(value, precision)` → line 103
- `f32_to_exp_string(value, uppercase)` → line 108
- `f32_is_nan(value)` → line 113 (**KEEP** — hardware op)
- `f32_is_infinite(value)` → line 118 (**KEEP** — hardware op)
- `f64_to_string(value)` → line 132
- `f64_to_string_precision(value, precision)` → line 137
- `f64_to_exp_string(value, uppercase)` → line 142
- `f64_is_nan(value)` → line 147 (**KEEP** — hardware op)
- `f64_is_infinite(value)` → line 152 (**KEEP** — hardware op)
- `f64_round(value)` → line 334 (**KEEP** — hardware op)

Tasks:
- [ ] 8.1.1 Implement `f64_to_string()` — integer part via digit extraction, decimal part via multiply-and-extract
- [ ] 8.1.2 Implement `f64_to_string_precision()` — same with fixed decimal digits
- [ ] 8.1.3 Implement `f64_to_exp_string()` — normalize to 1.xxx * 10^e format
- [ ] 8.1.4 Implement `f32_to_string()` / `f32_to_string_precision()` / `f32_to_exp_string()` — cast to F64, delegate
- [ ] 8.1.5 Keep `f32_is_nan`, `f32_is_infinite`, `f64_is_nan`, `f64_is_infinite`, `f64_round` as lowlevel (hardware)
- [ ] 8.1.6 Run all existing float display tests — must pass

---

## Phase 9: Formatting — Char/UTF-8 to String Pure TML

> **Sources**: `lib/core/src/char/decode.tml`, `char/methods.tml`, `ascii/char.tml`
> **C backing**: `compiler/runtime/core/essential.c` (char_to_string, utf8_*byte_to_string)
> **~14 lowlevel blocks to eliminate**

Calls to remove from `char/methods.tml`:
- `char_to_string(byte)` → line 637
- `utf8_2byte_to_string(b1, b2)` → line 645
- `utf8_3byte_to_string(b1, b2, b3)` → line 653
- `utf8_4byte_to_string(b1, b2, b3, b4)` → line 661

Calls to remove from `char/decode.tml`:
- `char_to_string(byte)` → line 204
- `utf8_2byte_to_string(b1, b2)` → line 210
- `utf8_3byte_to_string(b1, b2, b3)` → line 217
- `utf8_4byte_to_string(b1, b2, b3, b4)` → line 224

Calls to remove from `ascii/char.tml`:
- `char_to_string(byte)` → lines 431, 659

Tasks:
- [ ] 9.1.1 Implement `char_to_string()` — alloc 2 bytes, write byte + null terminator
- [ ] 9.1.2 Implement `utf8_2byte_to_string()` — alloc 3 bytes, write 2 UTF-8 bytes + null
- [ ] 9.1.3 Implement `utf8_3byte_to_string()` — alloc 4 bytes, write 3 UTF-8 bytes + null
- [ ] 9.1.4 Implement `utf8_4byte_to_string()` — alloc 5 bytes, write 4 UTF-8 bytes + null
- [ ] 9.1.5 Update `char/methods.tml` (4 calls)
- [ ] 9.1.6 Update `char/decode.tml` (4 calls)
- [ ] 9.1.7 Update `ascii/char.tml` (2 calls)
- [ ] 9.1.8 Run all existing char/UTF-8 tests — must pass

---

## Phase 10: Formatting — Math Number Formats Pure TML

> **Source**: `compiler/runtime/math/math.c` (411 lines)
> **Hex/binary/octal/bits formatting**

C functions to replace:
- `i64_to_binary_str()`
- `i64_to_octal_str()`
- `i64_to_lower_hex_str()` / `i64_to_upper_hex_str()`
- `float_to_fixed()`
- `float_bits()` / `float_from_bits()`

Tasks:
- [ ] 10.1.1 Implement `i64_to_binary_str()` — extract bits via `% 2` / `/ 2` or bitshift
- [ ] 10.1.2 Implement `i64_to_octal_str()` — extract via `% 8` / `/ 8`
- [ ] 10.1.3 Implement `i64_to_lower_hex_str()` / `i64_to_upper_hex_str()` — `% 16` with hex digit table
- [ ] 10.1.4 Implement `float_to_fixed()` — multiply by 10^precision, round, format as integer.decimal
- [ ] 10.1.5 Implement `float_bits()` / `float_from_bits()` — use `transmute` intrinsic
- [ ] 10.1.6 Remove corresponding emitters from `builtins/math.cpp`
- [ ] 10.1.7 Run all existing math format tests — must pass

---

## Phase 11: Text Utilities (std::text) Pure TML

> **Source**: `lib/std/src/text.tml`
> **C backing**: `compiler/runtime/text/text.c` (1,057 lines)
> **48 lowlevel blocks to eliminate**

Calls to remove (all `text_*` C functions):
- `text_new()` → line 56
- `text_from_str(s)` → line 62
- `text_with_capacity(cap)` → line 68
- `text_from_i64(value)` → line 74
- `text_from_f64(value, precision)` → lines 80, 86
- `text_from_bool(value)` → line 92
- `text_as_cstr(handle)` → line 102
- `text_clone(handle)` → line 107
- `text_drop(handle)` → line 113
- `text_len(handle)` → line 123
- `text_capacity(handle)` → line 128
- `text_is_empty(handle)` → line 133
- `text_byte_at(handle, index)` → line 139
- `text_clear(handle)` → line 148
- `text_push(handle, byte)` → line 153
- `text_data_ptr(handle)` → line 159
- `text_set_len(handle, new_len)` → line 165
- `text_push_str_len(handle, s, len)` → line 171
- `text_push_i64(handle, value)` → line 176
- `text_push_formatted(handle, ...)` → line 183
- `text_reserve(handle, additional)` → line 188
- `text_fill_char(handle, byte, count)` → line 193
- `text_push_log(handle, ...)` → line 202
- `text_push_path(handle, ...)` → line 211
- `text_index_of(handle, search)` → line 220
- `text_last_index_of(handle, search)` → line 225
- `text_starts_with(handle, prefix)` → line 230
- `text_ends_with(handle, suffix)` → line 236
- `text_contains(handle, search)` → line 242
- `text_to_upper(handle)` → line 257
- `text_to_lower(handle)` → line 263
- `text_trim(handle)` → line 269
- `text_trim_start(handle)` → line 275
- `text_trim_end(handle)` → line 281
- `text_substring(handle, start, end)` → line 287
- `text_repeat(handle, count)` → line 298
- `text_replace(handle, search, replacement)` → line 304
- `text_replace_all(handle, search, replacement)` → line 310
- `text_reverse(handle)` → line 316
- `text_pad_start(handle, target_len, pad_char)` → line 322
- `text_pad_end(handle, target_len, pad_char)` → line 328
- `text_concat(handle, other_handle)` → line 338
- `text_concat_str(handle, s)` → line 344
- `text_compare(handle, other_handle)` → line 354
- `text_equals(handle, other_handle)` → line 359
- `text_print(handle)` → line 369 (**KEEP** — I/O)
- `text_println(handle)` → line 374 (**KEEP** — I/O)

Tasks:
- [ ] 11.1.1 Rewrite `Text` struct as `data: *U8, len: I64, capacity: I64` (replace opaque handle)
- [ ] 11.1.2 Implement constructors: `new()`, `from_str()`, `with_capacity()`, `from_i64()`, `from_f64()`, `from_bool()`
- [ ] 11.1.3 Implement `as_str()` / `clone()` / `drop()`
- [ ] 11.1.4 Implement field accessors: `len()`, `capacity()`, `is_empty()`, `byte_at()`
- [ ] 11.1.5 Implement mutation: `clear()`, `push()`, `push_str()`, `reserve()`, `fill_char()`
- [ ] 11.1.6 Implement search: `index_of()`, `last_index_of()`, `starts_with()`, `ends_with()`, `contains()`
- [ ] 11.1.7 Implement transforms: `to_upper()`, `to_lower()`, `trim()`, `trim_start()`, `trim_end()`
- [ ] 11.1.8 Implement builders: `substring()`, `repeat()`, `replace()`, `replace_all()`, `reverse()`, `pad_start()`, `pad_end()`
- [ ] 11.1.9 Implement concat: `concat()`, `concat_str()`
- [ ] 11.1.10 Implement comparison: `compare()`, `equals()`
- [ ] 11.1.11 Keep `print()` / `println()` as lowlevel (I/O needs C)
- [ ] 11.1.12 Run all existing text tests — must pass

---

## Phase 12: Search Algorithms Pure TML

> **Sources**: `lib/std/src/search/bm25.tml`, `hnsw.tml`, `distance.tml`
> **C backing**: `compiler/runtime/search/search.c` (98 lines)
> **40 lowlevel blocks to eliminate**

### BM25 (14 calls to remove from `bm25.tml`):
- `ffi_bm25_create()` → line 123
- `ffi_bm25_set_k1/b/name_boost/signature_boost/doc_boost/path_boost()` → lines 129-154
- `ffi_bm25_add_document(...)` → line 169
- `ffi_bm25_add_text(...)` → line 180
- `ffi_bm25_build()` → line 188
- `ffi_bm25_search(...)` → line 201
- `ffi_bm25_size()` → line 215
- `ffi_bm25_idf()` → line 222
- `ffi_bm25_destroy()` → line 233

### HNSW + TfIdf (15 calls to remove from `hnsw.tml`):
- `ffi_hnsw_create/set_params/insert/search/size/dims/max_layer/destroy()` → lines 141-204
- `ffi_tfidf_create/add_document/build/vectorize/dims/is_built/destroy()` → lines 236-286

### Distance metrics (11 calls to remove from `distance.tml`):
- `ffi_dot_product/cosine_similarity/euclidean_distance/norm/normalize()` (F64) → lines 79-122
- `ffi_dot_product_f32/cosine_similarity_f32/euclidean_distance_f32/l2_squared_f32/norm_f32/normalize_f32()` → lines 139-196

Tasks:
- [ ] 12.1.1 Implement BM25 index as TML struct with `List[Document]`, term frequency maps, IDF cache
- [ ] 12.1.2 Implement BM25 `add_document()`, `build()`, `search()` in pure TML
- [ ] 12.1.3 Implement HNSW index with multi-layer graph structure in TML
- [ ] 12.1.4 Implement TF-IDF vectorizer in TML
- [ ] 12.1.5 Implement distance functions: `dot_product`, `cosine_similarity`, `euclidean_distance` — loop with multiply-add (F64 and F32 variants)
- [ ] 12.1.6 Implement `norm()`, `normalize()` — sqrt of dot product with self
- [ ] 12.1.7 Run all existing search tests — must pass

---

## Phase 13: JSON Helper Calls

> **Source**: `lib/std/src/json/types.tml`
> **3 lowlevel blocks to eliminate** (uses str_* helpers)

Calls to remove:
- `str_char_at(s, index)` → line 120
- `str_substring(s, start, length)` → line 124
- `str_len(s)` → line 128

Tasks:
- [ ] 13.1.1 Replace `str_char_at` / `str_substring` / `str_len` calls with pure TML string ops (from Phase 4-5)
- [ ] 13.1.2 Run all existing JSON tests — must pass

---

## Phase 14: Logging — Partial Migration

> **Source**: `lib/std/src/log.tml`
> **18 lowlevel blocks — PARTIAL migration**
> **Note**: I/O and file operations STAY in C, but formatting can move

Calls that STAY (need C runtime for I/O and global state):
- `rt_log_msg(level, module, message)` → lines 110, 124, 138, 151, 165, 179, 193 (**7 calls** — write to stdout/file)
- `rt_log_set_level/get_level/enabled()` → lines 215, 226, 245 (**3 calls** — global atomic state)
- `rt_log_set_filter/module_enabled()` → lines 271, 287 (**2 calls** — filter matching)
- `rt_log_structured()` → line 317 (**1 call** — JSON output)
- `rt_log_set_format/get_format()` → lines 338, 345 (**2 calls** — format config)
- `rt_log_open_file/close_file()` → lines 369, 378 (**2 calls** — file I/O)
- `rt_log_init_from_env()` → line 403 (**1 call** — env var read)

Tasks:
- [ ] 14.1.1 Audit: confirm all 18 log calls require C (I/O, global state, file ops)
- [ ] 14.1.2 If any are pure formatting, migrate those
- [ ] 14.1.3 Document: log module stays mostly C-backed (legitimate I/O dependency)

---

## Phase 15: Minor Modules

- [ ] 15.1.1 Migrate `compiler/runtime/memory/pool.c` (344 lines) logic to `lib/core/src/pool.tml`
- [ ] 15.1.2 Migrate `compiler/runtime/core/profile_runtime.c` (54 lines) to `lib/test/src/bench/`
- [ ] 15.1.3 Remove `compiler/runtime/core/io.c` (67 lines — duplicate of essential.c)
- [ ] 15.1.4 Run full test suite — all tests must pass

---

## Phase 16: Codegen Cleanup — Remove Dead Emitters

- [ ] 16.1.1 Remove dead string emitters from `compiler/src/codegen/llvm/builtins/string.cpp`
- [ ] 16.1.2 Remove dead collection emitters from `compiler/src/codegen/llvm/builtins/collections.cpp`
- [ ] 16.1.3 Remove dead math formatting emitters from `compiler/src/codegen/llvm/builtins/math.cpp`
- [ ] 16.1.4 Remove migrated `declare` statements from `compiler/src/codegen/llvm/core/runtime.cpp`
- [ ] 16.1.5 Verify no LLVM IR references to removed C functions (`emit-ir` check)
- [ ] 16.1.6 Remove migrated C files: `text/string.c`, `text/text.c`, `collections/collections.c`, `math/math.c`, `search/search.c`, `memory/pool.c`, `core/io.c`, `core/profile_runtime.c`
- [ ] 16.1.7 Remove `lib/std/runtime/collections.c` and `collections.h`
- [ ] 16.1.8 Update CMakeLists.txt to remove deleted files
- [ ] 16.1.9 Rebuild compiler and run full test suite

---

## Phase 17: Runtime Declaration Optimization (On-Demand Emit)

> **Source**: `compiler/src/codegen/llvm/core/runtime.cpp`
> **Problem**: 393 `declare` statements emitted unconditionally in EVERY IR file
> **Impact**: -2KB per IR file, faster LLVM optimization and linking

- [ ] 17.1.1 Add `declared_runtime_functions_` set to `LLVMIRGen` to track what's been declared
- [ ] 17.1.2 Create `ensure_runtime_decl(name, signature)` helper that emits declare only on first use
- [ ] 17.1.3 Replace unconditional block in `emit_runtime_declarations()` with on-demand calls
- [ ] 17.1.4 Update all call sites in builtins/*.cpp to call `ensure_runtime_decl()` before emitting calls
- [ ] 17.1.5 Keep LLVM intrinsics (`llvm.memcpy`, `llvm.memset`, etc.) as unconditional (LLVM requires them)
- [ ] 17.1.6 Keep essential declarations unconditional: `print`, `panic`, `mem_alloc`, `mem_free` (always needed)
- [ ] 17.1.7 Verify: `--emit-ir` output only contains declarations for functions actually called
- [ ] 17.1.8 Run full test suite — all tests pass

---

## Phase 18: Codegen Dispatch Cleanup — Eliminate Hardcoded Collection Dispatch

> **Problem**: 1,330 lines of if/else in `method_collection.cpp` + 430 lines in `builtins/collections.cpp`
> **Goal**: As each collection type migrates to TML, remove its hardcoded dispatch path

### 18.1 HashMap dispatch removal (COMPLETED — done as part of Phase 2)

- [x] 18.1.1 Remove HashMap branch from `method_collection.cpp` (~200 lines of `get/set/has/remove/len/clear/iter`)
- [x] 18.1.2 Remove HashMapIter branch from `method_collection.cpp` (~107 lines of `has_next/key/value/next`)
- [x] 18.1.3 Remove HashMap from bypass list in `method_impl.cpp:96` (allow normal impl dispatch)
- [x] 18.1.4 Remove HashMap from skip list in `decl/impl.cpp:155` (generate impl methods normally)
- [x] 18.1.5 Remove HashMap from skip list in `generate.cpp:715`
- [x] 18.1.6 Remove HashMap static methods from `method_static.cpp:45-104` (HashMap::new, ::default)
- [x] 18.1.7 Remove HashMap from type name validation in `method_static_dispatch.cpp:898`
- [x] 18.1.8 Remove 14 hashmap_* function registrations from `types/builtins/collections.cpp`
- [x] 18.1.9 Remove hashmap_* if/else chain from `builtins/collections.cpp`
- [x] 18.1.10 Remove hardcoded `{ ptr }` type-erasure for HashMap in `decl/struct.cpp:337-354`
- [x] 18.1.11 All HashMap tests pass through normal dispatch (11 tests)

### 18.2 Buffer dispatch removal (COMPLETED — done as part of Phase 3)

- [x] 18.2.1 Remove Buffer branch from `method_collection.cpp` (~973 lines → nullopt stub)
- [x] 18.2.2 Remove Buffer from bypass list in `method_impl.cpp:96`
- [x] 18.2.3 Remove Buffer from skip list in `decl/impl.cpp:155`
- [x] 18.2.4 Remove Buffer from skip list in `generate.cpp:715`
- [x] 18.2.5 Remove Buffer static methods from `method_static.cpp` (new, default, from_hex, from_string)
- [x] 18.2.6 Remove Buffer from type name validation in `method_static_dispatch.cpp`
- [x] 18.2.7 Remove 12 buffer_* function registrations from `types/builtins/collections.cpp`
- [x] 18.2.8 Remove buffer_* if/else chain from `builtins/collections.cpp` (→ nullopt stub)
- [x] 18.2.9 Remove Buffer struct type + 15 buffer_* runtime declarations from `runtime.cpp`
- [x] 18.2.10 All 8 Buffer test files pass through normal dispatch (31+ tests)

### 18.3 List cleanup (Phase 1 already done, just cleanup)

- [ ] 18.3.1 Remove `List` from type-erasure in `decl/struct.cpp:317-335` (allow TML-defined layout)
- [ ] 18.3.2 Remove `List` from type name validation in `method_static_dispatch.cpp:898`
- [ ] 18.3.3 Verify List dispatch goes through normal `gen_impl_method()` path

---

## Phase 19: File/Path Refactor — Move to TML Structs with @extern("c") FFI

> **Problem**: File and Path have 23 hardcoded static methods in `method_static.cpp` (lines 186-385)
> **Problem**: Skip impl dispatch in `method_impl.cpp`, `decl/impl.cpp`, `generate.cpp`
> **Goal**: TML structs with `@extern("c")` bindings to OS functions

### 19.1 File refactor

- [ ] 19.1.1 Create `lib/std/src/file.tml` with `pub type File { handle: *Unit }`
- [ ] 19.1.2 Add `@extern("c")` declarations for OS file operations (open, read, write, close, etc.)
- [ ] 19.1.3 Implement File static methods as TML impl calling @extern functions
- [ ] 19.1.4 Remove File from bypass list in `method_impl.cpp:96`
- [ ] 19.1.5 Remove File from skip list in `decl/impl.cpp:155`
- [ ] 19.1.6 Remove File from skip list in `generate.cpp:715`
- [ ] 19.1.7 Remove 12 hardcoded File methods from `method_static.cpp:186-263`
- [ ] 19.1.8 Run all File tests — must pass

### 19.2 Path refactor

- [ ] 19.2.1 Create `lib/std/src/path.tml` with `pub type Path { inner: Str }`
- [ ] 19.2.2 Add `@extern("c")` declarations for OS path operations
- [ ] 19.2.3 Implement Path static methods as TML impl calling @extern functions
- [ ] 19.2.4 Remove Path from bypass list in `method_impl.cpp:96`
- [ ] 19.2.5 Remove Path from skip list in `decl/impl.cpp:155`
- [ ] 19.2.6 Remove Path from skip list in `generate.cpp:715`
- [ ] 19.2.7 Remove 11 hardcoded Path methods from `method_static.cpp:266-385`
- [ ] 19.2.8 Run all Path tests — must pass

---

## Phase 20: Type System Cleanup — Remove Hardcoded Builtin Registrations

> **Problem**: `types/builtins/collections.cpp` registers 25 function signatures manually
> **Problem**: `types/builtins/string.cpp` registers 29 function signatures manually
> **Goal**: Type system discovers methods from TML source files, not hardcoded registrations

- [x] 20.1.1 After Phase 2: Remove 14 hashmap_* registrations from `types/builtins/collections.cpp` (done in Phase 2)
- [x] 20.1.2 After Phase 3: Remove 11 buffer_* registrations from `types/builtins/collections.cpp` (done in Phase 3)
- [ ] 20.1.3 After Phases 4-6: Remove 29 string registrations from `types/builtins/string.cpp`
- [ ] 20.1.4 After Phase 11: Remove 9 strbuilder_* registrations (if Text migrates)
- [ ] 20.1.5 Verify: type checker finds method signatures from TML impl blocks, not builtin registry
- [ ] 20.1.6 Run full test suite

---

## Phase 21: Fix Metadata Loss and Workarounds

> **Problem**: Behavior method return types lost in metadata loader → hardcoded workaround
> **Source**: `method_prim_behavior.cpp:378-390`

- [ ] 21.1.1 Fix metadata loader to preserve return types for behavior impls on primitives
- [ ] 21.1.2 Remove hardcoded return type workaround (eq→i1, cmp→Ordering, hash→i64)
- [ ] 21.1.3 Fix unresolved generic type placeholders in `decl/struct.cpp:160-167`
- [ ] 21.1.4 Run full test suite

---

## Phase 22: Benchmarks and Validation

- [ ] 22.1.1 Benchmark List push/pop/get: 1M operations (before vs after)
- [ ] 22.1.2 Benchmark HashMap insert/get/remove: 100K operations (before vs after)
- [ ] 22.1.3 Benchmark string concat/split/contains: 100K operations (before vs after)
- [ ] 22.1.4 Benchmark i64_to_string / f64_to_string: 1M operations (before vs after)
- [ ] 22.1.5 Benchmark Text builder: build 10K string (before vs after)
- [ ] 22.1.6 Benchmark search: BM25 index + query (before vs after)
- [ ] 22.1.7 Verify performance within 10% of C implementations
- [ ] 22.1.8 Run full test suite with --coverage
- [ ] 22.1.9 Document final metrics: C files removed, lowlevel blocks eliminated, codegen dispatch lines removed

---

## Summary: Expected Impact

| Metric | Before | Current | After All Phases |
|--------|--------|---------|-----------------|
| C runtime lines (migratable) | ~5,210 | ~700 (fmt, text, search remain) | 0 |
| C runtime lines (total) | ~20,000 | ~15,500 | ~12,500 (FFI + essential only) |
| Hardcoded dispatch lines | ~3,300 | ~500 | ~300 (intrinsics only) |
| Types bypassing impl dispatch | 5 (HashMap, Buffer, File, Path, Ordering) | 2 (File, Path) | 0 |
| Unconditional runtime declares | 393 | ~300 | ~30 (on-demand) |
| Hardcoded type registrations | 54 | ~29 (string) | 0 |
| Hardcoded collection methods | 1,330 lines | 0 ✓ | 0 |
| Hardcoded static methods | 500 lines | ~350 (File, Path) | ~150 (primitives only) |
