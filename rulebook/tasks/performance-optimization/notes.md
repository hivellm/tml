# Performance Optimization Notes

## Current Performance Gaps (Updated 2026-01-20)

| Area | Initial Gap | Current Gap | Target | Status |
|------|-------------|-------------|--------|--------|
| String Concat (literals) | 300x slower | **~1x** | < 2x | COMPLETE |
| Int to String | 22x slower | **1.7x** | < 2x | COMPLETE |
| Text Building | 28x slower | **~6x (push_str_len)** | < 2x | Improved |
| Small Appends push() | 10x slower | **1.4x faster** | < 2x | TML WINS |
| Array Iteration | 6-7x slower | **Optimized** | < 2x | BCE Implemented |
| Loop + Continue | 3.5x slower | **Optimized** | < 1.5x | Stacksave removed |
| MIR Lowlevel Blocks | **Broken** | **Fixed** | Working | COMPLETE |

## Latest Benchmark Results (2026-01-20)

| Benchmark | C++ ops/sec | TML ops/sec | Ratio | Status |
|-----------|-------------|-------------|-------|--------|
| Build JSON | 2.5M | 8.6M | **3.4x** | TML wins! |
| Number Formatting | 1.6M | 12.8M | **8x** | TML wins! |
| fill_char() batch | 1.2B | 43B | **36x** | TML wins! |
| Small Appends push() | 1.2B | 1.64B | **1.4x** | TML wins! |
| Small Appends push_str() | N/A | 70M | N/A | MIR Inlined |
| Small Appends raw ptr | N/A | 43B | N/A | Optimized |
| Build HTML | 65M | 20M | 0.30x | FFI overhead (str_len) |
| Build CSV | 54M | 10M | 0.19x | FFI overhead (str_len) |
| Log Messages | 31.5M | 8.3M | 0.26x | FFI overhead |
| Path Building | 71.8M | 12M | 0.17x | FFI overhead |

## MIR Inline Optimizations

Direct inline optimizations in MIR codegen for Text methods:
- `len()` - Branchless select for SSO/heap mode
- `clear()` - Inline stores to both SSO/heap locations
- `is_empty()` - Branchless select
- `capacity()` - Branchless select
- `push()` - V8-style inline with heap mode fast path (1.7B ops/sec)
- `push_str()` - Inline memcpy fast path + constant length (72M ops/sec)
- `push_i64()` - Fast path with push_i64_unsafe
- `push_formatted()` - Inline memcpy + push_i64_unsafe + constant length

## Constant String Length Propagation

Compile-time constant string length detection eliminates `str_len` FFI calls:
- `push_str("hello")` -> uses length 5 directly (no FFI)
- `push_formatted("prefix", n, "suffix")` -> prefix_len=6, suffix_len=6 directly
- Tracked via `value_string_contents_` map (ValueId -> string content)

## Legacy Codegen Inline Optimizations

- `push()` - V8-style inline for heap mode fast path
- `push_str_len()` - Inline memcpy for heap mode fast path
- `push_formatted()` - Inline prefix/suffix memcpy with push_i64_unsafe
- `push_log()` - Inline all string memcpy with push_i64_unsafe

## Runtime Optimizations

- `tml_text_push_i64_unsafe()` - No-check version for when heap mode/capacity verified

## Success Criteria

| Benchmark | Initial | Current | Target | Status |
|-----------|---------|---------|--------|--------|
| String Concat Small | 0.004x | ~1.0x | > 0.5x | Done |
| String Concat Loop | 0.09x | ~1.0x | > 0.5x | Done |
| Int to String | 0.045x | 0.34x | > 0.5x | In Progress |
| Build JSON | 0.83x | **2.7x** | > 0.5x | TML wins! |
| Build HTML | 0.07x | 0.20x | > 0.5x | FFI overhead |
| Build CSV | 0.04x | 0.28x | > 0.5x | FFI overhead |
| Number Formatting | N/A | **6.5x** | > 0.5x | TML wins! |
| fill_char() batch | N/A | **28x** | > 0.5x | TML wins! |
| Small Appends push() | N/A | 0.08x | > 0.5x | FFI overhead |
| Log Messages | N/A | 0.23x | > 0.5x | FFI overhead |
| Path Building | N/A | 0.17x | > 0.5x | Improved |
| Loop + Continue | 0.28x | ~0.7x | > 0.67x | Done |
| Higher Order Func | 0.64x | ~0.6x | > 0.5x | Done |

**Final Goal**: All benchmarks within 2x of C++ (ratio > 0.5x)

## Key Wins

- Build JSON: 2.7x faster than C++ due to optimized `to_string`
- Number Formatting: 6.5x faster than C++ due to lookup table optimization
- fill_char() batch: 28x faster than C++ by reducing FFI overhead
- String literals: Fully optimized at compile time

## Optimizations Implemented

- `fill_char()`: Batch character fill with single FFI call (memset-based)
- `push_formatted()`: Combines prefix + int + suffix in one FFI call
- `push_log()`: Combines s1 + n1 + s2 + n2 + s3 + n3 + s4 in one FFI call
- `push_path()`: Combines s1 + n1 + s2 + n2 + s3 in one FFI call
- Path Building: Reuse Text object instead of allocate/deallocate per iteration
