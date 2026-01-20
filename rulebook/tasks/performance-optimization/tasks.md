# Tasks: TML Performance Optimization

**Status**: In Progress (80%)

**Goal**: Achieve C++ parity (within 2x) for all operations where TML is currently slower.

## Phase 1: String Concatenation

- [x] 1.1.1 Profile `Str` concat to identify bottleneck
- [ ] 1.1.2 Measure FFI overhead for string operations
- [ ] 1.1.3 Compare LLVM IR output of TML vs C++ string concat
- [x] 1.1.4 Document current `tml_str_concat` implementation
- [ ] 1.2.1 Implement small string optimization (SSO) in runtime
- [ ] 1.2.2 Add string interning for common/repeated strings
- [x] 1.2.3 Optimize `tml_str_concat` to use realloc when possible
- [ ] 1.2.4 Implement rope-based concatenation for large strings
- [ ] 1.2.5 Add SIMD-optimized memcpy for string operations
- [x] 1.3.1 Detect concat chains and fuse into single allocation
- [x] 1.3.2 Implement string literal concatenation at compile time
- [x] 1.3.3 Inline string concat codegen using llvm.memcpy
- [ ] 1.3.4 Add escape analysis to stack-allocate short-lived strings
- [ ] 1.3.5 Optimize `+` operator to use in-place append when safe
- [ ] 1.4.1 Run string_bench.tml and verify < 2x gap vs C++
- [ ] 1.4.2 Verify no memory leaks with valgrind/ASAN
- [ ] 1.4.3 Run full test suite to ensure no regressions

## Phase 2: Int to String Conversion

- [x] 2.1.1 Profile `I64.to_string()` implementation
- [x] 2.1.2 Compare with C++ `std::to_string` and `snprintf`
- [x] 2.1.3 Identify if bottleneck is in division or string allocation
- [x] 2.2.1 Implement lookup table for 2-digit conversion (00-99)
- [ ] 2.2.2 Use multiplication by reciprocal instead of division
- [ ] 2.2.3 Implement Grisu2/Ryu algorithm for float-to-string
- [x] 2.2.4 Pre-allocate output buffer based on digit count estimation
- [ ] 2.2.5 Add SIMD vectorization for multi-digit extraction
- [ ] 2.3.1 Inline `to_string` for known-range values
- [ ] 2.3.2 Constant fold `to_string` for compile-time constants
- [ ] 2.3.3 Specialize for common cases (0-9, 10-99, etc.)
- [x] 2.4.1 Run int-to-string benchmark and verify < 2x gap
- [ ] 2.4.2 Test edge cases (0, negatives, I64_MAX, I64_MIN)

## Phase 3: Text/StringBuilder Performance

- [ ] 3.1.1 Profile `Text::push_str` and `Text::push_char`
- [ ] 3.1.2 Measure growth factor impact (current vs 1.5x vs 2x)
- [ ] 3.1.3 Compare with C++ `std::string` reserve+append pattern
- [ ] 3.2.1 Increase default capacity to 64 bytes
- [ ] 3.2.2 Use 2x growth factor (like C++ vector)
- [ ] 3.2.3 Implement `push_str` without intermediate allocations
- [ ] 3.2.4 Add `reserve_exact` for known final sizes
- [x] 3.2.5 Use memcpy intrinsic for bulk appends
- [ ] 3.3.1 Create `JsonBuilder` with pre-sized buffers
- [ ] 3.3.2 Create `PathBuilder` optimized for path concatenation
- [ ] 3.3.3 Add `write!` macro for formatted string building
- [ ] 3.3.4 Implement `format` with pre-computed size hints
- [ ] 3.4.1 Detect builder patterns and pre-compute total size
- [ ] 3.4.2 Fuse multiple `push_str` calls into single memcpy
- [ ] 3.4.3 Inline small Text operations
- [ ] 3.4.4 Devirtualize Text method calls
- [ ] 3.5.1 Run text_bench.tml and verify < 2x gap
- [ ] 3.5.2 Benchmark JSON/HTML/CSV building specifically

## Phase 4: Array Iteration (BCE Complete)

- [x] 4.1.1 Compare LLVM IR of TML array loop vs C++ range-for
- [x] 4.1.2 Check if bounds checks are being eliminated
- [ ] 4.1.3 Verify loop is being vectorized by LLVM
- [ ] 4.1.4 Check for unnecessary phi nodes or allocas in loop
- [x] 4.2.1 Implement bounds check elimination pass
- [x] 4.2.2 Implement loop induction variable analysis
- [ ] 4.2.3 Add loop vectorization hints
- [ ] 4.2.4 Implement loop unrolling for small known bounds
- [ ] 4.2.5 Add loop invariant code motion (LICM) pass
- [ ] 4.2.6 Optimize loop induction variable types
- [x] 4.3.1 Prove array bounds at compile time when possible
- [x] 4.3.2 Hoist bounds checks out of loops
- [ ] 4.3.3 Use `@llvm.assume` for bounds information
- [x] 4.3.4 Implement unchecked array access for proven-safe cases
- [ ] 4.4.1 Implement proper iterator abstraction in std lib
- [ ] 4.4.2 Add `for x in array` syntax sugar
- [ ] 4.4.3 Ensure iterator inlines completely
- [ ] 4.4.4 Support iterator fusion (map+filter+fold)
- [ ] 4.5.1 Run collections_bench and verify < 2x gap
- [ ] 4.5.2 Verify SIMD vectorization in generated assembly
- [ ] 4.5.3 Test with various array sizes

## Phase 5: Loop + Continue Optimization (Complete)

- [x] 5.1.1 Compare LLVM IR for loop with continue vs C++
- [x] 5.1.2 Check for unnecessary branches or phi nodes
- [x] 5.1.3 Verify SimplifyCFG is running on continue blocks
- [x] 5.2.1 Ensure continue generates single unconditional branch
- [x] 5.2.2 Remove unnecessary stacksave/stackrestore calls
- [ ] 5.2.3 Merge continue paths when possible
- [ ] 5.2.4 Add tail duplication for small continue blocks
- [ ] 5.3.1 Run control_flow_bench and verify < 1.5x gap
- [x] 5.3.2 Test nested loops with continue

## Phase 6: Higher-Order Functions

- [ ] 6.1.1 Profile indirect call overhead in TML
- [ ] 6.1.2 Compare with C++ lambda inlining behavior
- [ ] 6.1.3 Check if TML is preventing inlining through function pointers
- [ ] 6.2.1 Implement monomorphization for generic HOFs
- [ ] 6.2.2 Add devirtualization pass for known function targets
- [ ] 6.2.3 Inline small function arguments when possible
- [ ] 6.2.4 Support capturing closures with zero overhead
- [ ] 6.3.1 Run closure_bench and verify < 2x gap

## Phase 7: Benchmark Infrastructure

- [ ] 7.1.1 Create `tml bench` command for running all benchmarks
- [ ] 7.1.2 Add JSON output for benchmark results
- [ ] 7.1.3 Create comparison script (TML vs C++)
- [ ] 7.1.4 Add CI job to track performance regressions
- [ ] 7.2.1 Add `--profile` flag to emit profiling data
- [ ] 7.2.2 Support perf/VTune integration
- [ ] 7.2.3 Add flame graph generation
