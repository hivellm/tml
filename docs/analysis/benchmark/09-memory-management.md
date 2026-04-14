# 09 — Memory Management

## Leak Detection Results

| Benchmark | Leaks | Bytes Lost | Leak Size | Component |
|-----------|-------|-----------|-----------|-----------|
| encoding_bench | 200,000 | 2,800,000 | 14 bytes | String allocations |
| hashmap_bench | 0 | 0 | — | Manual `.destroy()` |
| list_bench | 0* | 0* | — | Manual cleanup |
| collections_bench | 0 | 0 | — | Stack arrays |
| math_bench | 0 | 0 | — | No heap |
| control_flow_bench | 0 | 0 | — | No heap |
| memory_bench | 0 | 0 | — | Stack-only |
| closure_bench | 0 | 0 | — | No heap |

*List bench calls `destroy()` explicitly.

## Encoding Leak Analysis

```
200000 leak(s), 2800000 bytes lost
#1: 14 bytes each (tag=mem_alloc)
```

100K iterations × 6 benchmarks × ~3 string allocations per call = ~200K allocations. Each `base64_encode()` / `hex_encode()` allocates a result string (14 bytes for "SGVsbG8sIFdvcmxkIQ==" padding). None are freed.

**Impact**: In a web server encoding 1000 requests/sec, this would leak ~14 KB/sec = ~1.2 GB/day.

## Memory Model Comparison

| Feature | Rust | TML |
|---------|------|-----|
| Ownership model | Compile-time borrow checker | Runtime reference tracking |
| Deallocation | Automatic via `Drop` trait | Manual `.destroy()` or GC |
| Leak prevention | Compiler-enforced | Runtime detector (debug only) |
| Double-free prevention | Compiler-enforced | Runtime checks |
| Use-after-free | Compiler-enforced | Runtime checks |
| Stack promotion | Aggressive (LLVM SROA) | Conservative |
| Copy semantics | Explicit `Clone`/`Copy` | `Duplicate` behavior |

## Allocation Patterns

### TML Allocation Overhead

| Operation | Allocations | Rust Equivalent |
|-----------|-------------|-----------------|
| `HashMap.new(N)` | 1 (bucket array) | Same |
| `HashMap.set(k, v)` | 0-1 (resize) | Same |
| `HashMap.destroy()` | Required manually | Automatic |
| `List.new()` | 1 (backing array) | Same |
| `List.push(v)` | 0-1 (resize) | Same |
| `Str + Str` | 1 (new string) | Same |
| `base64_encode(s)` | 1 (result string) | Same, but freed |

The allocation counts are similar. The difference is **deallocation**: Rust frees automatically, TML requires explicit cleanup or leaks.

## RAII Gap

TML lacks automatic resource cleanup via `Drop`/RAII:

```tml
// TML — manual cleanup required
let map: HashMap[I64, I64] = HashMap[I64, I64].new(16)
// ... use map ...
map.destroy()  // MUST call, or leak

// Rust — automatic cleanup
let map: HashMap<i64, i64> = HashMap::with_capacity(16);
// ... use map ...
// dropped automatically at end of scope
```

In benchmarks, this is manageable. In production code with error paths, early returns, and exception-like control flow, manual cleanup is error-prone and leads to the leaks seen in the encoding benchmark.

## Improvement Opportunities

| Priority | Change | Expected Impact |
|----------|--------|-----------------|
| P0 | Fix encoding string leaks | Correctness — prevents OOM |
| P0 | Implement `Drop` / `Disposable` behavior | Automatic cleanup |
| P1 | Scope-based deallocation (`defer`) | Covers error paths |
| P1 | Arena allocator for short-lived strings | 80% fewer allocs in encoding |
| P2 | Escape analysis for heap-to-stack promotion | Fewer allocs overall |
| P2 | String interning for small strings | SSO reduces alloc pressure |
