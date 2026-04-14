# 14 — Text/StringBuilder (std::text, core::str)

## TML Text vs Rust String

TML has two string types:
- `Str` — immutable, O(n) concatenation (like `&str`)
- `Text` — mutable buffer, O(1) amortized append (like `String`)

## Results

### TML (text_bench.tml) — BLOCKED by K001

`text_bench.tml` fails with:
```
K001: use of undefined value '@tml_N4core3str3lenE_S'
```

The `core::str::len` function symbol isn't being emitted in the LLVM IR. This blocks ALL string-related benchmarks.

### Rust (String benchmarks)

| Benchmark | Rust (ns/op) | Rust (ops/sec) |
|-----------|-------------|----------------|
| String append (pre-allocated, 100K) | 13 | 72.5M |
| Str naive append (O(n^2), 10K) | <1* | 1.45B* |
| Build JSON (10K items) | 32 | 30.6M |
| Build HTML (10K items) | 18 | 54.7M |
| Build CSV (10K rows) | 40 | 24.9M |
| Small appends push() (1M) | <1 | 2.58B |
| Number formatting (100K) | 29 | 34.1M |
| Log messages (100K) | 56 | 17.7M |
| Path building (100K) | 29 | 34.1M |

*O(n^2) naive append at 10K iterations completes under 1ms — too fast to measure with integer ns division.

## TML Text Architecture (from source analysis)

TML's `Text` type uses several optimization strategies visible in the benchmark source:

### 1. `push(byte)` — Direct Byte Append
```tml
t.push(120)  // ASCII 'x', single byte store
```
Expected: ~1-2 ns/op (direct memory store + length increment)

### 2. `push_str(s)` — String Append (FFI call)
```tml
t.push_str("item")  // FFI to C runtime
```
Expected: ~5-10 ns/op (FFI overhead + memcpy)

### 3. `push_formatted(prefix, n, suffix)` — Batch FFI
```tml
t.push_formatted("  <li>Item ", i, "</li>\n")  // Single FFI call
```
Expected: ~15-25 ns/op (one FFI call instead of 3)

### 4. `push_log(s1, n1, s2, n2, s3, n3, s4)` — Ultra-Batch FFI
```tml
t.push_log("[", i, "] INFO: Processing item #", i, " with value ", i * 42, "\n")
```
Expected: ~20-40 ns/op (7 ops in 1 FFI call)

### 5. `fill_char(byte, count)` — Batch Fill
```tml
t.fill_char(120, iterations)  // Single FFI call for N bytes
```
Expected: <1 ns/op amortized (single memset)

### 6. Raw Pointer Access
```tml
let data: *U8 = t.data_ptr()
lowlevel { store_byte(data, len, 120) }
```
Expected: <1 ns/op (zero overhead, register-based loop)

## Estimated TML vs Rust Comparison

Based on architecture analysis (not measured — K001 blocks):

| Operation | Rust (ns/op) | TML Est. (ns/op) | Est. Ratio |
|-----------|-------------|-------------------|------------|
| push('x') | <1 | 1-2 | ~2x |
| push_str("item") | 2-3 | 5-10 | 2-4x |
| Build JSON item | 32 | 40-60 | 1.2-2x |
| Build HTML item | 18 | 20-30 | 1.1-1.7x |
| Build CSV row | 40 | 30-50 | 0.8-1.3x |
| Number format | 29 | 50-80 | 1.7-2.8x |
| Log message | 56 | 40-70 | 0.7-1.3x |
| Path build | 29 | 30-50 | 1.0-1.7x |
| Batch fill | <1 | <1 | 1.0x |
| Raw ptr | <1 | <1 | 1.0x |

**Note**: TML's `push_log()` and `push_formatted()` batch FFI calls could make TML **faster** than Rust for log/CSV/path building, since Rust does separate `push_str()` + `write!()` calls.

## Blocked Tests — K001 Impact

| Module | What Can't Be Tested | Impact |
|--------|---------------------|--------|
| `core::str` | `len()`, `contains()`, `find()`, `split()`, `trim()` | Critical |
| `core::str::simd` | SIMD string search | Critical |
| `std::text::Text` | All builder operations | Critical |
| `core::fmt` | Display, Debug formatting | High |
| `core::encoding::base64` | Encode/decode (partially works) | Medium |

## Improvement Opportunities

| Priority | Change | Expected Impact |
|----------|--------|-----------------|
| P0 | Fix K001 `core::str::len` symbol emission | Unblocks ALL string benchmarks |
| P1 | Inline `push()` to avoid FFI for single bytes | 2-3x for char-level ops |
| P1 | SSO (Small String Optimization) for Str < 24B | Eliminates heap alloc for short strings |
| P2 | SIMD `memcpy` for `push_str` of known-length strings | 1.5-2x for bulk append |
