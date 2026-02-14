# Tasks: Migrate C Runtime Pure Algorithms to TML

**Status**: Proposed (0%)

**Scope**: ~4,585 lines of C removed, ~240 lowlevel blocks eliminated across ~25 TML files

---

## Phase 0: Validation — Verify Compiler Primitives Work

- [ ] 0.1.1 Write test: `ptr_read`, `ptr_write`, `ptr_offset` with I32, I64, Str
- [ ] 0.1.2 Write test: `mem_alloc` + `ptr_write` + `ptr_read` round-trip
- [ ] 0.1.3 Write test: `copy_nonoverlapping` for array-like buffers
- [ ] 0.1.4 Write test: `size_of[T]()` for primitives and structs
- [ ] 0.1.5 Write test: `@lowlevel` block with pointer arithmetic in generic context
- [ ] 0.1.6 Write test: generic struct with `*T` field (monomorphization)
- [ ] 0.1.7 Confirm all Phase 0 tests pass — if any fail, fix compiler before proceeding

---

## Phase 1: Collections — List[T] Pure TML

> **Source**: `lib/std/src/collections/list.tml`
> **C backing**: `compiler/runtime/collections/collections.c` + `lib/std/runtime/collections.c`
> **10 lowlevel blocks to eliminate**

Calls to remove:
- `list_create(initial_capacity)` → line 55
- `list_push(this.handle, value)` → line 80
- `list_pop(this.handle)` → line 93
- `list_get(this.handle, index)` → line 110
- `list_set(this.handle, index, value)` → line 124
- `list_len(this.handle)` → line 133
- `list_capacity(this.handle)` → line 142
- `list_is_empty(this.handle)` → line 151
- `list_clear(this.handle)` → line 182
- `list_destroy(this.handle)` → line 191

Tasks:
- [ ] 1.1.1 Rewrite `List[T]` struct: `data: *T, len: I64, capacity: I64` (replace opaque `handle: *Unit`)
- [ ] 1.1.2 Implement `new()` / `with_capacity()` using `mem_alloc`
- [ ] 1.1.3 Implement `push()` with grow via `mem_realloc` + `ptr_write`
- [ ] 1.1.4 Implement `pop()` returning `Maybe[T]` via `ptr_read`
- [ ] 1.1.5 Implement `get()` / `set()` via `ptr_offset` + `ptr_read`/`ptr_write`
- [ ] 1.1.6 Implement `len()`, `capacity()`, `is_empty()` as field reads
- [ ] 1.1.7 Implement `clear()` (set len=0) and `remove()` (shift via `copy`)
- [ ] 1.1.8 Implement `first()` / `last()` returning `Maybe[T]`
- [ ] 1.1.9 Implement `drop()` calling `mem_free`
- [ ] 1.1.10 Remove `list_*` emitters from `compiler/src/codegen/llvm/builtins/collections.cpp`
- [ ] 1.1.11 Remove `list_*` declarations from `compiler/src/codegen/llvm/core/runtime.cpp`
- [ ] 1.1.12 Run all existing List tests — must pass
- [ ] 1.1.13 Also fix `lib/core/src/collections.tml:267` (`list_get_mut` → use new List)

---

## Phase 2: Collections — HashMap[K, V] Pure TML

> **Source**: `lib/std/src/collections/hashmap.tml`
> **C backing**: `compiler/runtime/collections/collections.c` + `lib/std/runtime/collections.c`
> **14 lowlevel blocks to eliminate** (+ 10 iterator blocks)

Calls to remove:
- `hashmap_create(initial_capacity)` → line 56
- `hashmap_set(this.handle, key, value)` → line 84
- `hashmap_get(this.handle, key)` → line 103
- `hashmap_has(this.handle, key)` → line 116
- `hashmap_remove(this.handle, key)` → line 129
- `hashmap_len(this.handle)` → line 138
- `hashmap_clear(this.handle)` → line 143
- `hashmap_destroy(this.handle)` → line 152
- `hashmap_iter_create(this.handle)` → line 173
- `hashmap_iter_has_next(this.handle)` → line 206
- `hashmap_iter_next(this.handle)` → line 211
- `hashmap_iter_key(this.handle)` → line 220
- `hashmap_iter_value(this.handle)` → line 229
- `hashmap_iter_destroy(this.handle)` → line 238

