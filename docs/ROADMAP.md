# TML Roadmap

**Last updated**: 2026-02-16
**Current state**: Compiler functional, 75.1% library coverage, 8,763 tests passing

---

## Overview

```
Phase 1  [DONE 97%]   Fix codegen bugs (closures, generics, iterators)
Phase 2  [DONE]       Tests for working features → coverage 58% → 75% ✓
Phase 3  [ACTIVE 48%] Standard library essentials (Math✓, Instant✓, HashSet✓, Args✓)
Phase 4  [THEN]       Migrate C runtime → pure TML
Phase 5  [LATER]      Async runtime, networking, HTTP
Phase 6  [DISTANT]    Self-hosting compiler (rewrite C++ → TML)
```

### Why this order

| Phase | Rationale |
|-------|-----------|
| 1. Codegen bugs | Blocks everything else. Without working closures, iterators never work. Without generic enums, `Maybe`/`Outcome` are unusable idiomatically. |
| 2. Test coverage | Proves what works, catches regressions, builds confidence for bigger changes. |
| 3. Stdlib essentials | Makes TML usable for real programs. Collections, math, datetime are table stakes. |
| 4. Runtime migration | Architectural cleanup. C runtime works fine but blocks self-hosting long-term. |
| 5. Async + networking | Enables servers and networked applications. Depends on stable closures + iterators. |
| 6. Self-hosting | Ultimate goal. Requires everything above to be solid. |

### Current metrics

| Metric | Value |
|--------|-------|
| Library function coverage | 75.1% (3,005/4,000) |
| Tests passing | 8,763 across 744 files |
| Modules at 100% coverage | 71 |
| Modules at 0% coverage | 31 |
| C++ compiler size | ~238,000 lines |
| C runtime to migrate | ~4,585 lines |
| TML standard library | ~137,300 lines |

---

## Phase 1: Fix Codegen Bugs

**Goal**: Unblock ~365+ library functions currently broken by compiler bugs
**Priority**: CRITICAL — blocks Phases 2-6
**Tracking**: [test-failures/tasks.md](../rulebook/tasks/test-failures/tasks.md)

### 1.1 Closures with variable capture

Closures are the single largest functional gap. They block iterators, functional patterns, and test framework features.

- [x] 1.1.1 Fix capturing closures — fat pointer `{ func_ptr, env_ptr }` architecture implemented, captures heap-allocated
- [x] 1.1.2 Fix tuple type arguments in trait definitions (DONE 2026-02-14)
- [x] 1.1.3 Fix returning closures with captures from functions (DONE 2026-02-15 — verified: non-capturing, capturing, mutable captures, string captures, nested closure returns, struct field closures all working)
- [x] 1.1.4 Fix function pointer field calling (DONE 2026-02-14)

### 1.2 Generic enum method instantiation

Blocks idiomatic use of `Maybe`, `Outcome`, `Poll`, `Bound`, and all generic enums.

- [x] 1.2.1 Fix `CoroutineState[T]` methods — type parameters not substituted
- [x] 1.2.2 Fix `Poll[T]` methods `is_ready`/`is_pending` — generic enum variant resolution
- [x] 1.2.3 Fix `Bound::Unbounded` resolution in generic contexts (DONE 2026-02-13)
- [x] 1.2.4 Fix behavior constraint methods (`debug_string`, `to_string`) on generic enums
- [x] 1.2.5 Fix generic closures in `Maybe` methods (`.map()`, `.and_then()`, etc.) (DONE 2026-02-14)
- [x] 1.2.6 Fix nested `Outcome` drop function generation (DONE 2026-02-14)
- [x] 1.2.7 Fix auto-drop glue for enums with non-trivial payloads (DONE 2026-02-15 — verified: `Maybe[Shared[I32]]` and `Outcome[Sync[I32], Str]` correctly drop payloads, refcount decremented on scope exit)

### 1.3 Iterator associated types

Blocks the entire iterator system — 45 source files, ~200+ functions at 0% coverage.

- [x] 1.3.1 Fix associated type substitution (`I::Item` to concrete type) in iterator adapters (DONE 2026-02-14)
- [x] 1.3.2 Fix default behavior method dispatch returning `()` on concrete types (`iter.count()`, `iter.last()`, etc.)
- [x] 1.3.3 Fix higher-order generic types — `OnceWith`, `FromFn`, `Successors` (DONE 2026-02-14)
- [ ] 1.3.4 Fix async iterator support — depends on async runtime; deferred

### 1.4 Generic function monomorphization

- [x] 1.4.1 Fix `mem::zeroed[T]`, `mem::transmute[S,D]` — generic type resolution for module-qualified calls
- [x] 1.4.2 Fix `HashMapIter::key()` / `HashMapIter::value()` (DONE 2026-02-13)
- [x] 1.4.3 Fix `Slice::from_array` / `as_slice()` / `as_mut_slice()` (DONE 2026-02-14)

### 1.5 Behavior dispatch on generic structs

- [x] 1.5.1 Fix `MutexGuard::deref` / `::deref_mut` (DONE 2026-02-13)
- [x] 1.5.2 Fix `RwLockReadGuard::deref` / `RwLockWriteGuard::deref` (DONE 2026-02-13)
- [x] 1.5.3 Fix `SocketAddrV4`/`SocketAddr` trait impls (DONE 2026-02-13)
- [x] 1.5.4 Fix `SocketAddr::from_v4()` (DONE 2026-02-13)

### 1.6 LLVM type mismatches

- [x] 1.6.1 Fix `Maybe[ref T]` — `OnceCell::get()` fixed (DONE 2026-02-14); `OnceLock::get()` also fixed (DONE 2026-02-15)
- [x] 1.6.2 Fix `Maybe`/`Outcome` `to_string` (DONE 2026-02-13)

### 1.7 Other blocking bugs

- [x] 1.7.1 Fix exception subclass allocation (DONE 2026-02-13)
- [x] 1.7.2 Fix external module method linking for `std::types::Object` (DONE 2026-02-13)
- [x] 1.7.3 Fix `unicode_data::UNICODE_VERSION` constant — tuple constants now supported (DONE 2026-02-15)
- [x] 1.7.4 Fix external inheritance for exception subclasses (DONE 2026-02-15 — verified: `ArgumentNullException`, `FileNotFoundException` (3-level deep), `InvalidOperationException`, `TimeoutException` all work across modules)
- [x] 1.7.5 Fix `Text::data_ptr` — SSO mode crash (DONE 2026-02-14)
- [x] 1.7.6 Fix `Saturating[T]::add/sub/mul()`, `Wrapping[T]::add/sub/mul/neg()` (DONE 2026-02-14)
- [x] 1.7.7 Fix `clone::Duplicate::duplicate` coverage tracking for primitive types (DONE 2026-02-15 — clone module at 100%)

### 1.8 Nested generic type codegen

