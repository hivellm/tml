# Tasks: Advanced Compiler Optimizations

**Status**: ARCHIVED (All 10 Phases Complete)

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

## Phase 2: Bounds Check Elimination ✅

> **Status**: Complete - See `compiler/src/mir/passes/bounds_check_elimination.cpp`

- [x] 2.1 Implement Value Range Analysis (track integer ranges) - `ValueRange` struct, `compute_value_ranges()`
- [x] 2.2 Implement loop bounds inference (`loop i in 0 to n` → `0 <= i < n`) - `detect_loops()`, `LoopBoundsInfo`
- [x] 2.3 Compare index range against array size at compile time - `mark_safe_accesses()`
- [x] 2.4 Remove bounds checks when index provably in bounds - `apply_eliminations()`
- [x] 2.5 Handle nested loops and reverse iteration patterns - `get_induction_bounds()`
- [x] 2.6 Add bounds check elimination statistics - `BoundsCheckEliminationStats`

## Phase 3: Return Value Optimization (RVO) ✅

> **Status**: Complete - See `compiler/src/mir/passes/rvo.cpp`

- [x] 3.1 Implement Named Return Value Optimization (NRVO) - via `BuilderOptPass`
- [x] 3.2 Implement anonymous RVO for `return StructLiteral { ... }` - via copy elision
- [x] 3.3 Implement guaranteed copy elision for temporaries (`apply_copy_elision`)
- [x] 3.4 Handle multiple return statements (common return variable) - `RvoPass::all_returns_same_local()`
- [x] 3.5 Update codegen to pass hidden return pointer for large structs - `RvoPass::should_use_sret()`

**Related implementations:**
- RVO pass (`compiler/src/mir/passes/rvo.cpp`) - multiple return unification, sret detection
- Builder pattern optimization (`compiler/src/mir/passes/builder_opt.cpp`)
- Method chain detection and intermediate object elimination
- Constructor fusion (`compiler/src/mir/passes/constructor_fusion.cpp`)

## Phase 4: Alias Analysis ✅

> **Status**: Complete - See `compiler/src/mir/passes/alias_analysis.cpp`