Tasks:
- [ ] 2.1.1 Design `HashMap[K, V]` struct with open-addressing: `entries: *Entry[K,V], count: I64, capacity: I64`
- [ ] 2.1.2 Implement `Entry[K,V]` struct: `key: K, value: V, hash: U64, occupied: Bool`
- [ ] 2.1.3 Implement `new()` / `with_capacity()` using `mem_alloc_zeroed`
- [ ] 2.1.4 Implement `insert()` with linear probing or robin-hood, grow on load factor
- [ ] 2.1.5 Implement `get()` returning `Maybe[ref V]`
- [ ] 2.1.6 Implement `contains()` / `remove()`
- [ ] 2.1.7 Implement `len()`, `clear()`, `is_empty()`
- [ ] 2.1.8 Implement iterator as TML struct (no opaque handle)
- [ ] 2.1.9 Implement `drop()` calling `mem_free`
- [ ] 2.1.10 Require `K: Hash + Eq` behavior bounds
- [ ] 2.1.11 Remove `hashmap_*` emitters from `builtins/collections.cpp`
- [ ] 2.1.12 Remove `hashmap_*` declarations from `core/runtime.cpp`
- [ ] 2.1.13 Run all existing HashMap tests — must pass

---

## Phase 3: Collections — Buffer Pure TML

> **Source**: `lib/std/src/collections/buffer.tml`
> **C backing**: `compiler/runtime/collections/collections.c`
> **11 lowlevel blocks to eliminate**

Calls to remove:
- `buffer_create(initial_capacity)` → line 193
- `buffer_write_byte(this.handle, byte)` → line 212
- `buffer_write_i32(this.handle, value)` → line 221
- `buffer_read_byte(this.handle)` → line 239
- `buffer_read_i32(this.handle)` → line 248
- `buffer_len(this.handle)` → line 266
- `buffer_capacity(this.handle)` → line 275
- `buffer_remaining(this.handle)` → line 284
- `buffer_clear(this.handle)` → line 289
- `buffer_reset_read(this.handle)` → line 294
- `buffer_destroy(this.handle)` → line 303

Tasks:
- [ ] 3.1.1 Rewrite `Buffer` struct: `data: *U8, len: I64, capacity: I64, read_pos: I64`
- [ ] 3.1.2 Implement `new()` / `with_capacity()` using `mem_alloc`
- [ ] 3.1.3 Implement `write_byte()`, `write_i32()`, `write_i64()` via `ptr_write`
- [ ] 3.1.4 Implement `read_byte()`, `read_i32()` via `ptr_read` + read_pos advance
- [ ] 3.1.5 Implement `len()`, `capacity()`, `remaining()` as field arithmetic
- [ ] 3.1.6 Implement `clear()`, `reset_read()`, `drop()`
- [ ] 3.1.7 Remove `buffer_*` emitters from codegen
- [ ] 3.1.8 Run all existing Buffer tests — must pass

---

## Phase 4: Strings — Core Read-Only Operations Pure TML

> **Source**: `lib/core/src/str.tml`
> **C backing**: `compiler/runtime/text/string.c` (1,201 lines)
> **~20 lowlevel blocks to eliminate** (read-only string operations)

Calls to remove (standalone functions + impl Str methods):
- `str_len(s)` → lines 90, 695
- `str_char_at(s, idx)` → lines 123, 726
- `str_contains(s, pattern)` → lines 282, 765
- `str_starts_with(s, prefix)` → lines 294, 755
- `str_ends_with(s, suffix)` → lines 306, 760
- `str_find(s, pattern)` → lines 320, 771
- `str_rfind(s, pattern)` → lines 338, 777
- `str_as_bytes(this)` → line 720
- `str_hash(this)` → `lib/core/src/hash.tml:174`

Tasks:
- [ ] 4.1.1 Implement `str_len()` — iterate bytes via `ptr_read` until null terminator (or store length)
- [ ] 4.1.2 Implement `str_char_at()` — UTF-8 aware byte indexing via `ptr_offset` + `ptr_read`
- [ ] 4.1.3 Implement `str_contains()` — byte-by-byte search (naive or KMP)
- [ ] 4.1.4 Implement `str_starts_with()` / `str_ends_with()` — prefix/suffix byte compare
- [ ] 4.1.5 Implement `str_find()` / `str_rfind()` — return byte offset or -1
- [ ] 4.1.6 Implement `str_as_bytes()` — pointer cast to `ref [U8]`
- [ ] 4.1.7 Implement `str_hash()` in `lib/core/src/hash.tml` — FNV-1a or djb2 in pure TML
- [ ] 4.1.8 Update both standalone functions AND `impl Str` methods in `str.tml`
- [ ] 4.1.9 Remove corresponding emitters from `builtins/string.cpp`
- [ ] 4.1.10 Run all existing string tests — must pass