- [x] 1.8.1 Fix `%struct.T` — generic struct type param not substituted in nested contexts (DONE 2026-02-14)
- [x] 1.8.4 Fix generic method instantiation for library-internal types — non-public structs (`StackNode[T]`) not found in module search because only `mod.structs` was checked, not `mod.internal_structs`; also `is_library_type` flag was incorrectly set based on `pending_generic_structs_` instead of `pending_generic_impls_` (DONE 2026-02-14 — unblocked 8 disabled test files: lockfree_queue, lockfree_stack_peek, mpsc_channel, sync_mpsc, mpsc_repro_mutex_ptr, mpsc_channel_creation, sync_collections, thread)
- [x] 1.8.2 Fix nested adapter type generation (DONE 2026-02-15 — verified: 5-level deep chains `map->filter->take->enumerate->count`, `skip->take->sum` all work)
- [x] 1.8.3 Fix `FromFn[F]` as adapter input (DONE 2026-02-15 — verified: `from_fn(...).map(...)`, `from_fn(...).filter(...)` chains work correctly)

### 1.9 Performance fix

- [ ] 1.9.1 Fix generic cache O(n^2) in test suites (`codegen/core/generic.cpp:303`)

**Progress**: 36/37 items fixed (~97%). Coverage jumped from 43.7% to 75.1% (+2,400+ functions). Remaining items: async iterator (1.3.4, deferred), `OnceLock::get_or_init` closure type mismatch (6e.3), generic cache perf (1.9.1).
**Gate**: Phase 1 effectively complete. Coverage at 75.1% with 8,763 tests.

---

## Phase 2: Test Coverage

**Goal**: 58% → 75%+ function coverage — **ACHIEVED** (75.1%)
**Priority**: HIGH — proves stability, catches regressions
**Tracking**: Coverage reports via `tml test --coverage`

### 2.1 Iterator system (~130 uncovered functions remaining)

| Module | Functions | Covered | Coverage |
|--------|-----------|---------|----------|
| `iter` (core) | 11 | 11 | 100% |
| `iter/range` | 30 | 15 | 50% |
| `iter/sources/from_fn` | 2 | 2 | 100% |
| `iter/sources/once_with` | 3 | 2 | 66.7% |
| `iter/sources/legacy` | 18 | 12 | 66.7% |
| `iter/adapters/step_by` | 2 | 2 | 100% |
| `iter/adapters/skip_while` | 2 | 2 | 100% |
| `iter/adapters/take_while` | 2 | 2 | 100% |
| `iter/adapters/chain` | 3 | 2 | 66.7% |
| `iter/adapters/enumerate` | 3 | 2 | 66.7% |
| `iter/adapters/zip` | 3 | 2 | 66.7% |
| `iter/adapters/take` | 3 | 2 | 66.7% |
| `iter/adapters/skip` | 3 | 2 | 66.7% |
| `iter/adapters/fuse` | 3 | 2 | 66.7% |
| `iter/adapters/inspect` | 3 | 2 | 66.7% |
| `iter/adapters/cycle` | 3 | 2 | 66.7% |
| `iter/adapters/filter` | 2 | 1 | 50% |
| `iter/adapters/map` | 3 | 1 | 33.3% |
| `iter/traits/iterator` | 18 | 0 | 0% |
| `iter/traits/accumulators` | 20 | 0 | 0% |
| `iter/traits/double_ended` | 4 | 0 | 0% |
| `iter/traits/exact_size` | 1 | 0 | 0% |
| `iter/sources/empty` | 3 | 0 | 0% |
| `iter/sources/repeat_with` | 3 | 0 | 0% |
| `iter/sources/successors` | 2 | 0 | 0% |
| `iter/adapters/peekable` | 7 | 0 | 0% |
| `iter/adapters/rev` | 4 | 0 | 0% |
| `iter/adapters/*` (6 more) | ~16 | 0 | 0% |
| `slice/iter` | 19 | 0 | 0% |
| `array/iter` | 19 | 0 | 0% |

- [x] 2.1.1 Test `Iterator` trait core methods: `next`, `count` (DONE — `next` on RangeIter/Counter, `count` 4 tests, `advance_by` 4 tests; `size_hint`/`last`/`nth` blocked by tuple return / default method dispatch codegen)
- [ ] 2.1.2 Test accumulator methods: `sum`, `product`, `fold`, `reduce`, `all`, `any`, `find` (blocked by default behavior method dispatch — returns `()` instead of expected type)
- [ ] 2.1.3 Test `DoubleEndedIterator`: `next_back`, `rfold`, `rfind` (not yet tested)
- [x] 2.1.4 Test iterator adapters: `map`, `filter`, `enumerate`, `zip`, `chain`, `take`, `skip` (DONE — map 1 test, take 3, skip 3, chain 3, enumerate 2, zip 3; `filter` partially covered)
- [x] 2.1.5 Test iterator adapters: `fuse` (DONE — 2 tests; `flat_map`, `flatten`, `peekable`, `scan` not yet tested)
- [x] 2.1.6 Test iterator adapters: `inspect`, `step_by`, `cycle` (DONE — inspect 2, step_by 4, cycle 2; `intersperse` not yet tested)
- [x] 2.1.7 Test iterator adapters: `take_while`, `skip_while` (DONE — take_while 3, skip_while 3; `map_while`, `filter_map` not yet tested)
- [x] 2.1.8 Test iterator sources: `empty`, `once`, `once_with`, `repeat_n`, `from_fn` (DONE — empty 2, once 7, repeat_n 7, once_with 3, from_fn 4; `repeat_with`/`successors` blocked)
- [x] 2.1.9 Test `iter/range` — Step behavior + Range/RangeInclusive iteration (DONE — forward/backward_checked 25 tests all int types, Range 2 tests, RangeInclusive 3 tests; `steps_between` blocked by static method dispatch)
- [ ] 2.1.10 Test `slice/iter` and `array/iter` — slice and array iteration (blocked by const generics codegen)
- [ ] 2.1.11 Test `IntoIterator`, `FromIterator`, `Extend` traits (not yet tested)
- [ ] 2.1.12 Test `ExactSizeIterator` trait (not yet tested)

> **Note**: Closures (1.1) and iterator codegen (1.3) are fixed. Remaining blockers: default behavior method dispatch (accumulators), `size_hint` tuple return codegen, const generics (array/slice iter), static method dispatch (`steps_between`).

### 2.2 Operators (183 functions, 88% coverage)

| Module | Functions | Covered | Coverage |
|--------|-----------|---------|----------|
| `ops/arith` | 102 | 102 | 100.0% |
| `ops/bit` | 81 | 59 | 72.8% |

