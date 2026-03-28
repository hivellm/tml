# Proposal: Advanced Compiler Optimizations

## Why

The TML compiler currently has 39 MIR optimization passes and 4 HIR passes, providing good baseline performance. However, to achieve performance parity with Rust/LLVM, several critical optimization categories are missing:

1. **No devirtualization**: Virtual calls through behaviors remain indirect even when the concrete type is known at compile time, adding unnecessary overhead.

2. **No bounds check elimination**: Array accesses always include runtime bounds checks, even in loops where the index is provably within bounds.

3. **No return value optimization**: Large structs are copied on return instead of being constructed directly in the caller's memory.

4. **Limited alias analysis**: The compiler assumes all pointers may alias, preventing many load/store optimizations.

5. **No interprocedural optimizations**: Each function is optimized in isolation, missing cross-function opportunities.

6. **No SIMD vectorization**: Loops over numeric arrays cannot be automatically vectorized.

7. **No profile-guided optimization**: Hot paths receive the same optimization attention as cold code.

These optimizations are standard in production compilers (GCC, LLVM, rustc) and their absence means TML code runs slower than equivalent Rust code.

## What Changes

### New Optimization Passes

1. **Devirtualization** (`compiler/src/hir/hir_opt/devirtualization.cpp`)
   - Type-based analysis to resolve virtual calls
   - Build impl resolution map (type → behavior → method)
   - Speculative devirtualization with type guards

2. **Bounds Check Elimination** (`compiler/src/mir/passes/bounds_check_elim.cpp`)
   - Value Range Analysis for integer tracking
   - Loop bounds inference
   - Safe removal of redundant bounds checks

3. **Return Value Optimization** (`compiler/src/mir/passes/rvo.cpp`)
   - Named RVO (NRVO)
   - Anonymous RVO for struct literals
   - Guaranteed copy elision

4. **Alias Analysis** (`compiler/src/mir/analysis/alias_analysis.cpp`)
   - Stack/global non-aliasing
   - Type-based alias analysis (TBAA)
   - Field-sensitive analysis
   - Flow-sensitive tracking

5. **Interprocedural Optimizations** (`compiler/src/mir/passes/ipo/`)
   - Call graph construction
   - Interprocedural constant propagation
   - Argument promotion
   - Function attribute inference

6. **SIMD Vectorization** (`compiler/src/mir/passes/vectorize/`)
   - Loop vectorization
   - Memory dependence analysis
   - SLP (Superword Level Parallelism)
   - New SIMD types: `Vec4F32`, `Vec2F64`, etc.

7. **Profile-Guided Optimization** (`compiler/src/mir/passes/pgo/`)
   - Instrumentation pass
   - Profile data format
   - Hot path prioritization
   - CLI: `--profile-generate`, `--profile-use`

8. **Advanced Loop Optimizations** (`compiler/src/mir/passes/loop/`)
   - Loop interchange
   - Loop tiling/blocking
   - Loop fusion
   - Loop distribution

### CLI Changes

New flags:
- `tml build --profile-generate` - Build with profiling instrumentation
- `tml build --profile-use=data.prof` - Use profile data for optimization
- `@simd` directive for explicit vectorization hints

### New Types

SIMD types in core library:
- `Vec4F32`, `Vec2F64` - Floating point vectors
- `Vec4I32`, `Vec2I64` - Integer vectors

## Impact

- **Affected specs**: None (internal compiler change)
- **Affected code**:
  - New: `compiler/src/mir/passes/`, `compiler/src/mir/analysis/`
  - Modified: Pass manager, codegen
- **Breaking change**: NO (internal change only)
- **User benefit**:
  - 2-5x speedup for numeric/array-heavy code
  - Reduced binary size from devirtualization
  - Better cache utilization from loop optimizations
  - Production-quality performance for real-world applications

## Dependencies

- HIR implementation (complete)
- MIR pass infrastructure (complete)
- LLVM backend (complete)

## Success Criteria

- All 906+ existing tests pass
- Benchmark suite shows measurable improvements
- Compile time increase < 10%
- Debug info preserved through optimizations
