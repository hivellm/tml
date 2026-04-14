# 17 — Core Library Coverage Map

Mapping every `core` module to benchmark status.

## core::ops (Operators) — 12 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `ops::arith` (Add, Sub, Mul, Div, Rem) | Yes | 1.0x vs Rust | math_bench: integer/float identical |
| `ops::bit` (And, Or, Xor, Shl, Shr) | Yes | 1.0-2.2x | math_bench: bitwise ok |
| `ops::function` (Fn, FnMut, FnOnce) | Yes | 8x gap | closure_bench: fn ptr dispatch |
| `ops::index` (Index, IndexMut) | Yes | 3-5x | collections: bounds check overhead |
| `ops::deref` (Deref, DerefMut) | No | — | No dedicated benchmark |
| `ops::drop` (Drop, ManuallyDrop) | No | — | TML lacks auto Drop |
| `ops::range` (Range, RangeInclusive) | Indirect | OK | Used by for-in loops |
| `ops::try_trait` (Try/?) | No | — | No benchmark |
| `ops::coroutine` | No | — | No benchmark |
| `ops::async_function` | No | — | No benchmark |
| `ops::flags` | No | — | No benchmark |

**Coverage**: 5/12 modules (42%)

## core::num (Numeric) — 7 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `num::integer` (abs, signum, bit ops) | Yes | 1.0x | math_bench |
| `num::traits` (numeric behaviors) | Yes | 1.0x | type_bench |
| `num::saturating` | No | — | No benchmark |
| `num::overflow` | No | — | No benchmark |
| `num::constants` | Indirect | OK | Used in math ops |

**Coverage**: 2/7 modules (29%)

## core::str (String) — 9 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `str::len` | BLOCKED | K001 | Undefined symbol |
| `str::contains` | BLOCKED | K001 | Depends on len |
| `str::find` | BLOCKED | K001 | Depends on len |
| `str::split` | BLOCKED | K001 | Depends on len |
| `str::trim` | BLOCKED | K001 | Depends on len |
| `str::starts_with` | BLOCKED | K001 | Depends on len |
| `str::conversion` | Partial | OK | `to_string()` works in benchmarks |
| `str::simd` | BLOCKED | K001 | SIMD search blocked |

**Coverage**: 0/9 modules (0%) — ALL BLOCKED

## core::fmt (Formatting) — 9 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `fmt::traits` (Display, Debug) | Indirect | — | `print()` works, no perf data |
| `fmt::num` | Indirect | ~50 ns/op | Number formatting in text_bench |
| `fmt::formatter` | No | — | No benchmark |
| `fmt::builders` | No | — | No benchmark |
| `fmt::impls` | Indirect | — | Used by to_string() |

**Coverage**: 0/9 directly (0%)

## core::iter (Iterators) — 2+ files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `iter::traits` (Iterator, IntoIterator) | Simulated | ~1 ns/op | closure_bench simulations |
| `iter::adapters` (map, filter, take) | Simulated | 669-729M ops/sec | Manual simulation |
| `iter::sources` (empty, once, repeat) | No | — | No benchmark |

**Coverage**: Indirect only — no real iterator benchmarks

## core::encoding (Encoding) — 15 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `encoding::base64` | Yes | 134-346 ns/op | 2.4-3.1x vs Rust |
| `encoding::hex` | Yes | 119 ns/op | TML wins vs naive Rust |
| `encoding::base32` | Yes | 123 ns/op | No Rust comparison |
| `encoding::base58` | No | — | |
| `encoding::base62` | No | — | |
| `encoding::base91` | No | — | |
| `encoding::ascii85` | No | — | |
| `encoding::percent` | No | — | |
| `encoding::bstr` | No | — | |
| `encoding::base8` | No | — | |
| `encoding::base16` | No | — | |
| `encoding::base36` | No | — | |
| `encoding::base45` | No | — | |
| `encoding::base64url` | No | — | |

**Coverage**: 3/15 modules (20%)

## core::ptr (Pointers) — 6 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `ptr::const_ptr` | Indirect | — | Used in array access |
| `ptr::mut_ptr` | Indirect | — | Used in raw pointer bench |
| `ptr::non_null` | No | — | |
| `ptr::alignment` | No | — | |
| `ptr::operations` | Indirect | — | Used in memory_bench |

**Coverage**: 0/6 directly (0%)

## core::cell (Interior Mutability) — 6 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `cell::unsafe_cell` | No | — | |
| `cell::cell` | No | — | |
| `cell::ref_cell` | No | — | |
| `cell::lazy` | No | — | |
| `cell::once_cell` | No | — | |

**Coverage**: 0/6 (0%)

## core::data (Data Structures) — 7 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `data::arena` | No | — | |
| `data::bitset` | No | — | |
| `data::ringbuf` | No | — | |
| `data::pool` | No | — | |
| `data::soo` | No | — | |

**Coverage**: 0/7 (0%)

## core::simd (SIMD Vectors) — 20 files

| Module | Benchmarked | Result | Notes |
|--------|------------|--------|-------|
| `simd::f32x4` | No | — | string_simd_bench exists but blocked |
| `simd::f64x2` | No | — | |
| `simd::i32x4` | No | — | |
| `simd::i64x2` | No | — | |
| `simd::u8x16` | No | — | |
| `simd::mask4` | No | — | |

**Coverage**: 0/20 (0%)

## Summary

| core Category | Files | Benchmarked | Coverage |
|--------------|-------|------------|----------|
| ops (operators) | 12 | 5 | 42% |
| num (numeric) | 7 | 2 | 29% |
| str (string) | 9 | 0 | 0% (BLOCKED) |
| fmt (formatting) | 9 | 0 | 0% |
| iter (iterators) | 2+ | 0* | 0% |
| encoding | 15 | 3 | 20% |
| ptr (pointers) | 6 | 0 | 0% |
| cell (mutability) | 6 | 0 | 0% |
| data (structures) | 7 | 0 | 0% |
| simd (vectors) | 20 | 0 | 0% (BLOCKED) |
| traits/cmp | 7 | 0 | 0% |
| alloc (memory) | 6 | 0* | 0% |
| types (tuple/range) | 5 | 0 | 0% |
| async/future | 5 | 0 | 0% |
| **TOTAL** | **~120** | **~10** | **~8%** |

**Only 8% of core is covered by benchmarks.** The largest gap is string/SIMD (29 modules, all blocked).