- [x] 2.2.1 Test `Add/Sub/Mul/Div/Rem` for all integer and float types (DONE 2026-02-14)
- [x] 2.2.2 Test `AddAssign/SubAssign/MulAssign/DivAssign/RemAssign` for all types (DONE 2026-02-14)
- [x] 2.2.3 Test `BitAnd/BitOr/BitXor/Shl/Shr` for all integer types (DONE 2026-02-14)
- [x] 2.2.4 Test `BitAndAssign/BitOrAssign/BitXorAssign/ShlAssign/ShrAssign` for all types (DONE 2026-02-14)
- [x] 2.2.5 Test `Neg` (unary minus) for signed types and floats (DONE 2026-02-14)
- [x] 2.2.6 Test `Not` (bitwise not) for integer types and Bool (DONE 2026-02-14)

### 2.3 Formatting implementations (72 functions, 97.2% coverage)

| Module | Functions | Covered | Coverage |
|--------|-----------|---------|----------|
| `fmt/impls` | 72 | 70 | 97.2% |

- [x] 2.3.1 Test `Display` impl for all primitive types (I8-I128, U8-U128, F32, F64, Bool, Char, Str) (DONE 2026-02-14)
- [x] 2.3.2 Test `Debug` impl for all primitive types (DONE 2026-02-14)
- [x] 2.3.3 Test `Binary`, `Octal`, `LowerHex`, `UpperHex` for integer types (DONE 2026-02-14)
- [x] 2.3.4 Test `LowerExp`, `UpperExp` for float types (DONE 2026-02-14)
- [x] 2.3.5 Test `Display`/`Debug` for compound types (Maybe, Outcome, Ordering) (DONE 2026-02-14; tuples blocked by codegen)

### 2.4 Slices and arrays (50+ functions, partial coverage)

| Module | Functions | Coverage |
|--------|-----------|----------|
| `slice` | ~30 | ~50% |
| `slice/cmp` | 9 | ~100% |
| `slice/sort` | 12 | partial |
| `array/ascii` | 9 | 0% |

- [x] 2.4.1 Test slice comparison methods (DONE 2026-02-14)
- [x] 2.4.2 Test slice sort, rotate_left, rotate_right, split_at, copy_from_slice (DONE 2026-02-14; sort_by/sort_by_key blocked by closure ref codegen)
- [ ] 2.4.3 Test array ASCII methods

### 2.5 Option and Result (Maybe/Outcome)

- [x] 2.5.0.1 Test `Maybe[I32]`: expect, unwrap_or_default, unwrap_or_else, or_else, one_of, map_or (DONE 2026-02-14)
- [x] 2.5.0.2 Test `Outcome[I32,Str]`: expect, expect_err, is_ok_and, is_err_and, unwrap_or_default, unwrap_or_else, alt, ok() (DONE 2026-02-14)
- [ ] 2.5.0.3 Test remaining methods blocked by generic codegen: map[U], and_then[U], also[U], filter (ref closure), ok_or[E]

### 2.5.1 Borrow, Default, CMP gaps

- [x] 2.5.1.1 Test `F32::default()`, `F64::default()` (DONE 2026-02-14)
- [x] 2.5.1.2 Test `Cow[I32]`: is_borrowed, is_owned, into_owned for Owned and Borrowed variants (DONE 2026-02-14)
- [x] 2.5.1.3 Test `Ord::max`, `Ord::min`, `Ord::clamp` on I32 (DONE 2026-02-14)
- [x] 2.5.1.4 Test `cmp::max[T]`, `cmp::min[T]` free functions (DONE 2026-02-14)
- [x] 2.5.1.5 Test `Ordering::eq` (PartialEq impl) for all variant combinations (DONE 2026-02-14)
- [ ] 2.5.1.6 `Ordering::ne` blocked by default impl codegen (ref type mismatch)
- [ ] 2.5.1.7 `PartialEq::eq/ne`, `PartialOrd::lt/le/gt/ge` on primitives blocked by codegen

### 2.6 Convert and type coercion (6 functions, 0%)

- [ ] 2.6.1 Test `From`/`Into` implementations
- [ ] 2.6.2 Test `TryFrom`/`TryInto` implementations

### 2.7 String module gaps (49/54 covered, 90.7%)

- [ ] 2.7.1 Test remaining 12 uncovered `str` functions (parse_i64, parse_u16 blocked by Maybe layout; as_bytes blocked by Slice unsized type)
- [ ] 2.7.2 Test `bstr` module (4/5 covered)

### 2.8 Memory module (17/20 covered, 85%)

- [ ] 2.8.1 Test remaining 4 uncovered `mem` functions (`zeroed`, `transmute`, etc.)

> **Note**: Depends on Phase 1.4.1 being fixed first.

### 2.9 Pointer module (14/19 covered, 73.7%)

- [ ] 2.9.1 Test remaining 5 uncovered `ptr/const_ptr` functions

### 2.10 Other gaps with existing implementations

