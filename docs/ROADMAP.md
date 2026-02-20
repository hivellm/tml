# TML Roadmap

**Last updated**: 2026-02-20
**Current state**: Compiler functional, 76.2% library coverage, 9,045+ tests passing

---

## Overview

```
Phase 1  [DONE]       Fix codegen bugs (closures, generics, iterators)
Phase 2  [DONE]       Tests for working features → coverage 58% → 76.2% ✓
Phase 3  [DONE 98%]  Standard library essentials (Math✓, Instant✓, HashSet✓, Args✓, Deque✓, Vec✓, SystemTime✓, DateTime✓, Random✓, BTreeMap✓, BTreeSet✓, BufIO✓, Process✓, Regex captures✓, ThreadRng✓)
Phase 4  [IN PROGRESS] Migrate C runtime → pure TML + eliminate hardcoded codegen (List✓, HashMap✓, Buffer✓, Str✓, fmt✓, File/Path/Dir✓, dead code✓, StringBuilder✓, Text✓, Float math→intrinsics✓, Sync/threading→@extern✓, Time→@extern✓, Dead C files deleted✓, Float NaN/Inf→LLVM IR✓, On-demand declares✓, FuncSig cleanup✓, Dead file audit✓, string.c→inline IR✓, math.c→inline IR✓, collections.c cleaned✓, 9 inline IR→TML dispatch✓, search.c→pure TML✓, dead stubs/atomics/pool cleaned✓, glob→@extern FFI✓, dead crypto list builders removed✓, dead codegen paths removed✓, primitive to_string→TML behavior dispatch✓, string interp→TML Display✓; runtime: 15 compiled .c files, 0 migration candidates; inline IR: 12 functions remaining)
Phase 5  [LATER]      Async runtime, networking, HTTP
Phase 6  [DISTANT]    Self-hosting compiler (rewrite C++ → TML)
```

### Why this order

| Phase | Rationale |
|-------|-----------|
| 1. Codegen bugs | Blocks everything else. Without working closures, iterators never work. Without generic enums, `Maybe`/`Outcome` are unusable idiomatically. |
| 2. Test coverage | Proves what works, catches regressions, builds confidence for bigger changes. |
| 3. Stdlib essentials | Makes TML usable for real programs. Collections, math, datetime are table stakes. |
| 4. Runtime migration + codegen cleanup | **Dual scope**: (a) migrate ~5,210 lines of C algorithms to pure TML, (b) eliminate ~3,300 lines of hardcoded type dispatch in the compiler. Both block self-hosting — every hardcoded if/else chain must be rewritten when the compiler moves to TML. |
| 5. Async + networking | Enables servers and networked applications. Depends on stable closures + iterators. |
| 6. Self-hosting | Ultimate goal. Requires everything above to be solid. |

### Current metrics

| Metric | Value |
|--------|-------|
| Library function coverage | 76.2% (3,228/4,235) |
| Tests passing | 9,045+ across 790+ files |
| Modules at 100% coverage | 73 |
| Modules at 0% coverage | 31 |
| C++ compiler size | ~238,000 lines |
| C runtime compiled | 15 files (15 essential FFI, 0 migration candidates) |
| C runtime to migrate | 0 lines (collections.c cleaned Phase 43; search.c deleted Phase 35) |
| Dead C files on disk | 0 (9 deleted in Phases 30-35: text.c, thread.c, async.c, io.c, profile_runtime.c, collections.c dup, string.c, math.c, search.c) |
| Inline IR in runtime.cpp | 12 functions (~280 lines) — Phase 33: -7, Phase 34: -2, Phase 36: -1, Phase 44: -3, Phase 45: -3; 3 black_box must stay |
| Dead builtin handlers removed | Phase 36: 18 string handlers, Phase 38: 13 math handlers, Phase 39: 8 time registrations, Phase 41: 3 dead stubs + declarations |
| Dead C runtime functions removed | Phase 41: 6 i64 atomics from sync.c, 4 pool exports from pool.c |
| Hardcoded codegen dispatch | ~350 lines remaining (of ~3,300 original; collections + File/Path done) |
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
- [ ] ~~1.3.4 Fix async iterator support~~ — **moved to Phase 5** (depends on async runtime)

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
- [x] 1.7.8 Add `__FILE__`, `__DIRNAME__`, `__LINE__` compile-time constants (DONE 2026-02-17 — lexer-level expansion, enables scripts to use paths relative to script location)
- [x] 1.7.9 Fix `Shared[T]` memory leak — `decrement_count`/`increment_count` codegen broken for library-imported generics (DONE 2026-02-17 — rewritten with `ptr_write` intrinsic; root cause: `(*ptr).field = value` produces CallExpr instead of UnaryExpr for library-imported generic types)
- [x] 1.7.10 Fix `when`-expr void result — branches returning Unit now handled correctly (DONE 2026-02-18)
- [x] 1.7.11 Fix enum array size computation — variant size calculation corrected (DONE 2026-02-18)
- [x] 1.7.12 Fix cast unsigned propagation — unsigned casts now propagate correctly through expressions (DONE 2026-02-18)
- [x] 1.7.13 Fix generic struct type/function name mismatch in `tml run`/`tml build` (DONE 2026-02-18)

### 1.8 Nested generic type codegen

- [x] 1.8.1 Fix `%struct.T` — generic struct type param not substituted in nested contexts (DONE 2026-02-14)
- [x] 1.8.4 Fix generic method instantiation for library-internal types — non-public structs (`StackNode[T]`) not found in module search because only `mod.structs` was checked, not `mod.internal_structs`; also `is_library_type` flag was incorrectly set based on `pending_generic_structs_` instead of `pending_generic_impls_` (DONE 2026-02-14 — unblocked 8 disabled test files: lockfree_queue, lockfree_stack_peek, mpsc_channel, sync_mpsc, mpsc_repro_mutex_ptr, mpsc_channel_creation, sync_collections, thread)
- [x] 1.8.2 Fix nested adapter type generation (DONE 2026-02-15 — verified: 5-level deep chains `map->filter->take->enumerate->count`, `skip->take->sum` all work)
- [x] 1.8.3 Fix `FromFn[F]` as adapter input (DONE 2026-02-15 — verified: `from_fn(...).map(...)`, `from_fn(...).filter(...)` chains work correctly)

### 1.9 Performance fix