- [x] 4.1 Implement basic alias analysis (stack/global don't alias) - `basic_alias()`
- [x] 4.2 Implement type-based alias analysis (TBAA) - `type_based_alias()`
- [x] 4.3 Implement field-sensitive analysis (struct fields don't alias) - `field_alias()`, `gep_alias()`
- [x] 4.4 Implement flow-sensitive analysis (track through CFG) - `analyze_pointers()`, `trace_pointer_origin()`
- [x] 4.5 Integrate with LoadStoreOpt, LICM, and GVN passes - Added to O1 pipeline before LoadStoreOpt

**Key components:**
- `AliasResult`: NoAlias/MayAlias/MustAlias/PartialAlias
- `MemoryLocation`: Base pointer + offset + size + type
- `PointerInfo`: Origin tracking (StackAlloca/HeapAlloc/GEP/etc.)
- `AliasAnalysisStats`: Query statistics tracking

## Phase 5: Interprocedural Optimizations (IPO) ✅

> **Status**: Complete - See `compiler/src/mir/passes/ipo.cpp`

- [x] 5.1 Build and use call graph for analysis (`build_call_graph` in DeadMethodEliminationPass)
- [x] 5.2 Implement interprocedural constant propagation - `IpcpPass`, `gather_constants()`
- [x] 5.3 Implement argument promotion (ref → value for small types) - `ArgPromotionPass`, `can_promote_argument()`
- [x] 5.4 Enhance dead argument elimination for cross-module (`DeadArgEliminationPass`)
- [x] 5.5 Implement function attribute inference (`@pure`, `@nothrow`) - `AttrInferencePass`, `FunctionAttributes`

**Key components:**
- `IpoPass`: Main interprocedural optimization pass
- `IpcpPass`: Interprocedural constant propagation
- `ArgPromotionPass`: Reference to value argument promotion
- `AttrInferencePass`: Infers @pure, @nothrow, @readonly, @willreturn

**Related implementations:**
- Dead function elimination (`DeadFunctionEliminationPass`)
- Dead method elimination for OOP (`DeadMethodEliminationPass`)

## Phase 6: SIMD Vectorization ✅

> **Status**: Complete - See `compiler/src/mir/passes/vectorization.cpp`

- [x] 6.1 Implement loop vectorization analysis - `LoopVectorizationPass`, `analyze_loops()`, `can_vectorize_loop()`
- [x] 6.2 Implement memory dependence analysis - `MemoryDependenceAnalysis`, `get_dependence()`, `can_vectorize()`
- [x] 6.3 Handle reductions (sum, product, min, max) - `detect_reductions()`, `ReductionInfo`, `ReductionOp`
- [x] 6.4 Implement SLP (Superword Level Parallelism) - `SLPVectorizationPass`, `find_slp_groups()`, `are_isomorphic()`
- [x] 6.5 Generate LLVM vector IR (`<4 x float>`, etc.) - `get_llvm_vector_type()`, vector type infrastructure
- [x] 6.6 Add SIMD types to TML (`Vec4F32`, `Vec2F64`) - `VectorElementType`, `VectorWidth`, `TargetVectorWidth`
- [x] 6.7 Add `@simd` directive for explicit vectorization hints - `VectorizationConfig::force_vectorize`

**Key components:**
- `VectorizationPass`: Combined pass running loop and SLP vectorization
- `LoopVectorizationPass`: Analyzes and vectorizes loops with induction variables
- `SLPVectorizationPass`: Finds isomorphic scalar ops to combine into vectors
- `MemoryDependenceAnalysis`: RAW/WAR/WAW dependence tracking for safety
- `VectorizationConfig`: Target width (SSE/AVX/AVX512), trip count thresholds
- `VectorizationStats`: Tracks loops analyzed/vectorized, SLP groups, reductions

## Phase 7: Profile-Guided Optimization (PGO) ✅

> **Status**: Complete - See `compiler/src/mir/passes/pgo.cpp`

- [x] 7.1 Implement instrumentation pass (insert counters) - `ProfileInstrumentationPass`, `add_block_counters()`
- [x] 7.2 Design and implement profile data format (`ProfileData` struct, text format)
- [x] 7.3 Implement profile reader and merger - `ProfileIO::read()`, `ProfileIO::merge()`
- [x] 7.4 Update inlining to prefer hot call sites - `PgoInliningPass`, `should_inline()`, `get_inline_priority()`
- [x] 7.5 Update branch layout (likely path first) - `BranchProbabilityPass`, `BlockLayoutPass`
- [x] 7.6 Add `tml build --profile-generate` option - Added to dispatcher.cpp
- [x] 7.7 Add `tml build --profile-use=data.prof` option - Added to dispatcher.cpp

**Key components:**
- `ProfileData`: Function profiles, block counts, edge counts, call site profiles
- `ProfileIO`: Read/write/merge profile data files
- `ProfileInstrumentationPass`: Insert profiling counters
- `PgoInliningPass`: Profile-guided inlining decisions
- `BranchProbabilityPass`: Apply branch hints based on edge frequencies
- `BlockLayoutPass`: Reorder blocks for better branch prediction
- `PgoPass`: Combined PGO optimization pass
- `InliningPass::set_profile_data()`: Direct PGO integration with main inliner
- `PassManager::set_profile_data()`: Pipeline-wide PGO configuration

## Phase 8: Advanced Loop Optimizations ✅

> **Status**: Complete - See `compiler/src/mir/passes/loop_opts.cpp`

- [x] 8.1 Implement loop interchange (swap nested loop order) - `LoopInterchangePass`, `can_interchange()`
- [x] 8.2 Implement loop tiling/blocking for cache locality - `LoopTilingPass`, `should_tile()`, `tile_size_`
- [x] 8.3 Implement loop fusion (combine adjacent loops) - `LoopFusionPass`, `have_same_bounds()`
- [x] 8.4 Implement loop distribution (split independent parts) - `LoopDistributionPass`, `find_independent_groups()`

**Key components:**
- `LoopInfo`: Header/latch blocks, body blocks, induction variable, bounds
- `AdvancedLoopOptPass`: Combined pass that applies all loop optimizations
- Loop tree construction with nesting depth tracking
- Back-edge detection for loop identification

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
| 2 | Bounds Check Elimination | **Complete** | 6/6 |
| 3 | Return Value Optimization | **Complete** | 5/5 |
| 4 | Alias Analysis | **Complete** | 5/5 |
| 5 | Interprocedural Optimizations | **Complete** | 5/5 |
| 6 | SIMD Vectorization | **Complete** | 7/7 |
| 7 | Profile-Guided Optimization | **Complete** | 7/7 |
| 8 | Advanced Loop Optimizations | **Complete** | 4/4 |
| 9 | Escape Analysis & Stack Promotion | **Complete** | 7/7 |
| 10 | OOP-Specific Optimizations | **Complete** | 6/6 |