- [ ] 2.10.1 Test `cell/lazy` — 5 functions at 0% (blocked: generic `LazyCell[T,F]`)
- [ ] 2.10.2 Test `thread/scope` — 9 functions at 0%
- [ ] 2.10.3 Test `ops/function` — 3 functions at 0%
- [ ] 2.10.4 Test `future` — 12 functions at 0%
- [ ] 2.10.5 Test `runner` (test framework) — 9 functions at 0%
- [x] 2.10.6 Test `zlib/error` — 6 functions: `zlib_error_kind_from_code`, `with_code`, `with_message`, `from_code`, `is_ok`, `to_string` (DONE — 16 tests)
- [x] 2.10.7 Test `net/error` — 15 functions: all `NetErrorKind` constructors + `NetError::kind`, `would_block`, `from_last_error` (DONE — 15 tests)
- [x] 2.10.8 Test `crypto/hash` — 11 functions: `digest_size`, `block_size`, `Digest::bytes`, `Hash::copy`, `Hash::update_bytes`, `*_bytes` one-shot variants (DONE — 11 tests)
- [x] 2.10.9 Test `crypto/cipher` — 4 functions: `CipherAlgorithm::block_size`, `is_aead`, `tag_size`, `from_name` (DONE — 10 tests)
- [x] 2.10.10 Test `crypto/sign` — 6 functions: `SignatureAlgorithm::name/is_rsa/is_ecdsa/is_eddsa/is_pss/from_name`, `PssOptions::default/with_salt_length` (DONE — 11 tests)
- [x] 2.10.11 Test `crypto/random` — 7 functions: `timing_safe_equal_str`, `SecureRandom::next_u8/u16/i32/i64/f32/fill` (DONE — 8 tests; `generate_prime`, `check_prime` blocked by `Outcome[Buffer,E]` codegen)
- [x] 2.10.12 Test `iter/range` Step behavior: `forward_checked`, `backward_checked` on all 8 integer types (DONE — 22 tests; `steps_between` blocked by static method dispatch)
- [x] 2.10.13 Test `char/convert` extra: `to_u128`, `ParseCharError` methods (DONE — 5 tests)
- [x] 2.10.14 Test `fmt/builders` extra: `field_with`, `finish_non_exhaustive`, `entries`, `key().value()` (DONE — 6 tests)
- [x] 2.10.15 Test `num/overflow` `checked_shl`/`checked_shr` on I32 (DONE — 6 tests; `overflowing_*` blocked)
- [x] 2.10.16 Test `result` `err()` method (DONE — 2 tests)
- [x] 2.10.17 Test `collections/buffer` `duplicate` (DONE — 1 test; pushed to 100%)
- [x] 2.10.18 Test `crypto/key` — 15 functions: `KeyType::name/is_rsa/is_ec/from_name`, `KeyFormat::name`, `KeyEncoding::name`, `RsaKeyGenOptions::default/rsa3072/rsa4096`, `EcKeyGenOptions::p256/p384/p521/secp256k1` (DONE — 15 tests)
- [x] 2.10.19 Test `crypto/kdf` — 9 functions: `Argon2Variant::name`, `Argon2Params::default/high_security/low_memory/custom`, `ScryptParams::default/high_security/low_memory/custom` (DONE — 9 tests; FFI functions `pbkdf2/scrypt/hkdf/argon2/bcrypt` blocked by `Outcome[Buffer,E]`)
- [x] 2.10.20 Test `crypto/ecdh` — 9 functions: `EcCurve::name/key_bits/shared_secret_size/is_modern/from_name`, `EcPointFormat::name`, brainpool variants (DONE — 9 tests)
- [x] 2.10.21 Test `crypto/dh` — 5 functions: `DhGroup::name/prime_bits/is_deprecated/from_name` (DONE — 5 tests)
- [x] 2.10.22 Test `crypto/rsa` — 8 functions: `RsaPadding::name/is_oaep/overhead/max_data_size`, `OaepOptions::default/sha1/sha384/sha512` (DONE — 8 tests)
- [x] 2.10.23 Test `alloc/layout` — 27 functions: `LayoutError::new/to_string/debug_string`, `Layout::from_size_align` (valid/zero/invalid), accessors, `padding_needed_for`, `pad_to_align`, `align_to`, `with_size/with_align`, `extend_packed`, `repeat_packed`, `array`, `array_with_padding`, `array_of`, `equals`, `to_string/debug_string`, utility functions (`is_power_of_two`, `is_valid_align`, `is_aligned`, `padding_needed`, `align_up`, `align_down`, `next_power_of_two`) (DONE — 27 tests; pushed to 100%)
- [x] 2.10.24 Test `crypto/cipher` extra — 3 functions: `CipherAlgorithm::name`, `key_size`, `iv_size` (DONE — 3 tests added to cipher_gaps; total 12 tests)
- [x] 2.10.25 Test `zlib/options` — 12 functions: `BrotliOptions::default/text/font/fast/best/with_quality`, `ZstdOptions::default/fast/best/parallel/with_level/with_checksum` (DONE — 12 tests; pushed to 100%; `ZlibOptions` blocked by `Maybe[Buffer]` codegen)
- [x] 2.10.26 Test `sync/ordering` extra — 1 function: `has_release` false cases (DONE — 1 test added; pushed to 75%)
- [x] 2.10.27 Test `core/convert` — 15 functions: `From[I8] for I16/I32/I64`, `From[I16] for I32/I64`, `From[I32] for I64`, `From[U8] for U16/U32/U64`, `From[U16] for U32/U64`, `From[U32] for U64`, `From[F32] for F64`, `From[Bool] for I32/I64` (DONE — 15 tests in 2 files; unsigned tests need separate file due to codegen issue with mixed From impls)
- [x] 2.10.28 Test `fmt/traits` — 6 functions: `FmtError::new/to_string/debug_string`, `Alignment::to_string/debug_string`, `Sign::to_string/debug_string` (DONE — 6 tests)
- [x] 2.10.29 Test `ops/async_function` — 3 functions: `Poll::to_string/debug_string`, `is_ready/is_pending` (DONE — 6 tests for generic `Poll[I32]`)
- [x] 2.10.30 Test `unicode/unicode_data` — 8 functions: `is_lowercase_nonascii`, `is_uppercase_nonascii`, `is_whitespace_nonascii`, `is_numeric_nonascii`, `is_control_nonascii`, `is_grapheme_extend_nonascii`, `is_printable_nonascii`, `to_titlecase_nonascii` (DONE — 8 tests; `lookup_category` blocked by char type codegen)
- [x] 2.10.31 Test `ops/try_trait` ControlFlow — 2 functions: `ControlFlow[I32,Str]::is_continue/is_break` (DONE — 2 tests)
- [x] 2.10.32 Test `core/cache` SoaVec — 3 functions: `SoaVec::new/with_capacity/clear` (DONE — 3 tests added to existing cache.test.tml)
- [x] 2.10.33 Test `core/marker` PhantomPinned — 3 functions: `PhantomPinned::new/to_string/debug_string/default` (DONE — 3 tests)
- [x] 2.10.34 Test `core/cmp` Ordering — 6 functions: `Ordering::is_less/is_equal/is_greater/reverse/then_cmp` (DONE — 6 tests)
- [x] 2.10.35 Test `core/ops/range` Bound — 5 functions: `Bound[I32]::is_included/is_excluded/is_unbounded/to_string/debug_string` (DONE — 5 tests for generic enum)
- [x] 2.10.36 Test `core/hash` — 11 functions: `combine_hashes`, `DefaultHasher::new/with_seed/write_u8/write_i32/write_i64/finish`, `RandomState::with_keys/build_hasher` (DONE — 11 tests; primitive `.hash()` behavior dispatch crashes)
- [x] 2.10.37 Test `core/num/nonzero` NonZero — 4 functions: `NonZero::new(valid)/new(zero)/get`, negative values (DONE — 4 tests; `.eq()` blocked by generic type name mismatch in codegen)
- [x] 2.10.38 Test `core/num/traits` Zero/One/Bounded — 9 functions: `I32/I8/U8/I64/F64 ::zero/one/is_zero/is_one/min_value/max_value` (DONE — 9 tests)
- [x] 2.10.39 Test `core/ptr/alignment` — 8 functions: `align_up/align_down/is_aligned_to/align_offset/is_power_of_two/checked_next_power_of_two/prev_power_of_two/log2` (DONE — 8 tests)
- [x] 2.10.40 Test `core/num/integer` — 9 functions: `abs_i32/signum_i32/pow_i32/abs_i64/signum_i64/pow_i64`, `I32::abs/signum/is_positive/is_negative` methods (DONE — 9 tests)
- [x] 2.10.41 Test `core/cell/once` — 6 functions: `OnceCell::new/with_value/set/set_twice/into_inner`, `BorrowError/BorrowMutError::to_string` (DONE — 6 tests)
- [x] 2.10.42 Test `core/ops/coroutine` — 4 functions: `GeneratorState::Complete/Yielded/is_yielded/is_complete` (DONE — 4 tests)
- [x] 2.10.43 Expand `core/time` Duration coverage — 9 functions: `checked_add/checked_sub` (basic, nanos carry, underflow, nanos borrow), `eq`, `to_string` fractional, `mul/div` with nanos (DONE — 9 tests added, total 30)
- [x] 2.10.44 Expand `core/error` IoErrorKind coverage — 6 variants: `ConnectionReset/NotConnected/AddrInUse/WouldBlock/InvalidInput/UnexpectedEof` display+debug (DONE — 6 tests added, total 27)
- [x] 2.10.45 Complete `core/error` full coverage — 8 tests: remaining IoErrorKind variants `ConnectionAborted/AddrNotAvailable/InvalidData/WriteZero/Interrupted/OutOfMemory` display+debug, `IoError::debug_string` (no message + with message) (DONE — 8 tests added, total 35)
- [x] 2.10.46 Expand `core/error` BoxedError+ParseError — 3 tests: `BoxedError::from_message` display, `BoxedError::debug_string`, `ParseError::debug_string` with position (DONE — 3 tests added, total 38)
- [x] 2.10.47 Expand `core/time` Duration edge cases — 3 tests: `is_zero`, `saturating_add` nanos carry, `to_string` zero display (DONE — 3 tests added, total 33)
- [x] 2.10.48 Expand `core/unicode/char` classification — 9 tests: `is_grapheme_extend`, `eq_ignore_case`, `to_titlecase`, `is_letter`, `is_number`, `is_punctuation`, `is_symbol`, `is_separator`, `is_mark` (DONE — 9 tests added, total 12; `general_category`/`is_unassigned` blocked by enum when codegen bug)
- [x] 2.10.49 New `core/ops/bit_assign` tests — 23 tests: bitand/bitor/bitxor/shl/shr across I8/I16/I64/U8/U16/U32/U64 (DONE — new file ops_bit_assign.test.tml)
- [x] 2.10.50 Expand `core/char/methods` nonascii — 4 tests: `is_lowercase` Greek α, `is_uppercase` Greek Α, `is_whitespace` em space, `is_numeric` Arabic-Indic/Fullwidth/Superscript digits (DONE — 4 tests added, total 10)
- [x] 2.10.51 New `core/alloc/global` helpers — 6 tests: `layout_bytes`, `layout_bytes_aligned`, `alloc_single/dealloc_single`, `alloc_global_zeroed`, `realloc_global`, bad alignment error (DONE — new file alloc_global_helpers.test.tml)
- [x] 2.10.52 Expand `core/ops/coroutine` CoroutineResumePoint — 2 tests: `CoroutineResumePoint::Start` and `Finished` debug_string (DONE — 2 tests added, total 6; `AtYield(I64)` debug_string blocked by enum data extraction codegen bug)
- [x] 2.10.53 Expand `core/unicode/unicode_data` — 5 tests: `is_alphabetic_nonascii`, `to_uppercase_nonascii` Greek/Cyrillic, `to_lowercase_nonascii` Greek/Cyrillic (DONE — 5 tests added, total 13; `lookup_category` blocked by GeneralCategory i16/i32 codegen bug)

