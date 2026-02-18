# Tasks: Migrate C Runtime Pure Algorithms to TML

**Status**: In Progress (Phases 0-7 + 19 complete — List, HashMap, Buffer, File/Path/Dir migrated; str.tml 99.3% pure TML; fmt integer-to-string migrated; 0 types bypassing impl dispatch)

**Scope**: ~4,585 lines of C runtime to migrate + ~3,300 lines of hardcoded codegen dispatch to eliminate

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

## Phase 19: File/Path/Dir Refactor — Move to TML Structs with @extern("c") FFI (DONE)

> **Status**: COMPLETE — File, Path, and Dir all use TML structs with @extern("c") FFI
> **Impact**: 0 types bypassing impl dispatch, ~150 lines hardcoded dispatch removed, ~110 lines dead runtime declarations removed

### 19.1 File refactor (DONE)

- [x] 19.1.1 Rewrite `lib/std/src/file.tml` with TML struct + `@extern("c")` FFI to OS file ops
- [x] 19.1.2 Add `@extern("c")` declarations for 12 OS file operations (open, read, write, close, etc.)
- [x] 19.1.3 Implement File static methods as TML impl calling @extern functions
- [x] 19.1.4 Remove File from bypass list in `method_impl.cpp`
- [x] 19.1.5 Remove File from skip list in `decl/impl.cpp`
- [x] 19.1.6 Remove File from skip list in `decl/struct.cpp`
- [x] 19.1.7 Remove File from skip list in `generate.cpp`
- [x] 19.1.8 Remove File from type check in `method_static_dispatch.cpp`
- [x] 19.1.9 Remove 12 hardcoded File methods from `method_static.cpp`
- [x] 19.1.10 Remove File struct type declarations from `runtime.cpp`
- [x] 19.1.11 All File tests pass

### 19.2 Path refactor (DONE)

- [x] 19.2.1 Rewrite `lib/std/src/path.tml` with TML struct + `@extern("c")` FFI to OS path ops
- [x] 19.2.2 Add `@extern("c")` declarations for 11 OS path operations
- [x] 19.2.3 Implement Path static methods as TML impl calling @extern functions
- [x] 19.2.4 Remove Path from bypass list in `method_impl.cpp`
- [x] 19.2.5 Remove Path from skip list in `decl/impl.cpp`
- [x] 19.2.6 Remove Path from skip list in `decl/struct.cpp`
- [x] 19.2.7 Remove Path from skip list in `generate.cpp`
- [x] 19.2.8 Remove Path from type check in `method_static_dispatch.cpp`
- [x] 19.2.9 Remove 11 hardcoded Path methods from `method_static.cpp`
- [x] 19.2.10 Remove Path struct type declarations from `runtime.cpp`
- [x] 19.2.11 All Path tests pass

### 19.3 Dir refactor (DONE)

- [x] 19.3.1 Migrate `lib/std/src/file/dir.tml` from lowlevel blocks to @extern("c") FFI
- [x] 19.3.2 All Dir tests pass

### 19.4 Runtime cleanup (DONE)

- [x] 19.4.1 Remove dead network socket `_raw` declarations from `runtime.cpp` (~43 lines — lowlevel uses tml_ prefix wrappers)
- [x] 19.4.2 Remove dead TLS/SSL declarations from `runtime.cpp` (~68 lines — @extern in tls.tml handles everything)
- [x] 19.4.3 Remove stale "Note: removed" comments from `runtime.cpp`
- [x] 19.4.4 Full test suite passes: 9,025 tests across 787 files, 0 failures

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

## Summary: Impact and Status

| Metric | Before | Current (2026-02-18) | Notes |
|--------|--------|---------------------|-------|
| C runtime lines (migratable) | ~5,210 | ~700 (text, search, float) | Phase 7 eliminated 16 fmt lowlevel blocks |
| C runtime lines (total) | ~20,000 | ~15,500 | |
| Hardcoded dispatch lines | ~3,300 | ~350 | |
| Types bypassing impl dispatch | 5 | 0 ✓ | |
| Unconditional runtime declares | 393 | ~190 | Phase 17 (on-demand) deferred |
| Hardcoded type registrations | 54 | ~29 (string) | |
| Hardcoded collection methods | 1,330 lines | 0 ✓ | |
| Hardcoded static methods | 500 lines | ~150 (primitives only) ✓ | |

### Phase 7-15 Audit Summary

| Phase | Module | Lowlevel Blocks | Decision | Reason |
|-------|--------|----------------|----------|--------|
| 7 | fmt integers | 16 | **MIGRATED** | Pure TML string concat with digit lookup |
| 8 | fmt floats | 11 | KEEP | Hardware-dependent (snprintf, FPU) |
| 9 | char/UTF-8 | ~14 | KEEP | Registered functions_[] map entries |
| 10 | hex/octal/binary | 0 | Already pure TML | No lowlevel blocks |
| 11 | Text builder | 48 | BLOCKED | Type checker doesn't support memory intrinsics |
| 12 | Search (BM25/HNSW) | 40 | KEEP (Tier 2 FFI) | Complex algorithms, @extern pattern correct |
| 13 | JSON helpers | 3 | KEEP | Registered functions_[] map entries |
| 14 | Logging | 18 | KEEP | All I/O / global state |
| 15 | Minor modules | varies | DEFERRED | pool.c, profile_runtime.c, io.c |

### Remaining Lowlevel Block Census (818 total across lib/)

| Category | Count | Examples | Migratable? |
|----------|-------|---------|-------------|
| Memory intrinsics | ~280 | mem_alloc, ptr_read[T], ptr_write[T] | NO — fundamental |
| Array intrinsics | ~80 | array_get[T], array_set[T], array_uninit[T] | NO — compiler |
| String functions | ~80 | str_len, str_slice, char_to_string | NO — registered |
| Text functions | ~48 | text_new, text_push, text_trim | BLOCKED — type checker |
| Atomic operations | ~78 | compare_exchange, fetch_add | NO — hardware |
| Search FFI | ~40 | ffi_bm25_*, ffi_hnsw_*, ffi_dot_* | NO — Tier 2 FFI |
| Float functions | ~30 | f64_to_string, is_nan, round | NO — hardware |
| Logging | ~18 | rt_log_* | NO — I/O |
| Net/TLS/Sync | ~50 | socket_*, tls_*, mpsc_* | NO — OS |
| Other registered | ~114 | glob_*, json_*, utf8_* | NO — registered |

**Key blocker for further migration**: The TML type checker does not infer correct return types for memory intrinsics (`mem_alloc` → should be `ptr`, `ptr_read[T]` → should be `T`). Until this is fixed, any new code using manual memory management in lowlevel blocks will produce LLVM IR type mismatches. This blocks Phase 11 (Text) and any future growable buffer implementations.
