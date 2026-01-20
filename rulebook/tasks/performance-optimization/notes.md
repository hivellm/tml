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
| Build JSON | 2.5M | 9.6M | **3.8x** | TML wins! |
| Build HTML | 65M | 20.8M | 0.32x | FFI overhead (push_i64) |
| Build CSV | 54M | 11.2M | 0.21x | FFI overhead (push_i64) |
| Number Formatting | 1.6M | 13.3M | **8.3x** | TML wins! |
| fill_char() batch | 1.2B | 28.4B | **24x** | TML wins! |
| Small Appends push() | 1.2B | 1.63B | **1.4x** | TML wins! |
| Small Appends push_str() | N/A | 74.7M | N/A | MIR Inlined |
| Small Appends raw ptr | N/A | 25.8B | N/A | store_byte intrinsic |
| Log Messages | 31.5M | 8.5M | 0.27x | FFI overhead (push_i64) |
| Path Building | 71.8M | 12.8M | 0.18x | FFI overhead (push_i64) |

## MIR Inline Optimizations

Direct inline optimizations in MIR codegen for Text methods:
- `len()` - Branchless select for SSO/heap mode
- `clear()` - Inline stores to both SSO/heap locations
- `is_empty()` - Branchless select
- `capacity()` - Branchless select
- `push()` - V8-style inline with heap mode fast path (1.7B ops/sec)
- `push_str()` - Inline memcpy fast path + constant length (73M ops/sec)
- `push_i64()` - Fast path with push_i64_unsafe
- `push_formatted()` - Inline memcpy + push_i64_unsafe + constant length
- `push_log()` - Inline 4 memcpy + 3 push_i64_unsafe + constant lengths (8.1M ops/sec)
- `push_path()` - Inline 3 memcpy + 2 push_i64_unsafe + constant lengths (12.4M ops/sec)

## Constant String Length Propagation

Compile-time constant string length detection eliminates `str_len` FFI calls:
- `push_str("hello")` -> uses length 5 directly (no FFI)
- `push_formatted("prefix", n, "suffix")` -> prefix_len=6, suffix_len=6 directly
- `push_log("[", n1, "] INFO: ...", n2, ...)` -> all 4 string lengths computed at compile-time
- `push_path("/path/", n1, "/file", n2, ".txt")` -> all 3 string lengths computed at compile-time
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
- `store_byte()`: MIR intrinsic for direct memory write (GEP + store, no FFI)
- Path Building: Reuse Text object instead of allocate/deallocate per iteration

## Memory Intrinsics (MIR Codegen)

- `store_byte(ptr, offset, byte)`: Direct byte store at ptr+offset without FFI (25.8B ops/sec)

## Remaining Bottleneck: Integer-to-String Conversion

The remaining performance gap (Build HTML 0.32x, Build CSV 0.21x, Log Messages 0.27x, Path Building 0.18x) is due to FFI overhead from `tml_text_push_i64_unsafe` calls. The runtime uses an optimized lookup table algorithm but each call still has FFI overhead.

**Potential future optimizations:**
1. Enable LTO (Link-Time Optimization) to allow LLVM to inline FFI functions
2. Generate int-to-string conversion directly in LLVM IR
3. Use small integer lookup tables (0-99, 0-999) for common cases
4. Specialize for known value ranges during compilation