**Progress**: Phase 2 COMPLETE. ~520+ new tests added across sessions, reaching 75.1% (3,005/4,000):
- option/result, borrow/Cow, cmp/Ord, default, fmt compounds, slice operations (~76 tests)
- iter/range Step (22 tests), char/convert (5 tests), fmt/builders (6 tests)
- num/overflow checked_shl/shr (6 tests), result err() (2 tests), buffer duplicate (1 test)
- zlib/error (16 tests), net/error (15 tests), crypto/hash (11 tests)
- crypto/cipher (13 tests), crypto/sign (11 tests), crypto/random (8 tests)
- crypto/key (15 tests), crypto/kdf (9 tests), crypto/ecdh (9 tests)
- crypto/dh (5 tests), crypto/rsa (8 tests)
- alloc/layout (27 tests), zlib/options (12 tests), sync/ordering (4 tests)
- convert (15 tests), fmt/traits (6 tests), Poll (6 tests), unicode_data (8 tests)
- ControlFlow (2 tests), cache/SoaVec (3 tests)
- marker/PhantomPinned (3 tests), cmp/Ordering (6 tests), ops/range Bound (5 tests)
- hash (11 tests), nonzero (4 tests), num/traits (9 tests), ptr/alignment (8 tests), num/integer (9 tests)
- cell/once (6 tests), ops/coroutine (4 tests)
- time/Duration expanded (12 tests), error/IoErrorKind+BoxedError expanded (17 tests)
- unicode/char expanded (9 tests), ops/bit_assign (23 tests), char/methods nonascii (4 tests)
- alloc/global helpers (6 tests), ops/coroutine CoroutineResumePoint (2 tests), unicode_data expanded (5 tests)
- intrinsics/bits (7 tests), hash_hasher_coverage (14 tests), str_coverage_extra (4 tests)
- coverage/quick_wins: handle_alloc_error, TryFromCharError, DecodeUtf16Error, surrogates (19 tests)
- coverage/math intrinsics: sin, cos, floor, ceil, trunc, exp, fabs (7 tests)
- coverage/error module: ParseError, IoError constructors and methods (5 tests)

Compiler fixes enabling coverage push:
- Fixed 16 missing intrinsic names in recognition set
- Fixed coverage scanner to skip bodyless behavior declarations (~147 false positives removed)
- Fixed VEH crash handler with SEH recovery wrapper
- Fixed partial coverage generation when tests fail

Remaining uncovered areas blocked by: generic codegen (map[U], and_then[U], ok_or[E], Maybe::default), multi-arg LLVM intrinsics (minnum, maxnum, fma, copysign), Unit type methods, class inheritance method dispatch, Char→i32 type codegen.
**Note**: Full test suite crashes on x509/DH crypto tests (ACCESS_VIOLATION in OpenSSL). Coverage JSON generates before crash.
**Gate**: Coverage >= 75% — **ACHIEVED** (75.1%, 3,005/4,000 functions). 71 modules at 100%.

---

## Phase 3: Standard Library Essentials

**Goal**: Make TML usable for real programs
**Priority**: HIGH
**Tracking**: [stdlib-essentials/tasks.md](../rulebook/tasks/stdlib-essentials/tasks.md)

### 3.1 Collections

- [x] 3.1.1 Implement `HashSet[T]` — insert, remove, contains *(re-enabled in `std::collections::class_collections`, also includes ArrayList, Queue, Stack, LinkedList)*
- [ ] 3.1.2 Implement `BTreeMap[K, V]` — ordered map with O(log n) operations
- [ ] 3.1.3 Implement `BTreeSet[T]` — ordered set
- [ ] 3.1.4 Implement `Deque[T]` — double-ended queue
- [ ] 3.1.5 Implement `Vec[T]` alias for `List[T]` (ergonomic)
- [x] 3.1.6 Tests for collections *(HashSet, ArrayList, Queue, Stack, LinkedList tests passing)*