- [x] 1.9.1 Fix generic cache O(n^2) in test suites — replaced full-map scans with pending queues in `generate_pending_instantiations()` (DONE 2026-02-18; `struct_instantiations_` and `enum_instantiations_` are generated immediately at registration so loop was always scanning already-generated entries; `func_instantiations_` and `class_instantiations_` now use `pending_func_keys_`/`pending_class_keys_` vectors)

**Progress**: 43/43 actionable items fixed (**100%**). Coverage jumped from 43.7% to 76.2% (+2,600+ functions). Only 1.3.4 (async iterator) remains — moved to Phase 5 (depends on async runtime).
**Gate**: Phase 1 COMPLETE. Coverage at 76.2% with 9,010+ tests.

---

## Phase 2: Test Coverage

**Goal**: 58% → 75%+ function coverage — **ACHIEVED** (76.2%)
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
**Note**: DH crypto stack overflow crash **FIXED** (2026-02-20) — root cause was `std::thread` creating test execution thread with default 1 MB Windows stack; OpenSSL DH operations (BIGNUM modular exponentiation, Miller-Rabin primality) intermittently exceeded this. Fix: 8 MB stack via `_beginthreadex` with `STACK_SIZE_PARAM_IS_A_RESERVATION` in `suite_execution.cpp`.
**Gate**: Coverage >= 75% — **ACHIEVED** (76.2%, 3,228/4,235 functions). 73 modules at 100%.

---

## Phase 3: Standard Library Essentials

**Goal**: Make TML usable for real programs
**Priority**: HIGH
**Tracking**: [stdlib-essentials/tasks.md](../rulebook/tasks/stdlib-essentials/tasks.md)

### 3.1 Collections

- [x] 3.1.1 Implement `HashSet[T]` — insert, remove, contains *(re-enabled in `std::collections::class_collections`, also includes ArrayList, Queue, Stack, LinkedList)*
- [x] 3.1.2 Implement `BTreeMap[K, V]` — ordered map with O(log n) operations *(sorted-array backed, I64 specialized, binary search)*
- [x] 3.1.3 Implement `BTreeSet[T]` — ordered set *(wrapper around BTreeMap)*
- [x] 3.1.4 Implement `Deque[T]` — double-ended queue *(ring buffer backed by List[T])*
- [x] 3.1.5 Implement `Vec[T]` alias for `List[T]` (ergonomic) *(push/pop/len/get/set/contains/clear)*
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
- [x] 3.3.3 `env::current_dir()` / `env::set_current_dir()` *(implemented in `std::os` via getcwd/chdir FFI)*
- [x] 3.3.4 `process::exit(code)` — exit with status code *(implemented as `os::process_exit(code)` via `tml_os_exit` FFI)*
- [x] 3.3.5 `process::Command` — spawn subprocesses *(implemented as `os::exec(cmd)` returning stdout, `os::exec_status(cmd)` returning exit code, via `_popen`/`system` FFI)*
- [x] 3.3.6 Tests for env and process *(args, env_get/set/unset, process_exit tests passing)*

### 3.4 DateTime

- [x] 3.4.1 `Instant` — monotonic clock for measuring elapsed time *(implemented in `std::time`: `Instant::now()`, `elapsed()`, `as_nanos()`, `duration_since()`)*
- [x] 3.4.2 `SystemTime` — wall clock time *(implemented in `std::time`: `now()`, `as_secs()`, `subsec_nanos()`, `elapsed()`, `duration_since_epoch()`)*
- [x] 3.4.3 `DateTime` — date + time (UTC only) *(implemented in `std::datetime`: `now()`, `from_timestamp()`, `from_parts()`, component accessors, `weekday()`, `day_of_year()`, `is_leap_year()`)*
- [x] 3.4.4 Formatting: ISO 8601, RFC 2822, custom formats *(implemented: `to_iso8601()`, `to_rfc2822()`, `to_date_string()`, `to_time_string()`, `to_string()`, `debug_string()` + helper functions)*
- [x] 3.4.5 Parsing: string → DateTime *(implemented: `parse_iso8601()`, `parse_date()`, `parse()` with format strings)*
- [x] 3.4.6 Tests for datetime *(Instant tests passing: now, elapsed, sleep)*

### 3.5 Random number generation

- [x] 3.5.1 `Rng` type — `next_i64()`, `next_bool()` *(xoshiro256** PRNG, SplitMix64 seed expansion)*
- [x] 3.5.2 `ThreadRng` — per-thread PRNG with high-entropy seeding *(wraps xoshiro256** with SplitMix64-mixed nanosecond clock seed; 4 tests passing)*
- [x] 3.5.3 `random[T]()` — convenience functions *(implemented: `random_i64()`, `random_f64()`, `random_bool()`, `random_range(min, max)`)*
- [x] 3.5.4 `rng.range(min, max)` — random integer in range *(implemented)*
- [x] 3.5.5 `rng.shuffle(list)` — Fisher-Yates shuffle *(implemented: `shuffle_i64(List[I64])`, `shuffle_i32(List[I32])`, `next_f64()`, `range_f64()`)*
- [x] 3.5.6 Tests for random *(9 tests: Rng 5 + ThreadRng 4)*

### 3.6 Buffered I/O

- [x] 3.6.1 `BufReader` — buffered reader wrapping `File` *(implemented: `open()`, `from_file()`, `read_line()`, `read_all()`, `is_eof()`, `lines_read()`)*
- [x] 3.6.2 `BufWriter` — buffered writer wrapping `File` *(implemented: `open()`, `write()`, `write_line()`, `flush()`, auto-flush at 8KB capacity, `buffered()`, `total_written()`)*
- [ ] 3.6.3 `Read` / `Write` / `Seek` behaviors — **COMPILER-BLOCKED** (default behavior method dispatch returns `()` instead of expected type; same bug as 1.3.2/2.1.2)
- [x] 3.6.4 `LineWriter` — flush on newline *(implemented: `write()` flushes after last newline, `write_line()`, `flush()`, `close()`)*
- [x] 3.6.5 Tests for buffered I/O *(9 tests: 3 BufReader, 3 BufWriter, 3 LineWriter — all passing)*

### 3.7 Error context chains ✓

- [x] 3.7.1 `Context` behavior — `.context("msg")` on errors *(implemented in `core::error`: `.context()`, `.with_context()`)*
- [x] 3.7.2 `Error` source chain — `.source()` for error chaining *(implemented: `Error` behavior with `source()`, `ChainedError[E]`, `ErrorChain` iterator)*
- [x] 3.7.3 `AnyError` type — anyhow-style generic errors *(implemented: `BoxedError`, `SimpleError` for type-erased errors)*
- [x] 3.7.4 Display with backtrace and chain *(implemented: `error_chain()` function, `ErrorChain` iterator)*
- [x] 3.7.5 Tests for error chains *(tested in `core/tests/coverage/quick_wins_75e.test.tml`)*

