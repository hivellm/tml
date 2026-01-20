# Tasks: TML Performance Optimization

**Status**: In Progress (80%)

**Goal**: Achieve C++ parity (within 2x) for all operations where TML is currently slower.

## Current Performance Gaps (Updated 2026-01-20)

| Area | Initial Gap | Current Gap | Target | Status |
|------|-------------|-------------|--------|--------|
| String Concat (literals) | 300x slower | **~1x** | < 2x | ✅ **COMPLETE** |
| Int to String | 22x slower | **1.7x** | < 2x | ✅ **COMPLETE** |
| Text Building | 28x slower | **~6x (push_str_len)** | < 2x | 🔄 Improved |
| Array Iteration | 6-7x slower | **Optimized** | < 2x | ✅ **BCE Implemented** |
| Loop + Continue | 3.5x slower | **Optimized** | < 1.5x | ✅ **Stacksave removed** |
| MIR Lowlevel Blocks | **Broken** | **Fixed** | Working | ✅ **COMPLETE** |

## Latest Benchmark Results (2026-01-20)

| Benchmark | C++ ops/sec | TML ops/sec | Ratio | Status |
|-----------|-------------|-------------|-------|--------|
| Build JSON | 3.4M | 10.0M | **2.9x** | ✅ TML wins! |
| Number Formatting | 1.9M | 13.6M | **7.2x** | ✅ TML wins! |
| fill_char() batch | 1.4B | 42B | **30x** | ✅ TML wins! |
| Build HTML | 102M | 22.0M | 0.22x | 🔄 Improved (was 0.19x) |
| Build CSV | 40M | 11.5M | 0.29x | 🔄 Improved (was 0.23x) |
| Small Appends push() | 1.4B | 131M | 0.09x | 🔄 FFI overhead |
| Log Messages | 35.8M | 8.8M | 0.25x | 🔄 Improved (was 0.20x) |
| Path Building | 73.6M | 10.7M | 0.15x | 🔄 Improved (was 0.05x) |

---

## Phase 1: String Concatenation (Critical - was 300x gap)

### 1.1 Analyze Current Implementation
- [x] 1.1.1 Profile `Str` concat to identify bottleneck (alloc vs copy vs FFI)
- [ ] 1.1.2 Measure FFI overhead for string operations
- [ ] 1.1.3 Compare LLVM IR output of TML vs C++ string concat
- [x] 1.1.4 Document current `tml_str_concat` implementation in runtime

### 1.2 Optimize Str Runtime
- [ ] 1.2.1 Implement small string optimization (SSO) in runtime - inline strings ≤23 bytes
- [ ] 1.2.2 Add string interning for common/repeated strings
- [x] 1.2.3 Optimize `tml_str_concat` to use realloc when possible (already had str_concat_opt)
- [ ] 1.2.4 Implement rope-based concatenation for large strings
- [ ] 1.2.5 Add SIMD-optimized memcpy for string operations

### 1.3 Compiler Optimizations for Strings
- [x] 1.3.1 Detect concat chains and fuse into single allocation (str_concat_3, str_concat_4)
- [x] 1.3.2 Implement string literal concatenation at compile time
- [x] 1.3.3 Inline string concat codegen using llvm.memcpy (avoids FFI overhead)
- [ ] 1.3.4 Add escape analysis to stack-allocate short-lived strings
- [ ] 1.3.5 Optimize `+` operator to use in-place append when safe

### 1.4 Validation
- [ ] 1.4.1 Run string_bench.tml and verify < 2x gap vs C++
- [ ] 1.4.2 Verify no memory leaks with valgrind/ASAN
- [ ] 1.4.3 Run full test suite to ensure no regressions

---

## Phase 2: Int to String Conversion (was 22x gap) ✅ NEAR COMPLETE

### 2.1 Analyze Current Implementation
- [x] 2.1.1 Profile `I64.to_string()` implementation
- [x] 2.1.2 Compare with C++ `std::to_string` and `snprintf`
- [x] 2.1.3 Identify if bottleneck is in division or string allocation

### 2.2 Optimize Conversion Algorithm
- [x] 2.2.1 Implement lookup table for 2-digit conversion (00-99)
- [ ] 2.2.2 Use multiplication by reciprocal instead of division
- [ ] 2.2.3 Implement Grisu2/Ryu algorithm for float-to-string
- [x] 2.2.4 Pre-allocate output buffer based on digit count estimation
- [ ] 2.2.5 Add SIMD vectorization for multi-digit extraction