### 3.2 Math module ✓

- [x] 3.2.1 Trigonometric: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2` *(implemented in `std::math` via intrinsics + libc FFI)*
- [x] 3.2.2 Hyperbolic: `sinh`, `cosh`, `tanh` *(implemented via libc FFI)*
- [x] 3.2.3 Exponential: `exp`, `ln`, `log2`, `log10` *(implemented; `pow` via `exp(y * ln(x))` helper)*
- [x] 3.2.4 Rounding: `floor`, `ceil`, `round`, `trunc` *(implemented via intrinsics)*
- [x] 3.2.5 Utility: `abs`, `sqrt`, `cbrt`, `min`, `max`, `clamp` *(implemented)*
- [x] 3.2.6 Constants: `PI`, `E`, `TAU`, `SQRT_2`, `LN_2`, `LN_10` *(plus `LOG2_E`, `LOG10_E`, `FRAC_1_PI`, `FRAC_2_PI`, `FRAC_1_SQRT_2`)*
- [x] 3.2.7 Tests for math module *(30 tests across 4 files: constants, trig, functions, advanced)*

### 3.3 Environment and process

- [x] 3.3.1 `env::args()` — command-line arguments *(implemented as `os::args_count()` + `os::args_get(index)` with C FFI)*
- [x] 3.3.2 `env::var()` / `env::set_var()` — environment variables *(implemented in `std::os`: `env_get`, `env_set`, `env_unset`)*
- [ ] 3.3.3 `env::current_dir()` / `env::set_current_dir()`
- [x] 3.3.4 `process::exit(code)` — exit with status code *(implemented as `os::process_exit(code)` via `tml_os_exit` FFI)*
- [ ] 3.3.5 `process::Command` — spawn subprocesses with stdin/stdout/stderr
- [x] 3.3.6 Tests for env and process *(args, env_get/set/unset, process_exit tests passing)*

### 3.4 DateTime

- [x] 3.4.1 `Instant` — monotonic clock for measuring elapsed time *(implemented in `std::time`: `Instant::now()`, `elapsed()`, `as_nanos()`, `duration_since()`)*
- [ ] 3.4.2 `SystemTime` — wall clock time
- [ ] 3.4.3 `DateTime` — date + time with timezone support
- [ ] 3.4.4 Formatting: ISO 8601, RFC 2822, custom formats
- [ ] 3.4.5 Parsing: string → DateTime
- [x] 3.4.6 Tests for datetime *(Instant tests passing: now, elapsed, sleep)*

### 3.5 Random number generation

- [ ] 3.5.1 `Rng` behavior — `next_u32`, `next_u64`, `next_f64`
- [ ] 3.5.2 `ThreadRng` — per-thread CSPRNG
- [ ] 3.5.3 `random[T]()` — convenience function for random value
- [ ] 3.5.4 `rng.range(min, max)` — random integer in range
- [ ] 3.5.5 `rng.shuffle(list)` — Fisher-Yates shuffle
- [ ] 3.5.6 Tests for random

### 3.6 Buffered I/O

- [ ] 3.6.1 `BufReader[R]` — buffered wrapper for Read types
- [ ] 3.6.2 `BufWriter[W]` — buffered wrapper for Write types
- [ ] 3.6.3 `Read` / `Write` / `Seek` behaviors
- [ ] 3.6.4 `LineWriter` — flush on newline
- [ ] 3.6.5 Tests for buffered I/O

### 3.7 Error context chains ✓

- [x] 3.7.1 `Context` behavior — `.context("msg")` on errors *(implemented in `core::error`: `.context()`, `.with_context()`)*
- [x] 3.7.2 `Error` source chain — `.source()` for error chaining *(implemented: `Error` behavior with `source()`, `ChainedError[E]`, `ErrorChain` iterator)*
- [x] 3.7.3 `AnyError` type — anyhow-style generic errors *(implemented: `BoxedError`, `SimpleError` for type-erased errors)*
- [x] 3.7.4 Display with backtrace and chain *(implemented: `error_chain()` function, `ErrorChain` iterator)*
- [x] 3.7.5 Tests for error chains *(tested in `core/tests/coverage/quick_wins_75e.test.tml`)*

### 3.8 Regex engine

- [ ] 3.8.1 `Regex` type — compile pattern
- [ ] 3.8.2 `is_match()`, `find()`, `find_all()`
- [ ] 3.8.3 `captures()` — named and positional groups
- [ ] 3.8.4 `replace()`, `replace_all()`, `split()`
- [ ] 3.8.5 Character classes, quantifiers, Unicode support
- [ ] 3.8.6 NFA/DFA hybrid engine (no exponential backtracking)
- [ ] 3.8.7 Tests for regex

**Gate**: `HashSet`, `BTreeMap`, `Math`, `DateTime`, `Random`, `BufReader/BufWriter` all working with tests.

---

## Phase 4: Migrate C Runtime to Pure TML

**Goal**: Eliminate ~4,585 lines of unnecessary C runtime code
**Priority**: MEDIUM — architectural cleanup, prerequisite for self-hosting
**Tracking**: [migrate-runtime-to-tml/tasks.md](../rulebook/tasks/migrate-runtime-to-tml/tasks.md)

### Architecture

```
KEEP as C (@extern / @intrinsic):          MIGRATE to pure TML:
  - mem_alloc / mem_free (OS interface)      - String algorithms (1,201 lines)
  - ptr_read / ptr_write (LLVM intrinsics)   - Text utilities (1,057 lines)
  - print / panic / exit (I/O)               - Collections logic (1,353 lines)
  - File I/O (OS syscalls)                   - Math formatting (411 lines)
  - Crypto (OpenSSL/BCrypt FFI)              - Search algorithms (98 lines)
  - Compression (zlib FFI)                   - Logging internals
  - Networking (OS sockets)                  - JSON helpers