### 3.8 Regex engine

- [x] 3.8.1 `Regex` type — compile pattern (Thompson's NFA via shunting-yard postfix)
- [x] 3.8.2 `is_match()`, `find()`, `find_all()`
- [x] 3.8.3 `captures()` — positional capture groups *(Thompson's NFA with per-path capture tracking, iterative epsilon closure, 8 tests passing)*
- [x] 3.8.4 `replace()`, `replace_all()`, `split()`
- [x] 3.8.5 Character classes (`[a-z]`, `[^0-9]`), quantifiers (`*`, `+`, `?`), shorthand (`\d`, `\w`, `\s`)
- [x] 3.8.6 NFA engine (Thompson's simulation — no exponential backtracking)
- [x] 3.8.7 Tests for regex (30 tests: basic + advanced + captures)

**Progress**: 47/48 items complete (~98%). Only 3.6.3 (Read/Write/Seek behaviors) remains, blocked by compiler bug (default behavior method dispatch returns `()`).\
**Recent improvements** (2026-02-18): Added `destroy()` methods to all collection types (List, HashMap, Buffer, Deque, BTreeMap, BTreeSet, HashSet, LinkedList) fixing memory leaks; TML-style Hash behavior impls for net types (IpAddr, SocketAddr, etc.); DNS runtime fix for dynamic TLS and family_hint values.
**Gate**: `HashSet`, `BTreeMap`, `Math`, `DateTime`, `Random`, `ThreadRng`, `BufReader/BufWriter`, `Regex` (with captures), `Process` all working with tests.

---

## Phase 4: Migrate C Runtime to Pure TML + Eliminate Hardcoded Codegen

**Goal**: Eliminate all non-essential C/C++ runtime code, replacing with pure TML (`lowlevel` + `core::intrinsics`) or `@extern("c")` FFI wherever possible
**Priority**: MEDIUM — architectural cleanup, prerequisite for self-hosting
**Tracking**: [migrate-runtime-to-tml/tasks.md](../rulebook/tasks/migrate-runtime-to-tml/tasks.md)

**Migration hierarchy** (always prefer Tier 1):
1. **Pure TML** — `lowlevel` blocks + `core::intrinsics` (ptr_read/write/offset, llvm_add/sub/mul/and/or/shl/shr, mem_alloc, copy_nonoverlapping, cast, transmute)
2. **TML + @extern("c") FFI** — for libc/OS functions (snprintf, nextafterf, opendir)
3. **Inline LLVM IR** — only when TML has no equivalent (asm sideeffect for black_box)

### Progress (2026-02-20)

**Collections migration COMPLETE**: List, HashMap, Buffer all migrated to pure TML. Eliminated ~3,300 lines of C runtime AND ~2,800 lines of hardcoded compiler dispatch.

**String migration COMPLETE (99.3%)**: All C runtime string calls in `str.tml` replaced with pure TML. Only `as_bytes` remains (blocked on slice type codegen).

**File/Path/Dir migration COMPLETE**: Rewritten as TML structs with `@extern("c")` FFI.

**Integer formatting migration COMPLETE**: Phase 7 rewrote 16 integer Display/Debug lowlevel blocks to pure TML.

**Phase 16 dead code cleanup COMPLETE**: Removed 28 dead `functions_[]` map entries from `runtime.cpp`.

### Comprehensive runtime.cpp Audit (2026-02-18)

Full audit of all ~287 `declare` statements in `runtime.cpp` cross-referenced against compiler codegen C++ files and TML lowlevel blocks. Key findings:

| Category | Declares | Decision |
|----------|----------|----------|
| LLVM intrinsics + C stdlib + essential I/O + memory | ~68 | **KEEP** — fundamental, can never be removed |
| Dead declarations (never called anywhere) | 15 | **REMOVE** — Phase 17 (4 SIMD + 6 atomic + 2 print + 3 float/math) |
| Char classification (14 functions) | 14 | **MIGRATE** — already pure TML in char/methods.tml, codegen still emits C calls |
| Char-to-string / UTF-8 (4 functions) | 4 | **MIGRATE** — implement in pure TML |
| String ops (34 str_* functions) | 34 | **MIGRATE** — str.tml has pure TML impls, codegen still emits C calls in method_primitive_ext.cpp |
| StringBuilder (9 functions) | 9 | **MIGRATE** — codegen-only, no TML usage |
| Text type (51 tml_text_* functions) | 51 | **MIGRATE** — only called from TML lowlevel blocks, rewrite as TML struct |
| Float math (24 functions) | 24 | **MIGRATE** — ~16 replaceable with LLVM intrinsics, ~8 keep (snprintf, nextafter) |
| Threading/sync/channel (32 functions) | 32 | **MIGRATE** — TML already has @extern, codegen still hardcodes |
| Time/pool (20 functions) | 20 | **MIGRATE** — move to @extern FFI |
| Log/glob (17 functions) | 17 | **DONE** — Log: I/O, keep; Glob: Phase 42 migrated 5 declares to @extern FFI in glob.tml |

**Target**: Reduce `runtime.cpp` from ~287 declares to ~68 essential declares.

### Architecture Target

**Strategy**: Three-tier migration hierarchy, always preferring pure TML:

```
Tier 1 — PURE TML (lowlevel + intrinsics)     Best: self-hosting ready, no C dependency
  Uses: ptr_read/ptr_write/ptr_offset, llvm_add/sub/mul/div/rem,
        llvm_and/or/xor/shl/shr, mem_alloc/mem_free, copy_nonoverlapping,
        cast[T,U], transmute, size_of[T], sqrt/sin/cos/floor/ceil/round/fabs

Tier 2 — TML + @extern("c") FFI               Good: TML logic + external library calls
  Uses: @extern("c") func snprintf(...), @extern("c") func nextafterf(...),
        @extern("c") func opendir(...), etc.

Tier 3 — Inline IR in runtime.cpp              Last resort: asm sideeffect, bootstrap code
  Uses: define internal ... { asm sideeffect }  (no TML equivalent)
```

```
KEEP FOREVER (essential C runtime):             REMAINING INLINE IR (12 functions):
  - LLVM intrinsics (7)                            str_eq, str_concat_opt (deeply embedded, 30+ callsites)
  - C stdlib (printf, malloc, free) (5)            f64/f32_to_string (snprintf, TML lowlevel)
  - Essential runtime (panic, print) (4)           f64/f32_to_string_precision (snprintf, TML lowlevel)
  - Memory (mem_alloc, mem_free, etc.) (10)        f64/f32_to_exp_string (snprintf, TML lowlevel)
  - Coverage/debug (conditional)                   str_as_bytes (TML lowlevel in str.tml)
  - Panic catching + backtrace (4)
  - Format string constants                      KEEP AS INLINE IR (no TML equivalent):
  - Log runtime (12, I/O)                          black_box_i32/i64/f64 (asm sideeffect)
  - tml_random_seed (1, OS random)

REMAINING LIVE BUILTIN HANDLERS (7):            REMOVED (Phase 44-45):
  - sqrt, pow (LLVM intrinsics)                    i32/i64_to_string, bool_to_string (→ TML Display)
  - black_box, black_box_i64, black_box_f64       i64_to_str, f64_to_str, float_to_string (→ TML Display)
  - infinity, nan, is_inf, is_nan (LLVM const)
                                                 REMAINING C FILE CANDIDATES: none
                                                 ESSENTIAL FFI (must stay C):
ALREADY DONE:                                      essential.c, mem.c, pool.c, time.c, sync.c, os.c,
  - Collections (List/HashMap/Buffer) ✓             net.c, dns.c, tls.c, backtrace.c, log.c,
  - String algorithms (str.tml) ✓                  crypto*.c (7 files), file.c, glob.c
  - Integer formatting ✓
  - File/Path/Dir ✓
  - Dead functions_[] entries ✓
  - Char classification → pure TML ✓
  - StringBuilder → removed ✓
  - Text type → pure TML struct ✓
  - Float math → LLVM intrinsics ✓
  - Threading/sync → @extern FFI ✓
  - Time → @extern FFI ✓
  - string.c → inline IR (Phase 31) ✓
  - math.c → inline IR (Phase 32) ✓
  - 9 inline IR → TML dispatch (Phase 33-34) ✓
  - search.c → pure TML (Phase 35) ✓
  - 31 dead builtin handlers removed (Phase 36-38) ✓
  - 4 dead C functions from essential.c (Phase 37) ✓
  - 10 dead type registrations removed (Phase 38-39) ✓
  - 3 dead stub files + declarations (Phase 40-41) ✓
  - 6 dead i64 atomics + 4 dead pool exports (Phase 41) ✓
  - glob.tml lowlevel → @extern FFI (Phase 42) ✓
```

### 4.1 Collections — List[T] (DONE)

- [x] 4.1.1 Rewrite `List[T]` as pure TML using `ptr_read`/`ptr_write`/`mem_alloc` — 214 lines in `lib/std/src/collections/list.tml`
- [x] 4.1.2 Cleanup: remove dead `list_*` references from compiler
- [ ] 4.1.3 Cleanup: remove `List` type-erasure from `decl/struct.cpp:317-335`

### 4.2 Collections — HashMap[K, V] (DONE)

- [x] 4.2.1 Rewrite `HashMap[K, V]` as pure TML (open-addressing, linear probing, FNV-1a hashing)
- [x] 4.2.2 Rewrite `HashMapIter` as TML struct (no opaque handle)
- [x] 4.2.3 Remove 14 hashmap_* function registrations from `types/builtins/collections.cpp`
- [x] 4.2.4 Remove HashMap dispatch from `method_collection.cpp` (~200 lines)
- [x] 4.2.5 Remove HashMap from bypass lists (`method_impl.cpp`, `decl/impl.cpp`, `generate.cpp`)
- [x] 4.2.6 Remove HashMap static methods from `method_static.cpp` (~60 lines)
- [x] 4.2.7 Remove HashMap type-erasure from `decl/struct.cpp:337-354`
- [x] 4.2.8 All existing HashMap tests pass through normal dispatch (11 tests)

### 4.3 Collections — Buffer (DONE)

- [x] 4.3.1 Rewrite `Buffer` as pure TML (`data: *U8, len: I64, capacity: I64, read_pos: I64`)
- [x] 4.3.2 Remove 11 buffer_* function registrations from `types/builtins/collections.cpp`
- [x] 4.3.3 Remove Buffer dispatch from `method_collection.cpp` (~973 lines — largest single elimination)
- [x] 4.3.4 Remove Buffer from bypass lists (`method_impl.cpp`, `decl/impl.cpp`, `generate.cpp`)
- [x] 4.3.5 Remove Buffer static methods from `method_static.cpp` (~76 lines)
- [x] 4.3.6 All existing Buffer tests pass (31+ tests across 8 files)

### 4.4 File/Path/Dir — Refactor to TML + @extern("c") FFI (DONE)

- [x] 4.4.1 Rewrite `lib/std/src/file.tml` with TML struct + `@extern("c")` FFI to OS file ops
- [x] 4.4.2 Rewrite `lib/std/src/path.tml` with TML struct + `@extern("c")` FFI to OS path ops
- [x] 4.4.3 Migrate `lib/std/src/file/dir.tml` from lowlevel blocks to @extern FFI
- [x] 4.4.4 Remove File/Path from bypass lists (5 compiler files: method_impl.cpp, decl/impl.cpp, decl/struct.cpp, generate.cpp, method_static_dispatch.cpp)
- [x] 4.4.5 Remove 23 hardcoded static methods from `method_static.cpp`
- [x] 4.4.6 Remove File/Path struct type declarations from `runtime.cpp`
- [x] 4.4.7 Remove dead network socket _raw declarations from `runtime.cpp` (~43 lines)
- [x] 4.4.8 Remove dead TLS/SSL declarations from `runtime.cpp` (~68 lines)
- [x] 4.4.9 All 36 File/Path/Dir tests pass

### 4.5 Strings (core::str) — DONE (99.3%)

- [x] 4.5.1 Rewrite read-only ops in pure TML (len, char_at, contains, starts_with, ends_with, find, rfind)
- [x] 4.5.2 Rewrite transforms in pure TML (to_uppercase, to_lowercase, trim, trim_start, trim_end)
- [x] 4.5.3 Rewrite splitting (split, split_whitespace, lines) and allocating (substring, replace, repeat, join, chars)
- [x] 4.5.4 Rewrite parsing (parse_i32, parse_i64, parse_f64)
- [ ] 4.5.5 `str_as_bytes` — blocked on `ref [U8]` slice type codegen

### 4.6 Formatting (core::fmt) — PARTIALLY DONE

- [x] 4.6.1 Integer formatting — pure TML digit extraction + string concat (Phase 7)
- [x] 4.6.2 Hex/octal/binary — already pure TML, no lowlevel blocks (Phase 10)

### 4.7 Dead code cleanup — DONE

- [x] 4.7.1 Removed 28 dead `functions_[]` entries from `runtime.cpp` (Phase 16)

### 4.8 Remove dead declares (Phase 17) — DONE

- [x] 4.8.1 Remove 15 dead `declare` statements (4 unused SIMD, 6 dead atomic_counter, 2 dead print (f32/char), 3 dead float/math) — 9,025 tests pass

### 4.9 Char classification → pure TML dispatch (Phase 18.1) — DONE

- [x] 4.9.1 Remove 14 char_* builtin emitters from `builtins/string.cpp` and 14 char_* declares from `runtime.cpp` — TML char/methods.tml handles all classification/conversion via module-qualified dispatch
- [x] 4.9.2 Rewrote `compiler/tests/runtime/char.test.tml` to use module-qualified calls — 23 tests pass
- [x] 4.9.3 Implement `char_to_string`, `utf8_*byte_to_string` in pure TML using mem_alloc + ptr_write (Phase 18.2 — done, 4 declares + 4 FuncSigs removed)

### 4.10 Str codegen dispatch → TML dispatch (Phase 20) — DONE

- [x] 4.10.1 Remove ~21 hardcoded `@str_*` calls from `method_primitive_ext.cpp` — dispatch through TML impl
- [x] 4.10.2 Remove `@str_concat_opt` / `@str_eq` from `binary_ops.cpp`
- [x] 4.10.3 Remove `@i32_to_string` / `@i64_to_string` / `@bool_to_string` from `method_primitive.cpp`
- [x] 4.10.4 Update derive/*.cpp to dispatch through TML impls
- [x] 4.10.5 Migrate lowlevel `str_len`/`str_slice`/`str_hash`/`str_char_at`/`str_substring` in TML lib files to pure TML method calls
- [x] 4.10.6 Fix suite mode lazy library defs regression — `generated_functions_` marking deferred functions as generated in `impl.cpp`
- [x] 4.10.7 Fix primitive type `is_imported` detection in `method_impl.cpp` for suite mode

### 4.11 StringBuilder + dead collections cleanup (Phase 21) — DONE

- [x] 4.11.1 Remove 9 strbuilder_* emitters/declares/FuncSigs (zero TML usage — codegen-only dead code)
- [x] 4.11.2 Delete dead `builtins/collections.cpp` (nullopt stub) and `types/builtins/collections.cpp` (empty init)
- [x] 4.11.3 Remove `init_builtin_collections()` from register.cpp and env.hpp

### 4.12 Text type → TML struct (Phase 22) — DONE

- [x] 4.12.1 Rewrite Text as TML struct (pure TML using mem_alloc/ptr_read/ptr_write, 24-byte header)
- [x] 4.12.2 Implement all 48 text operations in pure TML (index_of, contains, replace, trim, pad, etc.)
- [x] 4.12.3 Update call_user.cpp + core.cpp template literal codegen to use TML dispatch
- [x] 4.12.4 Remove 51 tml_text_* declares + 48 functions_[] entries from runtime.cpp
- [x] 4.12.5 Remove ~800 lines of V8-style MIR Text optimizations from instructions_method.cpp
- [x] 4.12.6 Remove ~290 lines of V8-style AST Text optimizations from call_user.cpp
- [x] 4.12.7 Remove emit_inline_int_to_string + digit_pairs from MIR codegen
- [x] 4.12.8 Register f64_to_str/print_str/println_str in functions_[] map for text.tml lowlevel blocks

### 4.13 Float math → LLVM intrinsics (Phase 23) — DONE

- [x] 4.13.1 Replace `@float_abs` → `@llvm.fabs.f64`, `@float_sqrt` → `@llvm.sqrt.f64`, `@float_pow` → `@llvm.pow.f64`, `@float_round/floor/ceil` → `@llvm.round/floor/ceil.f64` + fptosi, `@int_to_float` → `sitofp`, `@float_to_int` → `fptosi`
- [x] 4.13.2 Replace bit casts with LLVM `bitcast` instructions (float32_bits, float32_from_bits, float64_bits, float64_from_bits)
- [x] 4.13.3 Replace NaN/infinity with LLVM `fcmp uno`/`fcmp oeq` + hex constants (`0x7FF8000000000000`, `0x7FF0000000000000`, `0xFFF0000000000000`)
- [x] 4.13.4 Keep: float-to-string (snprintf), nextafter (no intrinsic), f64/f32_is_nan/is_infinite (fmt lowlevel)
- [x] 4.13.5 Remove 16 float declares from runtime.cpp + update method_primitive.cpp `.pow()` to use `@llvm.pow.f64`

### 4.14 Sync/threading → @extern FFI (Phase 24) — DONE

- [x] 4.14.1 Remove thread/channel/mutex/waitgroup codegen from builtins/sync.cpp (keep spinlock)
- [x] 4.14.2 Remove typed atomic codegen from builtins/atomic.cpp (keep generic atomics)
- [x] 4.14.3 Remove FuncSig entries from types/builtins/sync.cpp and atomic.cpp
- [x] 4.14.4 Remove 23 declares from runtime.cpp (5 thread + 8 channel + 5 mutex + 5 waitgroup)
- [x] 4.14.5 Add typed atomic @extern declarations to core::sync.tml module
- [x] 4.14.6 Verify all sync/thread/alloc tests pass

### 4.14b String.c dead code removal — DONE

- [x] 4.14b.1 Remove 18 dead declares from runtime.cpp (str_concat/3/4, str_trim_start/end, str_find/rfind, str_parse_*, str_replace/first, str_split/whitespace, str_lines/chars, str_repeat, str_join)
- [x] 4.14b.2 Remove ~720 lines of dead C functions from string.c (str_split, str_chars, str_lines, str_join, str_find, str_rfind, str_replace, str_replace_first, str_repeat, str_parse_*, str_trim_start/end, str_concat legacy/3/4, char_is_* x8, char_to_* x6, strbuilder_* x9, static buffers)
- [x] 4.14b.3 String.c reduced from 1,202 lines to ~490 lines (only active functions remain)
- [x] 4.14b.4 Broke string.c→collections.c dependency (no more list_* calls from string.c)
- [x] 4.14b.5 All tests pass: str (241), fmt (404), crypto (476), sync (699), thread (38)

### 4.15 Time builtins → @extern FFI (Phase 25) — DONE

- [x] 4.15.1 Remove 8 hardcoded time builtins from codegen (time_ms, time_us, elapsed_ms, elapsed_us, sleep_us, time_ns kept as @extern)
- [x] 4.15.2 Remove 8 time declares from runtime.cpp
- [x] 4.15.3 Keep sleep_ms + time_ns as @extern("c") in std::time
- [x] 4.15.4 Migrate test files from dead time_ms/time_us builtins to std::time::Instant

### 4.16 Dead C file removal (Phase 26) — DONE

- [x] 4.16.1 Remove text.c from CMake build (already pure TML since Phase 22)
- [x] 4.16.2 Remove thread.c from CMake build (already @extern since Phase 24)
- [x] 4.16.3 Remove async.c from CMake build (already @extern since Phase 24)
- [x] 4.16.4 Remove dead strbuilder.test.tml (strbuilder completely removed)

### 4.17 Float NaN/Infinity → LLVM IR (Phase 27) — DONE

- [x] 4.17.1 Replace f32/f64_is_nan C runtime calls with `fcmp uno` LLVM IR in string.cpp
- [x] 4.17.2 Replace f32/f64_is_infinite C runtime calls with `fabs + fcmp oeq` LLVM IR in string.cpp
- [x] 4.17.3 Rewrite fmt/float.tml is_nan/is_infinite as pure TML (NaN: `value != value`, Inf: `value == value and diff != diff`)
- [x] 4.17.4 Remove 16 dead math functions from math.c (412→236 lines, -43%)
- [x] 4.17.5 Remove f64_is_nan/f64_is_infinite from essential.c and runtime.cpp

### 4.18 On-demand declaration emit (Phase 28) — DONE

- [x] 4.18.1 Remove 7 dead declares from runtime.cpp (287→122 total)
- [x] 4.18.2 Clean up unused runtime function references
- [x] 4.18.3 Import-based conditional emission for atomic ops (9 declares, guard: std::sync/std::thread)
- [x] 4.18.4 Import-based conditional emission for logging runtime (11 declares + 12 functions_[], guard: std::log)
- [x] 4.18.5 Import-based conditional emission for glob utilities (5 declares + 5 functions_[], guard: std::fs::glob)
- [x] 4.18.6 Conservative mode for library_ir_only (emit all declares for suite compatibility)

### 4.19 Type system + metadata cleanup (Phase 29) — DONE (partial)

- [x] 4.19.1 Remove 29 string FuncSig registrations from `types/builtins/string.cpp`
  - All 29 entries (12 string + 14 char + 3 removed) were dead code — no type checker queries
  - String ops go through `try_gen_builtin_string()` inline, char ops migrated to pure TML
  - `init_builtin_string()` call removed from `register.cpp`; function body is now no-op
  - Migrated 7 test files from bare `str_len()`/`str_eq()`/`str_hash()` builtins to method calls
    (`.len()`, `==`/`!=`, `.hash()`, `+`, `.slice()`, `.contains()`, `.starts_with()`, etc.)
- [ ] 4.19.2 Fix metadata loader to preserve behavior method return types (DEFERRED)
  - Binary format (`.tml.bin`) handles behaviors correctly
  - JSON format (`.tml.meta`) missing behavior serialization — needs serialize/deserialize code
  - Lower priority: binary format is the primary code path
- [ ] 4.19.3 Remove hardcoded `Ordering` enum from `types/builtins/types.cpp` (DEFERRED)
  - Only 7 lines; TML definition in `core::cmp.tml` overwrites it at module load time
  - Requires architectural changes to module loading order to remove safely
  - Codegen `enum_variants_` initialization depends on early availability

### 4.20 Final cleanup + validation (Phase 30) — DONE

- [x] 4.20.1 Delete 6 dead C files from disk (2,661 lines removed)
  - `text/text.c` (1,059 lines) — Text migrated to pure TML (Phase 22)
  - `concurrency/thread.c` (519 lines) — replaced by sync.c (Phase 24)
  - `concurrency/async.c` (952 lines) — dead executor (Phase 26)
  - `core/io.c` (67 lines) — superseded by essential.c
  - `core/profile_runtime.c` (54 lines) — unused profiling stub
  - `lib/std/runtime/collections.c` (10 lines) — orphaned, explicitly excluded in helpers.cpp
  - Cleaned 3 dead compile blocks from `helpers.cpp` (async.c, text.c, thread.c fallback paths)
  - Updated CMakeLists.txt comments (removed → deleted)
- [x] 4.20.2 C runtime inventory audit (18 compiled .c files in tml_runtime.lib)
  - **Essential FFI (14 files, must stay as C)**: essential.c, mem.c, pool.c, sync.c, net.c, dns.c, tls.c, os.c, crypto.c, crypto_key.c, crypto_x509.c, backtrace.c, log.c, time.c
  - **Migration candidates (1 file)**: collections.c (~70 lines, list_get/list_len legacy for crypto)
  - **Migrated (Phases 31-35)**: string.c deleted (Phase 31), math.c deleted (Phase 32), search.c deleted (Phase 35, → pure TML in distance.tml), collections.c cleaned (160→70 lines)
  - **Module-conditional (2 files in lib/std/runtime/)**: file.c (FFI, keep), glob.c (FFI via @extern, Phase 42)
  - **Uncompiled crypto extensions (5 files, future work)**: crypto_dh.c, crypto_ecdh.c, crypto_kdf.c, crypto_rsa.c, crypto_sign.c
- [ ] 4.20.3 Benchmark: TML implementations within 10% of C performance (DEFERRED — needs benchmark infrastructure)

### 4.21 Migrate string.c to inline LLVM IR (Phase 31) — DONE

Replaced all C runtime string functions with inline LLVM IR `define` blocks in `runtime.cpp`, using `internal` linkage to avoid COFF duplicate symbol issues. Deleted `string.c` (516 lines) and cleaned `collections.c` (160→70 lines).

**Strategy used:** Instead of rewriting callsites to use TML method dispatch (which would require ~50 changes across 15 files), we replaced the C `declare` statements with `define internal` functions that use libc primitives (`strcmp`, `strlen`, `malloc`, `memcpy`, `snprintf`). Same function signatures = zero callsite changes needed.

- [x] 4.21.1-4 Replace `@str_eq` with inline LLVM IR using `@strcmp` (null-safe, internal linkage)
- [x] 4.21.5-8 Replace `@str_concat_opt` with inline LLVM IR using `strlen+malloc+memcpy`
- [x] 4.21.9 Replace `@f64_to_str` with inline LLVM IR using `@snprintf` with `%g`
- [x] 4.21.9b Replace `@i64_to_str`, `@i32_to_string`, `@i64_to_string`, `@bool_to_string` with inline IR
- [x] 4.21.9c Replace `@str_hash` with inline FNV-1a LLVM IR
- [x] 4.21.9d Replace `@str_as_bytes` with inline null-safe identity IR
- [x] 4.21.10 Remove 13 dead `try_gen_builtin_string()` entries (str_len, str_hash, str_eq, str_concat, str_substring, str_slice, str_contains, str_starts_with, str_ends_with, str_to_upper, str_to_lower, str_trim, str_char_at)
- [x] 4.21.11 Remove `@str_*` declares from `runtime.cpp` (replaced by `define internal`)
- [x] 4.21.12 Delete `compiler/runtime/text/string.c` via `git rm` (516 lines)
- [x] 4.21.13 Remove 12 unused list_* functions from `collections.c` (keep buffer_destroy, buffer_len, list_get, list_len)
- [x] 4.21.14 Fix bare `str_len()` calls in `test/assertions/mod.tml` → `s.len()` (pre-existing bug)
- [x] 4.21.15 Fix bare `str_eq()` calls in `core/cache.tml` → `==` operator (pre-existing bug)

### 4.22 Migrate math.c to inline LLVM IR (Phase 32) — DONE

Replaced all 20 C functions in `math.c` with `define internal` inline IR blocks in `runtime.cpp`, using the same pattern as Phase 31 (string.c). Deleted `math.c` (279 lines).

- [x] 4.22.1 Replace black_box_i32/i64/f64 with inline asm sideeffect barrier
- [x] 4.22.2 Replace simd_sum_i32/f64 and simd_dot_f64 with loop-based IR (phi nodes)
- [x] 4.22.3 Replace float_to_fixed/float_to_string/f64_to_string with snprintf-based IR
- [x] 4.22.4 Replace f32/f64_to_string_precision and f32/f64_to_exp_string with IR
- [x] 4.22.5 Replace i64_to_binary/octal/lower_hex/upper_hex_str with bit-manipulation loop IR
- [x] 4.22.6 Replace nextafter32 with direct call to nextafterf from libm
- [x] 4.22.7 Delete math.c, remove from CMakeLists.txt and helpers.cpp
- [x] 4.22.8 All tests pass (9,045+)

### 4.23 Remove hardcoded inline IR dispatch (Phase 33) — DONE

**Goal**: Remove inline IR functions that already have pure TML implementations, switching to TML behavior dispatch instead.

Phase 33 removed 7 inline IR functions by dispatching through TML behavior impls:
- 3 SIMD functions (`simd_sum_i32`, `simd_sum_f64`, `simd_dot_f64`) — dispatched to `std::search::distance` pure TML
- 4 hex/binary/octal formatting (`i64_to_binary_str`, `i64_to_octal_str`, `i64_to_lower_hex_str`, `i64_to_upper_hex_str`) — dispatched to `core::fmt::impls` TML behaviors

**Key fix**: Added `core::fmt::impls` and `core::fmt::helpers` to 3 essential module lists in `runtime_modules.cpp` to enable proper behavior dispatch for primitive formatting methods.

- [x] 4.23.1-3 Remove SIMD sum/dot inline IR — TML behavior dispatch
- [x] 4.23.4-7 Remove hex/binary/octal inline IR — TML fmt behavior dispatch
- [x] 4.23.8 Remove `str_hash` inline IR — dead code, pure TML in `core::hash` (Phase 34)
- [x] 4.23.18 Remove `nextafter32` inline IR + C++ handler — dead code, no callers (Phase 34)

**Remaining 19 inline IR functions** (cannot easily migrate yet):
- `str_eq` (7+ C++ callsites), `str_concat_opt` (25+ C++ callsites) — deeply embedded in compiler
- `i64_to_str`, `i32_to_string`, `i64_to_string`, `bool_to_string` — string interpolation
- `f64_to_str` — essential print formatting, chicken-and-egg with runtime init
- `float_to_fixed`, `float_to_string`, `f64/f32_to_string`, `f64/f32_to_string_precision`, `f64/f32_to_exp_string` — snprintf-based, 8 functions
- `str_as_bytes` — TML lowlevel in str.tml
- `black_box_i32/i64/f64` — require `asm sideeffect`, must stay as inline IR

### 4.24 Migrate search.c to pure TML (Phase 35) — DONE

Rewrote all 11 vector distance/similarity functions in `lib/std/src/search/distance.tml` as pure TML using `ptr_read`/`ptr_write`/`ptr_offset` + `intrinsics::sqrt[T]`. Deleted `search.c` (98 lines) and removed 6 F32 distance wrappers from `search_engine.cpp`.

- [x] 4.24.1 Rewrite `dot_product`, `cosine_similarity`, `euclidean_distance`, `norm`, `normalize` (F64) as pure TML
- [x] 4.24.2 Rewrite `dot_product_f32`, `cosine_similarity_f32`, `euclidean_distance_f32`, `l2_squared_f32`, `norm_f32`, `normalize_f32` (F32) as pure TML
- [x] 4.24.3 Delete `search.c`, remove from CMakeLists.txt
- [x] 4.24.4 Remove F32 distance wrappers from `search_engine.cpp`

### 4.25 Migrate remaining C files (Phase 36+) — PLANNED

- [x] 4.25.1 Clean `collections.c` (Phase 43) — removed dead list_create/list_push, dead crypto list builders (crypto_get_hashes/ciphers/curves), dead codegen @list_get/@list_len paths, 26 dead C++ unit tests; kept buffer_destroy/buffer_len (zlib @extern) + list_get/list_len (crypto x509)
- [x] 4.25.2 Migrate `glob.tml` from lowlevel to @extern FFI (Phase 42) — 5 declares removed from runtime.cpp, glob.c stays as essential FFI

### Expected impact

| Metric | Before | Current | Target | Notes |
|--------|--------|---------|--------|-------|
| runtime.cpp declares | 393 | 87 (62 effective) | ~55 | -271 via Phases 17-27; 25 on-demand (Phase 28); 16 declares→defines (Phase 31); 19 declares→defines (Phase 32) |
| runtime.cpp inline IR | 0 | 12 functions (~280 lines) | 3 | Phase 31: +9 string, Phase 32: +19 math; Phase 33: -7, Phase 34: -2, Phase 36: -1, Phase 44: -3, Phase 45: -3; keep 3 black_box |
| C runtime (compiled) | 20 files | 15 files | 14 | 9 files deleted (Phases 30-35); collections.c cleaned (Phase 43), no migration candidates |
| C runtime (on disk) | 29 files | 18 files | 14 | 11 dead/migrated files deleted; 5 uncompiled crypto extensions kept |
| Dead C on disk | ~4,450 lines | 0 lines | 0 | All dead code deleted ✓ |
| Hardcoded codegen dispatch | 3,300 lines | ~100 lines | ~50 | Phases 36-38 removed 31 dead builtin handlers; Phase 44 removed primitive to_string dispatch; Phase 45 removed string interpolation dispatch (-50 lines) |
| Types bypassing impl dispatch | 5 | 0 ✓ | 0 | |
| Hardcoded type registrations | 54 | 0 ✓ | 0 | Phase 29: -29 string, Phase 38: -2 math, Phase 39: -8 time |
| Dead C functions in essential.c | ~20 | 0 ✓ | 0 | Phase 37: -4 (print_f32, print_char, float_to_precision, float_to_exp) |

**Progress**: Phases 0-7, 16-44 complete (29-30 partial).
- Phase 31: replaced 9 C string functions with inline IR, deleted string.c (516 lines)
- Phase 32: replaced 20 C math functions with inline IR, deleted math.c (279 lines)
- Phase 33: removed 7 inline IR functions (SIMD+fmt), switched to TML behavior dispatch
- Phase 34: removed 2 dead inline IR functions (str_hash, nextafter32)
- Phase 35: migrated search.c (98 lines) to pure TML, deleted search.c
- Phase 36: removed 18 dead string builtin handlers + 3 dead declares + 1 dead inline IR (float_to_fixed)
- Phase 37: removed 4 dead C functions from essential.c (print_f32, print_char, float_to_precision, float_to_exp)
- Phase 38: removed 13 dead math builtin handlers + nextafter declare + 2 type registrations
- Phase 39: removed 8 dead time type registrations
- Phase 40: removed dead init_builtin_time() call from register.cpp
- Phase 41: removed 3 dead stub files from build, 6 dead i64 atomic functions from sync.c, 4 dead pool exports from pool.c, header declarations cleaned
- Phase 42: migrated glob.tml from lowlevel blocks to @extern FFI, removed 5 glob declares + needs_glob detection from runtime.cpp
- Fix: increased test execution thread stack from 1 MB (default) to 8 MB via NativeThread wrapper, fixing intermittent OpenSSL DH stack overflow crashes
- Phase 43: removed dead list_create/list_push from collections.c, crypto_get_hashes/ciphers/curves from crypto C files, dead @list_get/@list_len codegen paths from loop.cpp/collections.cpp, 26 dead C++ unit tests (-471 lines net)
- Phase 44: removed hardcoded primitive to_string()/debug_string() dispatch from method_primitive.cpp — Bool, I8-I128, U8-U128, F32, F64 now use TML Display/Debug behavior impls; Str/Char kept as special cases; migrated Maybe/Outcome to_string to behavior dispatch; removed 3 inline IR (i32_to_string, i64_to_string, bool_to_string)
- Phase 45: replaced i64_to_str/f64_to_str/float_to_string with TML Display behavior dispatch in string interpolation (core.cpp) and derive codegen (display/debug/serialize.cpp); removed 3 inline IR functions; inlined float_to_string into f64/f32_to_string
- Current: 12 inline IR functions remain (9 active + 3 black_box); 7 live math builtin handlers; 0 C migration candidates; all primitive formatting uses TML Display behavior dispatch

**Next actionable items**:
- Fix lazy-lib resolution for Char Display/Debug impls
- Phase 29.2-29.3 (deferred cleanup), Phase 30.3 (benchmark)
- Remaining Phase 4 work: reduce 12 inline IR functions to 3 (black_box only) — 6 float snprintf functions used by TML lowlevel, str_eq (7+ C++ callsites), str_concat_opt (25+ C++ callsites), str_as_bytes (1 TML lowlevel callsite)

**Gate**: Zero types with hardcoded dispatch. C runtime reduced to essential I/O + FFI wrappers only. Inline IR reduced to 3 black_box functions only.

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

- [ ] 6.0.1 `migrate-runtime-to-tml` complete (Phase 4) — includes codegen dispatch cleanup (0 types with hardcoded bypass)
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
- [ ] 6.5.2 Full test suite passes on self-hosted compiler (9,000+ tests)
- [ ] 6.5.3 Performance within 3x of C++ compiler
- [ ] 6.5.4 C++ compiler relegated to bootstrap-only role

**Gate**: TML compiler compiles itself. Bootstrap chain produces byte-identical output.

---

## Parallel Tracks (independent of main phases)

These can be worked on alongside the main phases without blocking or being blocked.

### Developer Tooling

**Status**: 78% complete
**Tracking**: [developer-tooling/tasks.md](../rulebook/tasks/developer-tooling/tasks.md)

- [x] VSCode extension (published v0.17.0)
- [x] Compiler MCP server (21 tools — includes `project/slow-tests` for per-file timing analysis)
- [x] Code formatter
- [x] Linter
- [x] `__FILE__`, `__DIRNAME__`, `__LINE__` compile-time constants (2026-02-17)
- [x] 19 slash command skills for Claude Code (build, test, run, compile, check, emit-ir, emit-mir, format, lint, docs, coverage, explain, review-pr, commit, slow-tests, cache-invalidate) (2026-02-18)
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
| 1. Codegen bugs | 43 | 43 | 100% | **COMPLETE** |
| 2. Test coverage | 95 | 75 | 79% | **COMPLETE** (76.2%) |
| 3. Stdlib essentials | 48 | 47 | 98% | **EFFECTIVELY COMPLETE** |
| 4. Runtime migration + codegen cleanup | 76 | 57 | 75% | IN PROGRESS — C files: 8 deleted, 2 remain; inline IR: 28 functions to migrate → pure TML (Phase 33); remaining C: search.c, pool.c, collections.c, glob.c (Phases 34-35) |
| 5. Async + networking | 27 | 0 | 0% | NOT STARTED |
| 6. Self-hosting | 22 | 0 | 0% | NOT STARTED |
| Parallel: Tooling | 9 | 7 | 78% | IN PROGRESS |
| Parallel: Reflection | 5 | 3 | 60% | IN PROGRESS |
| **TOTAL** | **325** | **232** | **71.4%** | |

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
