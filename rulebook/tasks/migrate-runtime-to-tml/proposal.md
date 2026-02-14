# Proposal: Migrate C Runtime Pure Algorithms to TML

## Status
- **Created**: 2026-02-13
- **Status**: Proposed
- **Priority**: High

## Why

The TML project has C code spread across **two locations**:

1. **`compiler/runtime/`** — 29 C files, ~19,125 lines (the original compiler runtime)
2. **`lib/std/runtime/`** — 16 C files (embedded C backing for std library modules)
3. **`lib/test/runtime/`** — 2 C files (test framework backing)

A significant portion implements **pure algorithms** (string manipulation, collections, math formatting, search, JSON, text utilities) that do not require OS syscalls, hardware access, or external libraries. These should be implemented in TML using existing `@intrinsic` memory primitives (`ptr_read`, `ptr_write`, `ptr_offset`, `mem_alloc`, `mem_free`, `copy_nonoverlapping`) which already exist and work.

### The core problem

The current architecture wraps C functions in TML via `lowlevel` blocks:

```tml
// Current: TML function that just calls C
func contains(self, pattern: Str) -> Bool {
    lowlevel { str_contains(self.data, pattern.data) }  // calls C runtime
}
```

This should be:

```tml
// Correct: Pure TML using intrinsics
func contains(self, pattern: Str) -> Bool {
    let plen = pattern.byte_len()
    loop i in 0 to (self.byte_len() - plen) {
        if self.slice(i, i + plen) == pattern { return true }
    }
    false
}
```

**904 `lowlevel` blocks across 69 TML files** currently call C functions. Many of these are pure algorithms that don't need C.

### Why this matters

1. **Type safety loss**: C uses `void*` and `int64_t` erasure. `List[T]` becomes an opaque handle.
2. **Blocks LLVM optimization**: Cannot inline/vectorize across FFI boundaries.
3. **Violates language identity**: Rust's Vec, HashMap, String are all Rust. TML's should be TML.
4. **Maintenance burden**: Changing collection behavior requires editing C, rebuilding runtime, coordinating codegen.

### Three-tier architecture (correct model)

```
@intrinsic          → LLVM instructions (add, memcpy, sqrt, atomics)
                      CANNOT be TML. Compiler emits LLVM IR directly.

@extern("c") / FFI  → External C libraries (zlib, OpenSSL, BCrypt, libc syscalls)
                      SHOULD NOT be reimplemented. Call the library.

TML pure            → Algorithms (strings, collections, formatting, search, JSON)
                      SHOULD be TML using mem_alloc + ptr_read/write/offset.
```

## Full Audit: All C Dependencies

### LOCATION 1: compiler/runtime/ (29 files, 19,125 lines)

#### MIGRATE to TML (pure algorithms)

| File | Lines | What it does | Target |
|------|------:|--------------|--------|
| `text/string.c` | 1,201 | str_concat, str_trim, str_split, str_contains, str_replace, str_find, str_repeat, str_parse_*, StringBuilder | `lib/core/src/str.tml` |
| `text/text.c` | 1,057 | Additional text utilities, formatting helpers | `lib/core/src/fmt/` |
| `collections/collections.c` | 1,353 | List (push/pop/get/set), HashMap (insert/get/remove), Buffer (read/write/slice) | `lib/std/src/collections/` |
| `math/math.c` | 411 | i64_to_hex, float_to_fixed, float formatting, SIMD sum/dot, black_box, float_bits | `lib/core/src/fmt/` + `lib/core/src/num/` |
| `search/search.c` | 98 | Search algorithms | `lib/std/src/search/` |
| `memory/pool.c` | 344 | Memory pool allocator | `lib/core/src/pool.tml` |
| `core/io.c` | 67 | Duplicate of print/println from essential.c | Remove (redundant) |
| `core/profile_runtime.c` | 54 | black_box benchmarking helpers | `lib/test/src/bench/` |

**Subtotal**: 4,585 lines → TML

#### KEEP in C (OS/hardware/FFI)