---

## Phase 5: Strings — Allocating Operations Pure TML

> **Source**: `lib/core/src/str.tml`
> **C backing**: `compiler/runtime/text/string.c`
> **~23 lowlevel blocks to eliminate** (operations that allocate new strings)

Calls to remove (standalone + impl Str):
- `str_substring(s, start, len)` → lines 178, 733, 740
- `str_trim(s)` / `str_trim_start(s)` / `str_trim_end(s)` → lines 216/227/238, 792/797/802
- `str_to_uppercase(s)` / `str_to_lowercase(s)` → lines 254/266, 745/750
- `str_split(s, delimiter)` → lines 365, 782
- `str_split_whitespace(s)` → line 382
- `str_lines(s)` → line 396
- `str_replace(s, pattern, replacement)` → lines 419, 819
- `str_replace_first(s, pattern, replacement)` → line 437
- `str_repeat(s, n)` → line 460
- `str_join(parts, separator)` → line 639
- `str_chars(this)` → line 787

Tasks:
- [ ] 5.1.1 Implement `str_substring()` — `mem_alloc` + `copy_nonoverlapping`
- [ ] 5.1.2 Implement `str_trim()` / `trim_start()` / `trim_end()` — find non-whitespace bounds, then substring
- [ ] 5.1.3 Implement `str_to_uppercase()` / `str_to_lowercase()` — byte-by-byte case flip ('a'-'z' ↔ 'A'-'Z')
- [ ] 5.1.4 Implement `str_split()` — scan for delimiter, collect into `List[Str]`
- [ ] 5.1.5 Implement `str_split_whitespace()` / `str_lines()` — split on whitespace/newlines
- [ ] 5.1.6 Implement `str_replace()` / `str_replace_first()` — find + rebuild string
- [ ] 5.1.7 Implement `str_repeat()` — alloc n*len + repeated copy
- [ ] 5.1.8 Implement `str_join()` — calculate total len, alloc, copy parts with separator
- [ ] 5.1.9 Implement `str_chars()` — UTF-8 decode into `List[I32]`
- [ ] 5.1.10 Update both standalone functions AND `impl Str` methods
- [ ] 5.1.11 Remove corresponding emitters from `builtins/string.cpp`
- [ ] 5.1.12 Run all existing string tests — must pass

---

## Phase 6: Strings — Parsing Pure TML

> **Source**: `lib/core/src/str.tml`
> **C backing**: `compiler/runtime/text/string.c` (str_parse_* functions)
> **~5 lowlevel blocks to eliminate**

Calls to remove:
- `str_parse_i32(s)` → line 539
- `str_parse_i64(s)` → lines 561, 807, 813
- `str_parse_f64(s)` → line 582

Tasks:
- [ ] 6.1.1 Implement `str_parse_i32()` — iterate digits, accumulate value, handle sign
- [ ] 6.1.2 Implement `str_parse_i64()` — same as i32 with I64 accumulator
- [ ] 6.1.3 Implement `str_parse_f64()` — integer part + decimal part + optional exponent
- [ ] 6.1.4 Update `impl Str` parse methods (lines 807, 813)
- [ ] 6.1.5 Remove corresponding emitters from `builtins/string.cpp`
- [ ] 6.1.6 Run all existing parse tests — must pass

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

## Phase 16: Codegen Cleanup

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

## Phase 17: Benchmarks and Validation

- [ ] 17.1.1 Benchmark List push/pop/get: 1M operations (before vs after)
- [ ] 17.1.2 Benchmark HashMap insert/get/remove: 100K operations (before vs after)
- [ ] 17.1.3 Benchmark string concat/split/contains: 100K operations (before vs after)
- [ ] 17.1.4 Benchmark i64_to_string / f64_to_string: 1M operations (before vs after)
- [ ] 17.1.5 Benchmark Text builder: build 10K string (before vs after)
- [ ] 17.1.6 Benchmark search: BM25 index + query (before vs after)
- [ ] 17.1.7 Verify performance within 10% of C implementations
- [ ] 17.1.8 Run full test suite with --coverage
- [ ] 17.1.9 Document final metrics: C files removed, lowlevel blocks eliminated, total line count reduction