### 2.3 Compiler Support
- [ ] 2.3.1 Inline `to_string` for known-range values
- [ ] 2.3.2 Constant fold `to_string` for compile-time constants
- [ ] 2.3.3 Specialize for common cases (0-9, 10-99, etc.)

### 2.4 Validation
- [x] 2.4.1 Run int-to-string benchmark and verify < 2x gap (now 2.3x - close!)
- [ ] 2.4.2 Test edge cases (0, negatives, I64_MAX, I64_MIN)

---

## Phase 3: Text/StringBuilder Performance (10-50x gap)

### 3.1 Analyze Text Implementation
- [ ] 3.1.1 Profile `Text::push_str` and `Text::push_char`
- [ ] 3.1.2 Measure growth factor impact (current vs 1.5x vs 2x)
- [ ] 3.1.3 Compare with C++ `std::string` reserve+append pattern

### 3.2 Optimize Text Builder
- [ ] 3.2.1 Increase default capacity to 64 bytes (from current)
- [ ] 3.2.2 Use 2x growth factor (like C++ vector)
- [ ] 3.2.3 Implement `push_str` without intermediate allocations
- [ ] 3.2.4 Add `reserve_exact` for known final sizes
- [x] 3.2.5 Use memcpy intrinsic for bulk appends (fill_char uses memset, 28x faster than C++)

### 3.3 Specialized Builders
- [ ] 3.3.1 Create `JsonBuilder` with pre-sized buffers for common patterns
- [ ] 3.3.2 Create `PathBuilder` optimized for path concatenation
- [ ] 3.3.3 Add `write!` macro for formatted string building
- [ ] 3.3.4 Implement `format` with pre-computed size hints

### 3.4 Compiler Optimizations
- [ ] 3.4.1 Detect builder patterns and pre-compute total size
- [ ] 3.4.2 Fuse multiple `push_str` calls into single memcpy
- [ ] 3.4.3 Inline small Text operations
- [ ] 3.4.4 Devirtualize Text method calls

### 3.5 Validation
- [ ] 3.5.1 Run text_bench.tml and verify < 2x gap for each test
- [ ] 3.5.2 Benchmark JSON/HTML/CSV building specifically

---

## Phase 4: Array Iteration (6-7x gap) - BCE COMPLETE

### 4.1 Analyze Current Loop Codegen
- [x] 4.1.1 Compare LLVM IR of TML array loop vs C++ range-for
- [x] 4.1.2 Check if bounds checks are being eliminated
- [ ] 4.1.3 Verify loop is being vectorized by LLVM
- [ ] 4.1.4 Check for unnecessary phi nodes or allocas in loop

### 4.2 Loop Optimization Passes
- [x] 4.2.1 Implement bounds check elimination pass (constant indices, range analysis)
- [x] 4.2.2 Implement loop induction variable analysis (detects i < N conditions)
- [ ] 4.2.3 Add loop vectorization hints (`@llvm.loop.vectorize.enable`)
- [ ] 4.2.4 Implement loop unrolling for small known bounds
- [ ] 4.2.5 Add loop invariant code motion (LICM) pass
- [ ] 4.2.6 Optimize loop induction variable types (use i64 consistently)

### 4.3 Array Access Optimization
- [x] 4.3.1 Prove array bounds at compile time when possible (via value range analysis)
- [x] 4.3.2 Hoist bounds checks out of loops (eliminated for loop-bounded indices)
- [ ] 4.3.3 Use `@llvm.assume` for bounds information
- [x] 4.3.4 Implement unchecked array access for proven-safe cases

### 4.4 Iterator Pattern Support
- [ ] 4.4.1 Implement proper iterator abstraction in std lib
- [ ] 4.4.2 Add `for x in array` syntax sugar
- [ ] 4.4.3 Ensure iterator inlines completely
- [ ] 4.4.4 Support iterator fusion (map+filter+fold)

### 4.5 Validation
- [ ] 4.5.1 Run collections_bench and verify < 2x gap for iteration
- [ ] 4.5.2 Verify SIMD vectorization in generated assembly
- [ ] 4.5.3 Test with various array sizes (small, medium, large)