| File | Lines | Why it must stay |
|------|------:|------------------|
| `core/essential.c` | 987 | print, println, panic, setjmp/longjmp test harness, VEH handler |
| `memory/mem.c` | 210 | malloc/free/realloc wrappers (OS allocator) |
| `memory/mem_track.c` | 452 | Debug leak detection with tracking |
| `time/time.c` | 176 | OS timers (QueryPerformanceCounter, clock_gettime) |
| `os/os.c` | 771 | Platform detection, env vars, CPU count (OS syscalls) |
| `net/net.c` | 638 | BSD socket operations (OS syscalls) |
| `net/dns.c` | 316 | getaddrinfo DNS resolution (OS) |
| `net/tls.c` | 802 | TLS via Schannel/OpenSSL/SecTransport (external lib FFI) |
| `concurrency/sync.c` | 746 | Mutex/RwLock/CondVar via SRWLOCK/pthread (OS primitives) |
| `concurrency/thread.c` | 519 | CreateThread/pthread_create (OS) |
| `concurrency/async.c` | 952 | Async event loop (OS I/O completion) |
| `diagnostics/backtrace.c` | 834 | Stack unwinding (platform-specific SEH/DWARF) |
| `diagnostics/log.c` | 395 | Structured logging infrastructure |
| `crypto/crypto.c` | 1,556 | Crypto via BCrypt/OpenSSL (external lib FFI) |
| `crypto/crypto_dh.c` | 832 | Diffie-Hellman (external lib FFI) |
| `crypto/crypto_ecdh.c` | 883 | ECDH (external lib FFI) |
| `crypto/crypto_kdf.c` | 547 | Key derivation (external lib FFI) |
| `crypto/crypto_key.c` | 767 | Key management (external lib FFI) |
| `crypto/crypto_rsa.c` | 768 | RSA (external lib FFI) |
| `crypto/crypto_sign.c` | 512 | Digital signatures (external lib FFI) |
| `crypto/crypto_x509.c` | 877 | X.509 certificates (external lib FFI) |

**Subtotal**: 13,540 lines stay in C

---

### LOCATION 2: lib/std/runtime/ (16 files)

| File | Status | Reason |
|------|--------|--------|
| `collections.c` + `.h` | **MIGRATE** | Duplicate of compiler/runtime/collections.c — pure algorithms |
| `file.c` + `.h` | KEEP | File I/O syscalls (fopen, fread, stat, opendir) |
| `glob.c` + `.h` | KEEP | OS file pattern matching (glob(), FindFirstFile) |
| `crypto/crypto_common.c` | KEEP | Crypto buffer utilities for external libs |
| `crypto/crypto_random.c` | KEEP | OS CSPRNG (BCryptGenRandom, getrandom) |
| `crypto/crypto_hash_win.c` | KEEP | Windows CNG hashing (OS crypto API) |
| `crypto/crypto_hash_openssl.c` | KEEP | OpenSSL hashing (external lib FFI) |
| `crypto/crypto_hash_macos.c` | KEEP | macOS CommonCrypto (OS crypto API) |
| `crypto/crypto_cipher_win.c` | KEEP | Windows CNG ciphers (OS crypto API) |
| `crypto/crypto_kdf_win.c` | KEEP | Windows CNG KDF (OS crypto API) |
| `crypto/crypto_internal.h` | KEEP | Internal crypto declarations |
| `zlib/zlib_deflate.c` | KEEP | zlib FFI (external lib) |
| `zlib/zlib_brotli.c` | KEEP | Brotli FFI (external lib) |
| `zlib/zlib_zstd.c` | KEEP | Zstandard FFI (external lib) |
| `zlib/zlib_exports.c` | KEEP | Compression exports |

---

### LOCATION 3: lib/test/runtime/ (2 files)

| File | Status | Reason |
|------|--------|--------|
| `test.c` | KEEP | Test assertion runtime, panic catching |
| `coverage.c` | KEEP | Lock-free coverage tracking with atomics, performance-critical hot path |

---

### LOCATION 4: lib/ TML files with `lowlevel` blocks (69 files, 904 occurrences)

#### lib/core/ — lowlevel blocks that should become pure TML

| File | `lowlevel` count | C functions called | Migrate? |
|------|------------------:|--------------------|----|
| `str.tml` | 43 | str_trim, str_contains, str_split, str_replace, str_find, str_repeat, str_parse_*, str_chars, str_join | **YES** — pure algorithms |
| `fmt/impls.tml` | 26 | i8/i16/i32/i64/u8/u16/u32/u64_to_string, f32/f64_to_string, char_to_string | **YES** — pure formatting |
| `fmt/float.tml` | 11 | f32/f64_to_string, f32/f64_to_string_precision, f32/f64_to_exp_string | **YES** — pure formatting |
| `fmt/helpers.tml` | 5 | str_len, char_to_string, str_slice, i64_to_string | **YES** — pure helpers |
| `fmt/formatter.tml` | 3 | str_len, char_to_string, str_slice | **YES** — pure formatting |
| `fmt/traits.tml` | 1 | char_to_string | **YES** — trivial |
| `char/decode.tml` | 4 | char_to_string, utf8_*byte_to_string | **YES** — pure UTF-8 |
| `char/methods.tml` | 4 | char_to_string, utf8_*byte_to_string | **YES** — pure UTF-8 |
| `char/convert.tml` | 4 | char_to_string | **YES** — trivial |
| `ascii/char.tml` | 6 | char_to_string | **YES** — trivial |
| `hash.tml` | 3 | str_hash (djb2 algorithm) | **YES** — pure hash, tml_random_seed stays |
| `collections.tml` | 1 | list_get_mut | **YES** — with new List |