```

### 4.1 Collections (List, HashMap, Buffer)

- [ ] 4.1.1 Rewrite `List[T]` — replace C `list_create`/`list_push`/`list_get` with TML using `ptr_read`/`ptr_write`/`mem_alloc`
- [ ] 4.1.2 Rewrite `HashMap[K, V]` — replace C `hashmap_create`/`hashmap_set`/`hashmap_get` with TML
- [ ] 4.1.3 Rewrite `Buffer` — replace C `buffer_create`/`buffer_push`/`buffer_data` with TML
- [ ] 4.1.4 Remove `compiler/runtime/collections/collections.c` (1,353 lines)
- [ ] 4.1.5 Remove `lib/std/runtime/collections.c` (duplicate)
- [ ] 4.1.6 All existing collection tests pass

### 4.2 Strings (core::str)

- [ ] 4.2.1 Rewrite read-only operations: `str_len`, `str_contains`, `str_starts_with`, `str_ends_with`, `str_index_of`
- [ ] 4.2.2 Rewrite transformations: `str_to_upper`, `str_to_lower`, `str_trim`, `str_trim_start`, `str_trim_end`
- [ ] 4.2.3 Rewrite splitting: `str_split`, `str_split_at`, `str_lines`
- [ ] 4.2.4 Rewrite allocating: `str_concat`, `str_repeat`, `str_replace`, `str_join`
- [ ] 4.2.5 Rewrite parsing: `str_parse_i32`, `str_parse_i64`, `str_parse_f64`, `str_parse_bool`
- [ ] 4.2.6 Remove `compiler/runtime/text/string.c` (1,201 lines)
- [ ] 4.2.7 All existing string tests pass

### 4.3 Text utilities (std::text)

- [ ] 4.3.1 Rewrite `Text` builder: `text_new`, `text_push`, `text_push_str`, `text_to_str`
- [ ] 4.3.2 Rewrite text operations: `text_trim`, `text_contains`, `text_replace`
- [ ] 4.3.3 Remove `compiler/runtime/text/text.c` (1,057 lines)
- [ ] 4.3.4 All existing text tests pass

### 4.4 Formatting (core::fmt)

- [ ] 4.4.1 Rewrite integer formatting: `i32_to_string`, `i64_to_string`, `u32_to_string`, etc.
- [ ] 4.4.2 Rewrite float formatting: `f32_to_string`, `f64_to_string` (Ryu or similar algorithm)
- [ ] 4.4.3 Rewrite `char_to_string`, UTF-8 encoding
- [ ] 4.4.4 Rewrite hex/octal/binary formatting
- [ ] 4.4.5 Remove `compiler/runtime/math/math.c` (411 lines)
- [ ] 4.4.6 All existing formatting tests pass

### 4.5 Search algorithms

- [ ] 4.5.1 Rewrite BM25 scoring in pure TML
- [ ] 4.5.2 Rewrite HNSW vector search in pure TML
- [ ] 4.5.3 Rewrite distance metrics (cosine, euclidean, dot product)
- [ ] 4.5.4 Remove `compiler/runtime/search/search.c` (98 lines)
- [ ] 4.5.5 All existing search tests pass

### 4.6 Cleanup

- [ ] 4.6.1 Remove all migrated C files from `compiler/runtime/`
- [ ] 4.6.2 Remove duplicate C files from `lib/std/runtime/`
- [ ] 4.6.3 Update build scripts to exclude removed files
- [ ] 4.6.4 Verify: zero `lowlevel` blocks call migrated C functions
- [ ] 4.6.5 Verify: `compiler/runtime/core/essential.c` is the ONLY remaining C runtime (I/O, panic, test harness)

**Gate**: ~229 `lowlevel` blocks eliminated. C runtime reduced from ~19K lines to ~14K lines (only essential I/O remains).

---

## Phase 5: Async Runtime and Networking

**Goal**: Enable servers and networked applications
**Priority**: MEDIUM
**Dependencies**: Phase 1 (closures must work), Phase 3 (buffered I/O)
**Tracking**: [language-completeness-roadmap/tasks.md](../rulebook/tasks/language-completeness-roadmap/tasks.md) M3-M4

### 5.1 Async runtime

- [ ] 5.1.1 Event loop (epoll/IOCP/kqueue)
- [ ] 5.1.2 `async func` — state machine codegen
- [ ] 5.1.3 `await` expression — suspension and resumption
- [ ] 5.1.4 `Executor` with work-stealing scheduler
- [ ] 5.1.5 `spawn()`, `block_on()`, `sleep()`, `timeout()`
- [ ] 5.1.6 `AsyncMutex[T]`, `AsyncChannel[T]`, `AsyncSemaphore`
- [ ] 5.1.7 `select!`, `join!`
- [ ] 5.1.8 Tests and benchmarks

### 5.2 Networking (async layer on existing sync)

> Sync TCP/UDP/DNS/TLS already implemented

- [ ] 5.2.1 `AsyncTcpListener` / `AsyncTcpStream`
- [ ] 5.2.2 `AsyncUdpSocket`
- [ ] 5.2.3 Zero-copy buffer management
- [ ] 5.2.4 Connection pooling
- [ ] 5.2.5 Tests: async echo server, concurrent clients

### 5.3 HTTP

- [ ] 5.3.1 HTTP/1.1 parser and server
- [ ] 5.3.2 HTTP/1.1 client with connection pooling
- [ ] 5.3.3 HTTP/2 multiplexing
- [ ] 5.3.4 Router with path matching and method routing
- [ ] 5.3.5 Middleware pipeline (logging, auth, CORS, compression)
- [ ] 5.3.6 Decorator-based routing: `@Controller`, `@Get`, `@Post`
- [ ] 5.3.7 WebSocket support
- [ ] 5.3.8 HTTPS (TLS integration already done)
- [ ] 5.3.9 Tests and benchmarks

### 5.4 Promises and reactivity

- [ ] 5.4.1 `Promise[T]` — then, catch, finally, map, flat_map
- [ ] 5.4.2 `Observable[T]` — subscribe, map, filter, merge, zip
- [ ] 5.4.3 Operators: debounce, throttle, distinct, buffer
- [ ] 5.4.4 Pipe operator `|>` for fluent composition
- [ ] 5.4.5 Tests

**Gate**: TCP echo server runs, async/await compiles and executes, HTTP server serves routes.

---

## Phase 6: Self-Hosting Compiler

**Goal**: Rewrite the C++ compiler in TML
**Priority**: STRATEGIC (long-term)
**Dependencies**: Phases 1-4 must be complete
**Tracking**: [self-hosting-compiler/tasks.md](../rulebook/tasks/self-hosting-compiler/tasks.md)

### The bootstrap path

```
Stage 0: C++ compiler compiles TML code           (TODAY)
Stage 1: C++ compiler compiles TML-compiler-in-TML → tml_stage1.exe
Stage 2: tml_stage1.exe compiles itself            → tml_stage2.exe
Stage 3: tml_stage2.exe compiles itself            → tml_stage3.exe
         Verify: stage2 == stage3 (byte-identical) → SELF-HOSTING ACHIEVED