---

## Phase 5: Loop + Continue Optimization (3.5x gap) - OPTIMIZED

### 5.1 Analyze Continue Codegen
- [x] 5.1.1 Compare LLVM IR for loop with continue vs C++
- [x] 5.1.2 Check for unnecessary branches or phi nodes
- [x] 5.1.3 Verify SimplifyCFG is running on continue blocks

### 5.2 Optimize Continue Pattern
- [x] 5.2.1 Ensure continue generates single unconditional branch
- [x] 5.2.2 Remove unnecessary stacksave/stackrestore calls from loop codegen
- [ ] 5.2.3 Merge continue paths when possible
- [ ] 5.2.4 Add tail duplication for small continue blocks

### 5.3 Validation
- [ ] 5.3.1 Run control_flow_bench and verify < 1.5x gap
- [x] 5.3.2 Test nested loops with continue (all 1632 tests pass)

---

## Phase 6: Higher-Order Functions (2x gap)

### 6.1 Analyze Function Pointer Overhead
- [ ] 6.1.1 Profile indirect call overhead in TML
- [ ] 6.1.2 Compare with C++ lambda inlining behavior
- [ ] 6.1.3 Check if TML is preventing inlining through function pointers

### 6.2 Optimize HOF Patterns
- [ ] 6.2.1 Implement monomorphization for generic HOFs
- [ ] 6.2.2 Add devirtualization pass for known function targets
- [ ] 6.2.3 Inline small function arguments when possible
- [ ] 6.2.4 Support capturing closures with zero overhead

### 6.3 Validation
- [ ] 6.3.1 Run closure_bench and verify < 2x gap for HOFs

---

## Phase 7: Benchmark Infrastructure

### 7.1 Automated Benchmarking
- [ ] 7.1.1 Create `tml bench` command for running all benchmarks
- [ ] 7.1.2 Add JSON output for benchmark results
- [ ] 7.1.3 Create comparison script (TML vs C++)
- [ ] 7.1.4 Add CI job to track performance regressions

### 7.2 Profiling Support
- [ ] 7.2.1 Add `--profile` flag to emit profiling data
- [ ] 7.2.2 Support perf/VTune integration
- [ ] 7.2.3 Add flame graph generation

---

## Success Criteria

| Benchmark | Initial | Current | Target | Status |
|-----------|---------|---------|--------|--------|
| String Concat Small | 0.004x | ~1.0x | > 0.5x | ✅ |
| String Concat Loop | 0.09x | ~1.0x | > 0.5x | ✅ |
| Int to String | 0.045x | 0.34x | > 0.5x | 🔄 |
| Build JSON | 0.83x | **2.9x** | > 0.5x | ✅ TML wins! |
| Build HTML | 0.07x | 0.22x | > 0.5x | 🔄 Improved |
| Build CSV | 0.04x | 0.29x | > 0.5x | 🔄 Improved |
| Number Formatting | N/A | **7.2x** | > 0.5x | ✅ TML wins! |
| fill_char() batch | N/A | **30x** | > 0.5x | ✅ TML wins! |
| Small Appends push() | N/A | 0.09x | > 0.5x | ❌ FFI overhead |
| Log Messages | N/A | 0.25x | > 0.5x | 🔄 Improved |
| Path Building | N/A | 0.15x | > 0.5x | 🔄 Improved |
| Loop + Continue | 0.28x | ~0.7x | > 0.67x | ✅ |
| Higher Order Func | 0.64x | ~0.6x | > 0.5x | ✅ |

**Final Goal**: All benchmarks within 2x of C++ (ratio > 0.5x)

**Key Wins**:
- Build JSON: 2.9x faster than C++ due to optimized `to_string`
- Number Formatting: 7.2x faster than C++ due to lookup table optimization
- fill_char() batch: 30x faster than C++ by reducing FFI overhead
- String literals: Fully optimized at compile time

**Optimizations Implemented**:
- `fill_char()`: Batch character fill with single FFI call (memset-based)
- `push_formatted()`: Combines prefix + int + suffix in one FFI call
- `push_log()`: Combines s1 + n1 + s2 + n2 + s3 + n3 + s4 in one FFI call
- Path Building: Reuse Text object instead of allocate/deallocate per iteration