#### lib/core/ — lowlevel blocks that STAY (compiler intrinsics / memory)

| File | `lowlevel` count | Why it stays |
|------|------------------:|--------------|
| `intrinsics.tml` | 3 | @intrinsic declarations — compiler primitives |
| `mem.tml` | 7 | size_of, align_of, transmute, zeroed, forget — compiler intrinsics |
| `array/mod.tml` | 49 | array_get, array_set, array_as_ptr — compiler-generated array access |
| `array/iter.tml` | 11 | Array iteration via pointer offsets — compiler intrinsic |
| `array/ascii.tml` | 31 | ASCII array operations — compiler intrinsic |
| `slice/mod.tml` | 11 | slice_get, slice_set, slice_offset — compiler intrinsic |
| `slice/iter.tml` | 1 | Slice iteration — compiler intrinsic |
| `ptr/const_ptr.tml` | 7 | ptr_read, alignof — compiler intrinsic |
| `ptr/mut_ptr.tml` | 12 | ptr_write, memmove, memset — compiler intrinsic |
| `ptr/ops.tml` | 5 | memcpy, memmove, memset — LLVM intrinsics |
| `ptr/non_null.tml` | 3 | ptr_as_ref, alignof — compiler intrinsic |
| `ptr/mod.tml` | 1 | Pointer module — compiler intrinsic |
| `alloc/heap.tml` | 4 | mem_alloc, mem_free — OS allocator |
| `alloc/global.tml` | 4 | Global allocator — OS allocator |
| `sync.tml` | 15 | Atomic operations — CPU hardware |
| `pin.tml` | 8 | Pin projection — compiler safety |
| `ops/drop.tml` | 5 | Drop glue — compiler-generated |
| `cache.tml` | 8 | mem_alloc, mem_free, zeroed — allocator |
| `any.tml` | 5 | type_id, alloc, dealloc — compiler intrinsic |
| `pool.tml` | 3 | Memory pool — uses allocator |
| `async_iter/mod.tml` | 2 | Async iteration — compiler support |

#### lib/std/ — lowlevel blocks that should become pure TML

| File | `lowlevel` count | C functions called | Migrate? |
|------|------------------:|--------------------|----|
| `collections/list.tml` | 10 | list_create, list_push, list_pop, list_get, list_set, list_len, etc. | **YES** — pure data structure |
| `collections/hashmap.tml` | 14 | hashmap_create, hashmap_set, hashmap_get, hashmap_has, etc. | **YES** — pure data structure |
| `collections/buffer.tml` | 11 | buffer_create, buffer_write_*, buffer_read_*, buffer_slice, etc. | **YES** — pure byte buffer |
| `text.tml` | 48 | str_* text utilities, formatting helpers | **YES** — pure algorithms |
| `search/bm25.tml` | 14 | BM25 ranking algorithm | **YES** — pure algorithm |
| `search/hnsw.tml` | 15 | HNSW vector search | **YES** — pure algorithm |
| `search/distance.tml` | 11 | String distance metrics | **YES** — pure algorithm |
| `json/types.tml` | 4 | JSON value operations | **PARTIAL** — pure structure, SIMD parser can stay |
| `log.tml` | 18 | Logging with formatting | **PARTIAL** — formatting is pure, I/O stays |

#### lib/std/ — lowlevel blocks that STAY (OS/FFI)

