# Benchmark Baseline (2026-01-20)

## Test Environment
- **Platform**: Windows 10 x64
- **Compiler**: TML compiler (release build) + Clang 18 (C++ -O3)
- **CPU**: (to be filled)
- **Iterations**: 10M (unless noted)

---

## Complete Results

### Math Benchmarks

| Benchmark | C++ ops/sec | TML ops/sec | Ratio |
|-----------|------------|-------------|-------|
| Integer Addition | ∞ (optimized) | 167.8B | ∞ |
| Integer Multiplication | 371M | 168.4B | 453x ✅ |
| Integer Division | 773M | 168.4B | 217x ✅ |
| Integer Modulo | 2.01B | 168.4B | 83x ✅ |
| Bitwise Operations | 2.16B | 168.4B | 78x ✅ |
| Float Addition | 1.80B | 168.4B | 93x ✅ |
| Float Multiplication | 1.82B | 150.2B | 82x ✅ |
| Fib Recursive (n=20) | 84M | ∞ | ~equal |
| Fib Iterative (n=50) | ∞ | 157.2B | ~equal |
| Empty Loop | 10.9B | 168.9B | 15x ✅ |

### String Benchmarks

| Benchmark | C++ ops/sec | TML ops/sec | Ratio | Status |
|-----------|------------|-------------|-------|--------|
| Concat Small (3 strings) | 854M | 3.3M | **0.004x** | ❌ CRITICAL |
| Concat Loop (O(n)) | 826M | 77.9M | **0.09x** | ❌ |
| Concat Loop (naive O(n²)) | 456M | 190K | **0.0004x** | ❌ CRITICAL |
| String Length | ∞ | 240M | N/A | - |
| String Compare (equal) | ∞ | 295M | N/A | - |
| String Compare (different) | ∞ | 242M | N/A | - |
| Int to String | 165M | 7.4M | **0.045x** | ❌ |
| Log Building (Text) | 74.6M | 5.8M | **0.08x** | ❌ |

### Collections Benchmarks

| Benchmark | C++ ops/sec | TML ops/sec | Ratio | Status |
|-----------|------------|-------------|-------|--------|
| Vec Push (grow) | 244M | N/A | - | - |
| Vec Push (reserved) | 685M | N/A | - | - |
| Random Access | 3.31B | 1.76B | **0.53x** | ⚠️ |
| Sequential Read | 20.0B | 3.11B | **0.16x** | ❌ |
| Array Write | 3.06B | 164B | **53x** | ✅ |
| Linear Search | N/A | 791M | - | - |
| Accumulate Sum | N/A | 3.19B | - | - |
| HashMap Insert | 14.6M | N/A | - | - |
| HashMap Lookup | 376M | N/A | - | - |

### Memory Benchmarks

| Benchmark | C++ ops/sec | TML ops/sec | Ratio | Status |
|-----------|------------|-------------|-------|--------|
| malloc/free (64B) | 47M | N/A | - | - |
| new/delete Small | 45M | N/A | - | - |
| Stack Struct Small | ∞ | 162B | ∞ | ✅ |
| Stack Struct Medium | ∞ | 162B | ∞ | ✅ |
| Sequential Access | 3.48B | 2.78B | 0.80x | ✅ |
| Random Access | 2.17B | 2.28B | **1.05x** | ✅ |
| Struct Copy | 1.37B | N/A | - | - |
| Point Creation | N/A | 160B | - | ✅ |
| Array Fill | N/A | 5.08B | - | - |

### Text/StringBuilder Benchmarks

| Benchmark | C++ ops/sec | TML ops/sec | Ratio | Status |
|-----------|------------|-------------|-------|--------|
| stringstream Append | 6.8M | N/A | - | - |
| Reserve+Append | 99.7M | 5.6M | **0.06x** | ❌ |
| Naive Append | 468M | 187K | **0.0004x** | ❌ CRITICAL |
| Build JSON (10K) | 3.5M | 2.9M | 0.83x | ✅ |
| Build HTML (10K) | 86.4M | 5.8M | **0.07x** | ❌ |
| Build CSV (10K) | 53.3M | 2.1M | **0.04x** | ❌ |
| Small Appends (1 char) | 1.46B | 77.4M | **0.05x** | ❌ |
| Number Formatting | 2.0M | 3.1M | **1.55x** | ✅ |
| Log Messages | 38.0M | 2.0M | **0.05x** | ❌ |
| Path Building | 76.0M | 1.8M | **0.02x** | ❌ |

