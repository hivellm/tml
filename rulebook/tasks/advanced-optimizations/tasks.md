# Tasks: Advanced Compiler Optimizations

**Status**: Not started

**Priority**: High - Performance parity with Rust/LLVM

## Phase 1: Devirtualization

- [ ] 1.1 Implement type-based devirtualization analysis
- [ ] 1.2 Build impl resolution map (type → behavior → method)
- [ ] 1.3 Replace virtual dispatch with direct calls when type is known
- [ ] 1.4 Implement speculative devirtualization with type guards
- [ ] 1.5 Add devirtualization statistics to `--time` output

## Phase 2: Bounds Check Elimination

- [ ] 2.1 Implement Value Range Analysis (track integer ranges)
- [ ] 2.2 Implement loop bounds inference (`loop i in 0 to n` → `0 <= i < n`)
- [ ] 2.3 Compare index range against array size at compile time
- [ ] 2.4 Remove bounds checks when index provably in bounds
- [ ] 2.5 Handle nested loops and reverse iteration patterns
- [ ] 2.6 Add bounds check elimination statistics

## Phase 3: Return Value Optimization (RVO)

- [ ] 3.1 Implement Named Return Value Optimization (NRVO)
- [ ] 3.2 Implement anonymous RVO for `return StructLiteral { ... }`
- [ ] 3.3 Implement guaranteed copy elision for temporaries
- [ ] 3.4 Handle multiple return statements (common return variable)
- [ ] 3.5 Update codegen to pass hidden return pointer for large structs

## Phase 4: Alias Analysis

- [ ] 4.1 Implement basic alias analysis (stack/global don't alias)
- [ ] 4.2 Implement type-based alias analysis (TBAA)
- [ ] 4.3 Implement field-sensitive analysis (struct fields don't alias)
- [ ] 4.4 Implement flow-sensitive analysis (track through CFG)
- [ ] 4.5 Integrate with LoadStoreOpt, LICM, and GVN passes

## Phase 5: Interprocedural Optimizations (IPO)

- [ ] 5.1 Build and use call graph for analysis
- [ ] 5.2 Implement interprocedural constant propagation
- [ ] 5.3 Implement argument promotion (ref → value for small types)
- [ ] 5.4 Enhance dead argument elimination for cross-module
- [ ] 5.5 Implement function attribute inference (`@pure`, `@nothrow`)

## Phase 6: SIMD Vectorization

- [ ] 6.1 Implement loop vectorization analysis
- [ ] 6.2 Implement memory dependence analysis
- [ ] 6.3 Handle reductions (sum, product, min, max)
- [ ] 6.4 Implement SLP (Superword Level Parallelism)
- [ ] 6.5 Generate LLVM vector IR (`<4 x float>`, etc.)
- [ ] 6.6 Add SIMD types to TML (`Vec4F32`, `Vec2F64`)
- [ ] 6.7 Add `@simd` directive for explicit vectorization hints

## Phase 7: Profile-Guided Optimization (PGO)

- [ ] 7.1 Implement instrumentation pass (insert counters)
- [ ] 7.2 Design and implement profile data format
- [ ] 7.3 Implement profile reader and merger
- [ ] 7.4 Update inlining to prefer hot call sites
- [ ] 7.5 Update branch layout (likely path first)
- [ ] 7.6 Add `tml build --profile-generate` option
- [ ] 7.7 Add `tml build --profile-use=data.prof` option

## Phase 8: Advanced Loop Optimizations

- [ ] 8.1 Implement loop interchange (swap nested loop order)
- [ ] 8.2 Implement loop tiling/blocking for cache locality
- [ ] 8.3 Implement loop fusion (combine adjacent loops)
- [ ] 8.4 Implement loop distribution (split independent parts)

## Validation

- [ ] All 906+ existing tests pass after each phase
- [ ] New optimization passes have unit tests
- [ ] Benchmark suite shows performance improvements
- [ ] No regressions in compile time (< 10% increase acceptable)
- [ ] Debug info preserved through optimizations