| File | `lowlevel` count | Why it stays |
|------|------------------:|--------------|
| `sync/atomic.tml` | 78 | Atomic operations — CPU hardware |
| `sync/Arc.tml` | 15 | Atomic reference counting — CPU atomics |
| `sync/mpsc.tml` | 5 | Channel sync — OS primitives |
| `sync/queue.tml` | 5 | Concurrent queue — atomics |
| `sync/stack.tml` | 3 | Concurrent stack — atomics |
| `net/sys/mod.tml` | 71 | Socket syscalls — OS |
| `net/tls.tml` | 7 | TLS — external lib FFI |
| `net/pending/udp.tml` | 5 | UDP sockets — OS |
| `net/pending/buffer.tml` | 14 | Network buffer — OS I/O |
| `file/file.tml` | 11 | File I/O — OS syscalls |
| `file/dir.tml` | 3 | Directory ops — OS syscalls |
| `file/path.tml` | 14 | Path operations — OS paths |
| `glob.tml` | 5 | File globbing — OS pattern matching |
| `thread/local.tml` | 13 | Thread-local storage — OS threads |
| `crypto/*.tml` (7 files) | ~169 | All crypto — external lib FFI (OpenSSL/CNG) |

---

## Summary

### What migrates to TML

| Category | C lines removed | `lowlevel` blocks eliminated | Files affected |
|----------|----------------:|-----------------------------:|---------------:|
| Strings (core) | 1,201 + 1,057 | ~55 | str.tml, fmt/*.tml, char/*.tml, ascii/char.tml |
| Collections (std) | 1,353 | ~35 | list.tml, hashmap.tml, buffer.tml |
| Formatting (core) | 411 | ~45 | fmt/impls.tml, fmt/float.tml, fmt/helpers.tml |
| Text utilities (std) | (in text.c) | ~48 | text.tml |
| Search (std) | 98 | ~40 | bm25.tml, hnsw.tml, distance.tml |
| Hash (core) | 0 (in essential.c) | ~1 | hash.tml |
| Pool/Misc | 344 + 67 + 54 | ~5 | pool.tml, profile |
| **TOTAL** | **~4,585** | **~229** | **~25 files** |

### What stays in C

| Category | Lines | Reason |
|----------|------:|--------|
| Core runtime (essential.c, mem.c, mem_track.c) | 1,649 | I/O, panic, allocator, leak detection |
| OS interface (os.c, time.c) | 947 | Syscalls |
| Networking (net.c, dns.c, tls.c) | 1,756 | Socket syscalls + TLS FFI |
| Concurrency (sync.c, thread.c, async.c) | 2,217 | OS thread/sync primitives |
| Diagnostics (backtrace.c, log.c) | 1,229 | Platform-specific unwinding |
| Crypto (8 files) | 6,965 | External lib FFI (OpenSSL/CNG) |
| File I/O (file.c, glob.c) | ~500 | OS file syscalls |
| Compression (zlib/*.c) | ~1,200 | External lib FFI (zlib/brotli/zstd) |
| Test/Coverage | ~500 | Lock-free atomics, test harness |
| **TOTAL** | **~16,963** | |

### Net result

- **Before**: 47 C files, ~19,125 + ~5,000 lines across compiler/runtime + lib/runtime
- **After**: ~39 C files, ~16,963 lines (only OS/FFI/hardware)
- **Eliminated**: ~229 lowlevel blocks replaced with pure TML
- **675+ remaining lowlevel blocks** are legitimate (intrinsics, atomics, OS, FFI)

## Impact

- **Affected code**: `compiler/src/codegen/llvm/builtins/` (string, collections, math), `compiler/runtime/` (8 C files), `lib/std/runtime/collections.c`, `lib/core/src/`, `lib/std/src/`
- **Breaking change**: NO — TML API stays identical, only implementation moves from C to TML
- **User benefit**: Better optimization (inlining), type-safe generics, smaller runtime binary, cleaner architecture

## Risks

- **Compiler maturity**: `@lowlevel` blocks, pointer arithmetic, and generic monomorphization must be solid
- **Performance regression**: C may be faster for some operations. Need benchmarks before/after.
- **Incremental approach**: Must migrate one module at a time with tests passing at each step.

## Dependencies

- `@lowlevel` blocks must support pointer arithmetic correctly
- Generic monomorphization must work for `List[I32]`, `List[Str]`, `List[List[T]]` etc.
- `ptr_read`, `ptr_write`, `ptr_offset` intrinsics must be fully functional
- `mem_alloc`, `mem_free`, `mem_realloc` remain as C runtime (thin wrappers over malloc/free)

## Success Criteria

- All migrated operations implemented in pure TML using `@intrinsic` memory primitives
- All existing tests pass without modification
- ~229 `lowlevel` blocks eliminated from lib/ TML files
- 8 C files removed from compiler/runtime/, 1 from lib/std/runtime/
- No `call @str_*`, `call @list_*`, `call @hashmap_*` in generated LLVM IR for migrated functions
- Performance parity (within 10%) with C implementations on benchmarks
