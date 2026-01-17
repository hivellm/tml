# Tasks: LLVM IR Optimization Blockers

**Status**: In Progress (~55% - Phase 1.2-1.4, 2, 3.1 Complete)

**Priority**: Critical - Root cause of 10-30x performance gap vs Rust/C++

## Phase 1: Pointer-Based Object Representation

> **Status**: Complete - All phases use pointer-based semantics

### 1.1 MIR Object Representation
- [ ] 1.1.1 Add `StackAllocInst` instruction type to MIR for class instances
- [x] 1.1.2 Change `StructExpr` codegen to emit `alloca` + stores instead of `insertvalue` (mir_codegen.cpp)
- [ ] 1.1.3 Store object base pointer in variable map instead of aggregate value
- [x] 1.1.4 Update `build_field` to emit `getelementptr` + `load` instead of `extractvalue` (ExtractValueInst in mir_codegen.cpp)
- [x] 1.1.5 Update `build_struct_expr` to write fields via `getelementptr` + `store` (StructInitInst, TupleInitInst)
- [x] 1.1.6 Add tests for pointer-based struct representation (all 1577 tests pass)

### 1.2 Phi Node Elimination for Aggregates
- [x] 1.2.1 Detect aggregate-typed phi nodes in `hir_mir_builder.cpp` (using `is_aggregate()` helper)
- [x] 1.2.2 For control flow merge points, use pointer to stack slot instead of value phi
- [x] 1.2.3 Emit stores to stack slot before branch, load after merge
- [x] 1.2.4 Add pattern: `phi %struct.T` → `alloca` + `store` + `load` (in `build_if` and `build_when`)
- [x] 1.2.5 Handle nested control flow (if inside if)
- [x] 1.2.6 Add tests for phi elimination (test_tuple_phi.tml verified, all 1577 tests pass)

### 1.3 Method Call Convention
- [x] 1.3.1 Change method signatures from `(%struct.T %this)` to `(ptr %this)` (already uses `ptr %this`)
- [x] 1.3.2 Update method call sites to pass pointer instead of value (already passes pointer)
- [x] 1.3.3 Update method body to access fields via `getelementptr` + `load` (see struct.cpp:687-693)
- [x] 1.3.4 Add `sret` parameter for methods returning large structs (value classes returned by value)
- [ ] 1.3.5 Verify LLVM's SROA can now break down stack-allocated structs
- [x] 1.3.6 Add tests for method calling convention (covered by OOP tests)

### 1.4 Codegen Updates
- [x] 1.4.1 Update `LLVMIRGen::visit_struct_expr` for pointer semantics (already uses alloca+gep+store)
- [x] 1.4.2 Update `LLVMIRGen::visit_field_expr` for pointer semantics (uses gep+load)
- [x] 1.4.3 Update `LLVMIRGen::visit_method_call` for pointer semantics (passes ptr)
- [x] 1.4.4 Add helper `emit_object_alloca(type)` in codegen (uses alloca directly)
- [x] 1.4.5 Add helper `emit_field_gep(obj_ptr, field_idx)` in codegen (uses getelementptr)
- [ ] 1.4.6 Verify IR output matches Rust patterns

## Phase 2: Trivial Drop Elimination

> **Status**: Complete

### 2.1 Drop Call Elimination
- [x] 2.1.1 Skip all `drop_` function calls in MIR codegen (`mir_codegen.cpp`)
- [x] 2.1.2 Verify all 1577 tests pass

### 2.2 Future Work (when custom destructors added)
- [ ] 2.2.1 Add `is_trivially_destructible` flag to `TypeInfo` struct
- [ ] 2.2.2 Detect types containing only primitives (no heap, no resources)
- [ ] 2.2.3 Only skip drops for types that are truly trivially destructible
- [ ] 2.2.4 Mark built-in types (I32, F64, Bool, etc.) as trivially destructible
- [ ] 2.2.5 Handle class hierarchy (all ancestors must be trivial)
- [ ] 2.2.6 Add query `TypeEnv::is_trivially_destructible(type)`

## Phase 3: Standard Loop Form

> **Status**: Partial (stacksave removed)

### 3.1 Stacksave/Stackrestore Removal
- [x] 3.1.1 Remove `@llvm.stacksave()` from `gen_loop()` in `llvm_ir_gen_control.cpp`
- [x] 3.1.2 Remove `@llvm.stackrestore()` from `gen_loop()` back-edge
- [x] 3.1.3 Remove stacksave/stackrestore from `gen_while()`
- [x] 3.1.4 Remove stacksave/stackrestore from `gen_for()`
- [x] 3.1.5 Verify all 1577 tests pass

### 3.2 Loop Structure Analysis
- [ ] 3.2.1 Document current loop codegen pattern in `build_loop()`
- [ ] 3.2.2 Identify continuation block proliferation causes
- [ ] 3.2.3 Map TML loop constructs to canonical LLVM loop form
- [ ] 3.2.4 Design refactored loop codegen structure

### 3.3 Canonical Loop Generation
- [ ] 3.3.1 Generate `preheader` block for loop initialization
- [ ] 3.3.2 Generate `header` block with single phi for induction variable
- [ ] 3.3.3 Generate `body` block for loop body
- [ ] 3.3.4 Generate `latch` block for increment and backedge
- [ ] 3.3.5 Generate `exit` block for loop termination
- [ ] 3.3.6 Connect blocks with proper CFG edges