```

### What gets rewritten (~195K lines C++)

| Component | C++ Lines | Difficulty | Stage |
|-----------|----------:|------------|-------|
| Lexer | 3,183 | Easy | 1 |
| Preprocessor | 1,168 | Easy | 1 |
| Parser | 9,384 | Medium | 1 |
| Type Checker | 20,205 | Very Hard | 2 |
| HIR | 14,295 | Hard | 2 |
| LLVM Codegen | 52,907 | Very Hard | 2 |
| Borrow Checker | 6,579 | Very Hard | 3 |
| MIR + 49 passes | 36,951 | Very Hard | 3 |
| CLI/Builder | 26,975 | Medium | 4 |
| Test Runner | 12,899 | Medium | 4 |
| Query System | 2,736 | Hard | 3 |
| Formatter/Linter | 2,516 | Easy | 4 |
| Documentation | 4,303 | Medium | 4 |

### What stays as FFI (not rewritten)

- LLVM library — accessed via `@extern("c")` to LLVM C API
- LLD linker — accessed via `@extern("c")`
- OS syscalls — accessed via `@extern("c")` to libc
- Crypto libraries — accessed via `@extern("c")` to OpenSSL/BCrypt

### Prerequisites

- [ ] 6.0.1 `migrate-runtime-to-tml` complete (Phase 4)
- [ ] 6.0.2 Closures with variable capture working
- [ ] 6.0.3 Recursive enums working (AST nodes)
- [ ] 6.0.4 `HashMap[Str, T]` with 10K+ entries
- [ ] 6.0.5 File I/O reliable (read source, write object files)
- [ ] 6.0.6 LLVM C API bindings written as `@extern("c")` (~500 functions)
- [ ] 6.0.7 LLD bindings written

### Stage 1: Frontend in TML

- [ ] 6.1.1 Lexer — tokenize TML source (32 keywords, 80+ token kinds)
- [ ] 6.1.2 Preprocessor — `#if`/`#elif`/`#else`/`#endif`, `defined()`, predefined symbols
- [ ] 6.1.3 Parser — Pratt expression parser, declarations, statements, patterns, types
- [ ] 6.1.4 Validation — TML frontend produces identical output to C++ frontend on full test suite

### Stage 2: Middle-end in TML

- [ ] 6.2.1 Type system data structures — `Type` enum, `TypeContext`, `SymbolTable`
- [ ] 6.2.2 Type checker — inference, unification, method resolution, exhaustiveness
- [ ] 6.2.3 HIR lowering — desugar, monomorphize, coercion materialization
- [ ] 6.2.4 LLVM codegen — all instruction types via LLVM C API FFI
- [ ] 6.2.5 Backend integration — verification, optimization, object emission, linking

### Stage 3: Optimizations in TML

- [ ] 6.3.1 MIR construction — HIR to SSA-like CFG
- [ ] 6.3.2 MIR optimization passes (49 passes, ported incrementally)
- [ ] 6.3.3 Borrow checker — lifetime tracking, NLL
- [ ] 6.3.4 Query system — incremental compilation

### Stage 4: Tooling in TML

- [ ] 6.4.1 Full CLI — build, run, check, test, format, lint
- [ ] 6.4.2 Test runner — DLL-based execution, coverage
- [ ] 6.4.3 Formatter and linter

### Stage 5: Bootstrap validation

- [ ] 6.5.1 Bootstrap chain: C++ → Stage1 → Stage2 → Stage3 (byte-identical)
- [ ] 6.5.2 Full test suite passes on self-hosted compiler (8,700+ tests)
- [ ] 6.5.3 Performance within 3x of C++ compiler
- [ ] 6.5.4 C++ compiler relegated to bootstrap-only role

**Gate**: TML compiler compiles itself. Bootstrap chain produces byte-identical output.

---

## Parallel Tracks (independent of main phases)

These can be worked on alongside the main phases without blocking or being blocked.

### Developer Tooling

**Status**: 65% complete
**Tracking**: [developer-tooling/tasks.md](../rulebook/tasks/developer-tooling/tasks.md)

- [x] VSCode extension (published v0.17.0)
- [x] Compiler MCP server (20 tools)
- [x] Code formatter
- [x] Linter
- [ ] LSP: go-to-definition, references, rename
- [ ] `tml doc` — HTML documentation generation
- [ ] Doc comment preservation in compiler pipeline

### Reflection System

**Status**: 48% complete
**Tracking**: [implement-reflection/tasks.md](../rulebook/tasks/implement-reflection/tasks.md)

- [x] Core intrinsics (field_count, variant_count, field_name, etc.)
- [x] TypeInfo generation and @derive(Reflect)
- [x] Any type with downcast
- [ ] OOP reflection (class/method introspection)
- [ ] Integration tests

### Serialization

- [ ] `Serialize` / `Deserialize` generic behaviors
- [ ] `@derive(Serialize, Deserialize)`
- [ ] TOML, YAML, MessagePack, CSV parsers

### Package Manager

- [ ] `tml.toml` manifest format
- [ ] Git-based dependencies
- [ ] Version resolution and lock files
- [ ] Package registry

### Cross-Compilation

- [ ] Target triple support (`--target=x86_64-linux-gnu`)
- [ ] Tier 1: Windows, Linux (x86_64), macOS
- [ ] Tier 2: WebAssembly, Linux ARM64
- [ ] CI/CD cross-compile matrix

---

## Progress Tracking

| Phase | Items | Done | Progress | Status |
|-------|-------|------|----------|--------|
| 1. Codegen bugs | 37 | 36 | 97% | NEARLY COMPLETE |
| 2. Test coverage | 95 | 75 | 79% | **COMPLETE** (75.1%) |
| 3. Stdlib essentials | 42 | 20 | 48% | **IN PROGRESS** |
| 4. Runtime migration | 28 | 0 | 0% | NOT STARTED |
| 5. Async + networking | 27 | 0 | 0% | NOT STARTED |
| 6. Self-hosting | 22 | 0 | 0% | NOT STARTED |
| Parallel: Tooling | 7 | 4 | 57% | IN PROGRESS |
| Parallel: Reflection | 5 | 3 | 60% | IN PROGRESS |
| **TOTAL** | **263** | **138** | **52.5%** | |

---

## References

| Document | Location |
|----------|----------|
| Migrate Runtime to TML | [rulebook/tasks/migrate-runtime-to-tml/](../rulebook/tasks/migrate-runtime-to-tml/) |
| Self-Hosting Compiler | [rulebook/tasks/self-hosting-compiler/](../rulebook/tasks/self-hosting-compiler/) |
| Test Failures (Codegen Bugs) | [rulebook/tasks/test-failures/](../rulebook/tasks/test-failures/) |
| Language Completeness | [rulebook/tasks/language-completeness-roadmap/](../rulebook/tasks/language-completeness-roadmap/) |
| Stdlib Essentials | [rulebook/tasks/stdlib-essentials/](../rulebook/tasks/stdlib-essentials/) |
| Developer Tooling | [rulebook/tasks/developer-tooling/](../rulebook/tasks/developer-tooling/) |
| Reflection System | [rulebook/tasks/implement-reflection/](../rulebook/tasks/implement-reflection/) |
| Expand Core/Std Modules | [rulebook/tasks/expand-core-std-modules/](../rulebook/tasks/expand-core-std-modules/) |
| Test Infrastructure | [rulebook/tasks/improve-test-infrastructure/](../rulebook/tasks/improve-test-infrastructure/) |