### Control Flow Benchmarks

| Benchmark | C++ ops/sec | TML ops/sec | Ratio | Status |
|-----------|------------|-------------|-------|--------|
| If-Else Chain (4) | 1.33B | 165B | **124x** | ✅ |
| Nested If (4 levels) | 3.51B | 165B | **47x** | ✅ |
| Switch Dense (10) | 1.22B | 165B | **135x** | ✅ |
| Switch Sparse (10) | 1.30B | 165B | **127x** | ✅ |
| Loop | ∞ | 158B | ~equal | ✅ |
| Nested Loops | ∞ | 2.65B | N/A | - |
| Loop + Continue | 6.44B | 1.81B | **0.28x** | ❌ |
| Ternary Chain | 1.34B | 165B | **124x** | ✅ |
| Short-Circuit AND | 990M | 162B | **164x** | ✅ |
| Short-Circuit OR | 955M | 165B | **173x** | ✅ |

### Function Call Benchmarks

| Benchmark | C++ ops/sec | TML ops/sec | Ratio | Status |
|-----------|------------|-------------|-------|--------|
| Inline Call | ∞ | 7.03B | ~equal | ✅ |
| Direct Call (noinline) | 1.07B | 7.11B | **6.6x** | ✅ |
| Many Params (6 args) | ∞ | 42.7B | ∞ | ✅ |
| Fib Recursive (n=20) | 94M | 97M | **1.03x** | ✅ |
| Fib Tail (n=50) | ∞ | 42.9B | ∞ | ✅ |
| Mutual Recursion | ∞ | 42.7B | ∞ | ✅ |
| Function Pointer | 940M | 7.17B | **7.6x** | ✅ |
| std::function | 383M | N/A | - | - |
| Virtual Call | 1.08B | N/A | - | - |

### Closure/Iterator Benchmarks

| Benchmark | C++ ops/sec | TML ops/sec | Ratio | Status |
|-----------|------------|-------------|-------|--------|
| Lambda/Func Pointer | ∞ | 7.17B | ~equal | ✅ |
| Func Pointer Switch | N/A | 246M | - | - |
| Higher Order Func | 2.62B | 1.67B | **0.64x** | ⚠️ |
| Closure Factory | 3.40B | 1.71B | **0.50x** | ⚠️ |
| Manual Loop (array) | 19.8B | 2.69B | **0.14x** | ❌ |
| Filter Pattern | 7.05B | 3.22B | **0.46x** | ⚠️ |
| Chain Operations | 6.12B | 3.14B | **0.51x** | ⚠️ |

---

## Summary Statistics

| Category | Tests | TML Wins | C++ Wins | Within 2x | Critical |
|----------|-------|----------|----------|-----------|----------|
| Math | 10 | 8 | 0 | 10 | 0 |
| String | 8 | 0 | 7 | 0 | **3** |
| Collections | 7 | 1 | 2 | 4 | 0 |
| Memory | 9 | 4 | 0 | 5 | 0 |
| Text/Builder | 10 | 2 | 8 | 3 | **1** |
| Control Flow | 10 | 9 | 1 | 10 | 0 |
| Functions | 9 | 7 | 0 | 9 | 0 |
| Closures | 7 | 1 | 5 | 3 | 0 |
| **TOTAL** | **70** | **32** | **23** | **44** | **4** |

### Critical Issues (> 100x slower)
1. String Concat Small: **250x slower**
2. String Concat Naive: **2500x slower**
3. Str Naive Append: **2500x slower**
4. (Borderline) Array Sequential Read: **6.2x slower**

### Good Performance (TML faster)
1. Control flow branching: **47-173x faster**
2. Function calls: **6.6-7.6x faster**
3. Math operations: **78-453x faster**
4. Array writes: **53x faster**
5. Stack struct creation: **∞x faster**
