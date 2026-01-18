# Tasks: Advanced Compiler Optimizations

**Status**: In Progress (Phase 1 Complete, Phase 3 Partial)

**Priority**: High - Performance parity with Rust/LLVM

## Phase 1: Devirtualization ✅

> **Status**: Complete - See `compiler/src/mir/passes/devirtualization.cpp`

- [x] 1.1 Implement type-based devirtualization analysis (`DevirtualizationPass`)
- [x] 1.2 Build impl resolution map (type → behavior → method) - Class hierarchy analysis
- [x] 1.3 Replace virtual dispatch with direct calls when type is known (`DevirtReason::ExactType`)
- [x] 1.4 Implement speculative devirtualization with type guards (`gen_guarded_virtual_call`)
- [x] 1.5 Add devirtualization statistics to `--time` output (`DevirtualizationStats`)

**Additional devirtualization features implemented:**
- Sealed class devirtualization (`DevirtReason::SealedClass`)
- Final method devirtualization (`DevirtReason::FinalMethod`)
- Single implementation devirtualization (`DevirtReason::SingleImpl`)
- Dead method elimination (`DeadMethodEliminationPass`)
- Whole-program analysis mode (`WholeProgramConfig`)

## Phase 2: Bounds Check Elimination

- [ ] 2.1 Implement Value Range Analysis (track integer ranges)
- [ ] 2.2 Implement loop bounds inference (`loop i in 0 to n` → `0 <= i < n`)
- [ ] 2.3 Compare index range against array size at compile time
- [ ] 2.4 Remove bounds checks when index provably in bounds
- [ ] 2.5 Handle nested loops and reverse iteration patterns
- [ ] 2.6 Add bounds check elimination statistics

## Phase 3: Return Value Optimization (RVO) - Partial ✅

> **Status**: Partial - Builder pattern and copy elision implemented

- [x] 3.1 Implement Named Return Value Optimization (NRVO) - via `BuilderOptPass`
- [x] 3.2 Implement anonymous RVO for `return StructLiteral { ... }` - via copy elision
- [x] 3.3 Implement guaranteed copy elision for temporaries (`apply_copy_elision`)
- [ ] 3.4 Handle multiple return statements (common return variable)
- [ ] 3.5 Update codegen to pass hidden return pointer for large structs

**Related implementations:**
- Builder pattern optimization (`compiler/src/mir/passes/builder_opt.cpp`)
- Method chain detection and intermediate object elimination
- Constructor fusion (`compiler/src/mir/passes/constructor_fusion.cpp`)

## Phase 4: Alias Analysis

- [ ] 4.1 Implement basic alias analysis (stack/global don't alias)
- [ ] 4.2 Implement type-based alias analysis (TBAA)
- [ ] 4.3 Implement field-sensitive analysis (struct fields don't alias)
- [ ] 4.4 Implement flow-sensitive analysis (track through CFG)
- [ ] 4.5 Integrate with LoadStoreOpt, LICM, and GVN passes

## Phase 5: Interprocedural Optimizations (IPO) - Partial ✅

> **Status**: Partial - Call graph and dead code elimination implemented

- [x] 5.1 Build and use call graph for analysis (`build_call_graph` in DeadMethodEliminationPass)
- [ ] 5.2 Implement interprocedural constant propagation
- [ ] 5.3 Implement argument promotion (ref → value for small types)
- [x] 5.4 Enhance dead argument elimination for cross-module (`DeadArgEliminationPass`)
- [ ] 5.5 Implement function attribute inference (`@pure`, `@nothrow`)

**Related implementations:**
- Dead function elimination (`DeadFunctionEliminationPass`)
- Dead method elimination for OOP (`DeadMethodEliminationPass`)

## Phase 6: SIMD Vectorization

- [ ] 6.1 Implement loop vectorization analysis
- [ ] 6.2 Implement memory dependence analysis
- [ ] 6.3 Handle reductions (sum, product, min, max)
- [ ] 6.4 Implement SLP (Superword Level Parallelism)
- [ ] 6.5 Generate LLVM vector IR (`<4 x float>`, etc.)
- [ ] 6.6 Add SIMD types to TML (`Vec4F32`, `Vec2F64`)
- [ ] 6.7 Add `@simd` directive for explicit vectorization hints

## Phase 7: Profile-Guided Optimization (PGO) - Partial ✅

> **Status**: Partial - Type profiling infrastructure exists

- [x] 7.1 Implement instrumentation pass (insert counters) - `enable_instrumentation()` in devirt
- [x] 7.2 Design and implement profile data format (`TypeProfileFile` struct)
- [ ] 7.3 Implement profile reader and merger
- [ ] 7.4 Update inlining to prefer hot call sites
- [ ] 7.5 Update branch layout (likely path first)
- [ ] 7.6 Add `tml build --profile-generate` option
- [ ] 7.7 Add `tml build --profile-use=data.prof` option

**Related implementations:**
- Type frequency hints for speculative devirtualization
- Profile-guided type data format (`TypeProfileFile` in devirtualization.hpp)

## Phase 8: Advanced Loop Optimizations

- [ ] 8.1 Implement loop interchange (swap nested loop order)
- [ ] 8.2 Implement loop tiling/blocking for cache locality
- [ ] 8.3 Implement loop fusion (combine adjacent loops)
- [ ] 8.4 Implement loop distribution (split independent parts)

## Phase 9: Escape Analysis & Stack Promotion ✅

> **Status**: Complete - See `compiler/src/mir/passes/escape_analysis.cpp`

- [x] 9.1 Implement escape analysis (`EscapeAnalysisPass`)
- [x] 9.2 Track object lifetime through all uses
- [x] 9.3 Identify objects that escape (heap, global, return)
- [x] 9.4 Stack promotion for non-escaping objects (`StackPromotionPass`)
- [x] 9.5 Remove corresponding free calls for promoted objects
- [x] 9.6 Handle conditional escapes (`ConditionalEscape`)
- [x] 9.7 Integrate with SROA for scalar replacement

## Phase 10: OOP-Specific Optimizations ✅

> **Status**: Complete - See `compiler/src/mir/mir_pass.cpp`

- [x] 10.1 Constructor fusion (`ConstructorFusionPass`)
- [x] 10.2 Destructor hoisting (`DestructorHoistPass`)
- [x] 10.3 Batch destruction (`BatchDestructionPass`)
- [x] 10.4 Builder pattern optimization (`BuilderOptPass`)
- [x] 10.5 Vtable deduplication and splitting
- [x] 10.6 Value class auto-inference (`is_value_class_candidate`)

## Validation

- [x] All 1577+ existing tests pass after each phase
- [x] New optimization passes have unit tests (`oop_test.cpp`)
- [x] Benchmark suite shows performance improvements
- [ ] No regressions in compile time (< 10% increase acceptable)
- [x] Debug info preserved through optimizations

## Summary

| Phase | Description | Status | Progress |
|-------|-------------|--------|----------|
| 1 | Devirtualization | **Complete** | 5/5 |
| 2 | Bounds Check Elimination | Not Started | 0/6 |
| 3 | Return Value Optimization | Partial | 3/5 |
| 4 | Alias Analysis | Not Started | 0/5 |
| 5 | Interprocedural Optimizations | Partial | 2/5 |
| 6 | SIMD Vectorization | Not Started | 0/7 |
| 7 | Profile-Guided Optimization | Partial | 2/7 |
| 8 | Advanced Loop Optimizations | Not Started | 0/4 |
| 9 | Escape Analysis & Stack Promotion | **Complete** | 7/7 |
| 10 | OOP-Specific Optimizations | **Complete** | 6/6 |