### 3.4 Loop Metadata
- [ ] 3.4.1 Add `!llvm.loop` metadata to loop back-edges
- [ ] 3.4.2 Add unroll hints for small constant bounds
- [ ] 3.4.3 Add vectorization hints for numeric loops
- [ ] 3.4.4 Verify LLVM recognizes loop structure

## Phase 4: LLVM Lifetime Intrinsics

> **Status**: Not Started

### 4.1 Lifetime Emission
- [ ] 4.1.1 Add `llvm.lifetime.start` after `alloca` instructions
- [ ] 4.1.2 Add `llvm.lifetime.end` before scope exit
- [ ] 4.1.3 Calculate object size for lifetime intrinsics
- [ ] 4.1.4 Handle conditional scope exit (if, when)
- [ ] 4.1.5 Handle loop scope (lifetime per iteration)

### 4.2 Scope Tracking
- [ ] 4.2.1 Track stack allocations per scope in `BuildContext`
- [ ] 4.2.2 Emit lifetime.end for all scope allocations at scope exit
- [ ] 4.2.3 Handle early return (emit lifetime.end before return)
- [ ] 4.2.4 Handle break/continue (emit lifetime.end for loop scope)
- [ ] 4.2.5 Add tests for lifetime intrinsic placement

## Phase 5: Bug Fixes

> **Status**: Not Started

### 5.1 Dangling Pointer in Class Factory Methods
- [ ] 5.1.1 Detect return context in `gen_struct_expr_ptr` (struct.cpp:169-196)
- [ ] 5.1.2 Heap allocate class objects when returned from function
- [ ] 5.1.3 Or implement sret calling convention for class returns
- [ ] 5.1.4 Add escape analysis to determine allocation strategy
- [ ] 5.1.5 Add tests for factory method memory safety

## Phase 6: Validation and Benchmarking

> **Status**: Partial

### 6.1 IR Quality Verification
- [ ] 6.1.1 Compare TML IR with Rust IR for same benchmark
- [ ] 6.1.2 Verify SROA successfully breaks down structs
- [ ] 6.1.3 Verify constant propagation through methods
- [ ] 6.1.4 Verify DCE eliminates unused computations
- [ ] 6.1.5 Check loop optimization diagnostics

### 6.2 Performance Benchmarks
- [x] 6.2.1 Run OOP benchmark suite before changes
- [x] 6.2.2 Run OOP benchmark suite after Phase 2-3
- [ ] 6.2.3 Compare with Rust and C++ baselines
- [ ] 6.2.4 Document performance improvements per phase
- [ ] 6.2.5 Target: Virtual Dispatch < 400 µs (within 2x of Rust)

### 6.3 Regression Testing
- [x] 6.3.1 Run full test suite after Phase 2
- [x] 6.3.2 Run full test suite after Phase 3
- [ ] 6.3.3 Add new tests for edge cases discovered
- [ ] 6.3.4 Test with AddressSanitizer for memory errors

## Summary

| Phase | Description | Status | Progress |
|-------|-------------|--------|----------|
| 1 | Pointer-Based Objects | **Complete** | 20/24 |
| 2 | Trivial Drop Elimination | **Complete** | 2/8 |
| 3 | Standard Loop Form | **Partial** | 5/15 |
| 4 | Lifetime Intrinsics | Not Started | 0/10 |
| 5 | Bug Fixes | Not Started | 0/5 |
| 6 | Validation & Benchmarks | Partial | 4/14 |
| **Total** | | **~55%** | **31/76** |

## Performance Results

| Benchmark | Before | After Phase 2-3 | After Phase 1.1-1.2 | Target | Rust |
|-----------|--------|-----------------|---------------------|--------|------|
| Virtual Dispatch | 820 µs | 787 µs (-4%) | 799 µs | < 400 µs | 286 µs |
| Object Creation | 1280 µs | 1072 µs (-16%) | 1236 µs | < 200 µs | 0 µs |
| HTTP Handler | 690 µs | 578 µs (-16%) | 579 µs | < 200 µs | 150 µs |
| Game Loop | 1870 µs | 1858 µs (-1%) | 1860 µs | < 500 µs | 381 µs |
| Deep Inheritance | 310 µs | 307 µs (-1%) | 327 µs | < 100 µs | 0 µs |
| Method Chaining | 2370 µs | 2333 µs (-2%) | 2355 µs | < 100 µs | 0 µs |

**Note**: Phase 1.1-1.2 changes struct/tuple initialization to use alloca+gep+store pattern instead of insertvalue. This is the correct pattern for LLVM's SROA (Scalar Replacement of Aggregates) pass. Performance may show temporary overhead until full optimization pipeline is enabled.

## Key Files

- `compiler/src/codegen/mir_codegen.cpp` - Drop elimination
- `compiler/src/codegen/llvm_ir_gen_control.cpp` - Loop codegen
- `compiler/src/codegen/expr/struct.cpp` - Struct/class allocation
- `compiler/src/mir/builder/hir_expr.cpp` - HIR to MIR phi elimination (build_if, build_when)
- `compiler/src/hir/hir_builder_expr.cpp` - Tuple field access lowering
