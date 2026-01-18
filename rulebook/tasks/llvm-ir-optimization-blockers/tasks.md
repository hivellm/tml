# Tasks: LLVM IR Optimization Blockers

**Status**: In Progress (~90%)

## Phase 1: Pointer-Based Object Representation

### 1.1 MIR Object Representation
- [ ] 1.1.1 Add `StackAllocInst` instruction type to MIR for class instances
- [x] 1.1.2 Change `StructExpr` codegen to emit `alloca` + stores
- [ ] 1.1.3 Store object base pointer in variable map instead of aggregate value
- [x] 1.1.4 Update `build_field` to emit `getelementptr` + `load`
- [x] 1.1.5 Update `build_struct_expr` to write fields via `getelementptr` + `store`
- [x] 1.1.6 Add tests for pointer-based struct representation

### 1.2 Phi Node Elimination for Aggregates
- [x] 1.2.1 Detect aggregate-typed phi nodes in `hir_mir_builder.cpp`
- [x] 1.2.2 Use pointer to stack slot instead of value phi at merge points
- [x] 1.2.3 Emit stores to stack slot before branch, load after merge
- [x] 1.2.4 Add pattern: `phi %struct.T` -> `alloca` + `store` + `load`
- [x] 1.2.5 Handle nested control flow (if inside if)
- [x] 1.2.6 Add tests for phi elimination

### 1.3 Method Call Convention
- [x] 1.3.1 Change method signatures to use `ptr %this`
- [x] 1.3.2 Update method call sites to pass pointer
- [x] 1.3.3 Update method body to access fields via `getelementptr` + `load`
- [x] 1.3.4 Add `sret` parameter for methods returning large structs
- [x] 1.3.5 Verify LLVM's SROA can break down stack-allocated structs
- [x] 1.3.6 Add tests for method calling convention

### 1.4 Codegen Updates
- [x] 1.4.1 Update `LLVMIRGen::visit_struct_expr` for pointer semantics
- [x] 1.4.2 Update `LLVMIRGen::visit_field_expr` for pointer semantics
- [x] 1.4.3 Update `LLVMIRGen::visit_method_call` for pointer semantics
- [x] 1.4.4 Add helper `emit_object_alloca(type)` in codegen
- [x] 1.4.5 Add helper `emit_field_gep(obj_ptr, field_idx)` in codegen
- [ ] 1.4.6 Verify IR output matches Rust patterns

## Phase 2: Trivial Drop Elimination

### 2.1 Drop Call Elimination
- [x] 2.1.1 Skip all `drop_` function calls in MIR codegen
- [x] 2.1.2 Verify all tests pass

### 2.2 Future Work (when custom destructors added)
- [ ] 2.2.1 Add `is_trivially_destructible` flag to `TypeInfo` struct
- [ ] 2.2.2 Detect types containing only primitives
- [ ] 2.2.3 Only skip drops for trivially destructible types
- [ ] 2.2.4 Mark built-in types as trivially destructible
- [ ] 2.2.5 Handle class hierarchy (all ancestors must be trivial)
- [ ] 2.2.6 Add query `TypeEnv::is_trivially_destructible(type)`

## Phase 3: Standard Loop Form

### 3.1 Stacksave/Stackrestore Removal
- [x] 3.1.1 Remove `@llvm.stacksave()` from `gen_loop()`
- [x] 3.1.2 Remove `@llvm.stackrestore()` from `gen_loop()` back-edge
- [x] 3.1.3 Remove stacksave/stackrestore from `gen_while()`
- [x] 3.1.4 Remove stacksave/stackrestore from `gen_for()`
- [x] 3.1.5 Verify all tests pass

### 3.2 Loop Structure Analysis
- [x] 3.2.1 Document current loop codegen pattern
- [x] 3.2.2 Identify continuation block proliferation causes
- [x] 3.2.3 Map TML loop constructs to canonical LLVM loop form
- [ ] 3.2.4 Design refactored loop codegen structure

### 3.3 Canonical Loop Generation
- [ ] 3.3.1 Generate `preheader` block for loop initialization
- [ ] 3.3.2 Generate `header` block with single phi for induction variable
- [ ] 3.3.3 Generate `body` block for loop body
- [ ] 3.3.4 Generate `latch` block for increment and backedge
- [ ] 3.3.5 Generate `exit` block for loop termination
- [ ] 3.3.6 Connect blocks with proper CFG edges

### 3.4 Loop Metadata
- [x] 3.4.1 Add `!llvm.loop` metadata to loop back-edges
- [x] 3.4.2 Add unroll hints for small constant bounds
- [x] 3.4.3 Add vectorization hints for numeric loops
- [x] 3.4.4 Verify LLVM recognizes loop structure

## Phase 4: LLVM Lifetime Intrinsics

### 4.1 Lifetime Emission
- [x] 4.1.1 Add `llvm.lifetime.start` after `alloca` instructions
- [x] 4.1.2 Add `llvm.lifetime.end` before scope exit
- [x] 4.1.3 Calculate object size for lifetime intrinsics
- [ ] 4.1.4 Handle conditional scope exit (if, when)
- [ ] 4.1.5 Handle loop scope (lifetime per iteration)

### 4.2 Scope Tracking
- [x] 4.2.1 Track stack allocations per scope
- [x] 4.2.2 Emit lifetime.end at scope exit
- [ ] 4.2.3 Handle early return
- [ ] 4.2.4 Handle break/continue
- [x] 4.2.5 Add tests for lifetime intrinsic placement

## Phase 5: Bug Fixes

### 5.1 Dangling Pointer in Class Factory Methods
- [x] 5.1.1 Detect return context in `gen_struct_expr_ptr`
- [x] 5.1.2 Return value classes by value instead of pointer
- [x] 5.1.3 Update constructor codegen to return struct by value for @value classes
- [x] 5.1.4 Update function codegen to return value classes by value
- [x] 5.1.5 Fix method argument passing for value class arguments
- [x] 5.1.6 Add tests for factory method memory safety

### 5.2 Class Struct Literal Double Indirection
- [x] 5.2.1 Fix `gen_let_stmt` to handle class struct literals correctly
- [x] 5.2.2 Store alloca pointer directly in `locals_` for class struct literals
- [x] 5.2.3 Set correct `%class.X` type in locals instead of `"ptr"`
- [x] 5.2.4 Verify field access works correctly

### 5.3 MIR Codegen Intrinsics
- [x] 5.3.1 Fix CastInst type tracking for intrinsic return values
- [x] 5.3.2 Fix CallInst/MethodCallInst return type registration
- [x] 5.3.3 Fix primitive method value passing

### 5.4 Test Runner
- [x] 5.4.1 Fix BAD_STACK crash (SEH + setjmp conflict)
- [x] 5.4.2 Workaround assert symbol shadowing in runtime tests

### 5.5 SimplifyCfgPass PHI Node Handling
- [x] 5.5.1 Fix PHI updates in `remove_empty_blocks` to use predecessors correctly
- [x] 5.5.2 Fix PHI cleanup in `remove_unreachable_blocks`
- [x] 5.5.3 Fix PHI cleanup in `simplify_constant_branches`

## Phase 6: Validation and Benchmarking

### 6.1 IR Quality Verification
- [ ] 6.1.1 Compare TML IR with Rust IR for same benchmark
- [x] 6.1.2 Verify SROA successfully breaks down structs
- [x] 6.1.3 Verify constant propagation through methods
- [x] 6.1.4 Verify DCE eliminates unused computations
- [x] 6.1.5 Check loop optimization diagnostics

### 6.2 Performance Benchmarks
- [x] 6.2.1 Run OOP benchmark suite before changes
- [x] 6.2.2 Run OOP benchmark suite after Phase 2-3
- [ ] 6.2.3 Compare with Rust and C++ baselines
- [ ] 6.2.4 Document performance improvements per phase
- [ ] 6.2.5 Target: Virtual Dispatch < 400 us (within 2x of Rust)

### 6.3 Regression Testing
- [x] 6.3.1 Run full test suite after Phase 2
- [x] 6.3.2 Run full test suite after Phase 3
- [ ] 6.3.3 Add new tests for edge cases discovered
- [ ] 6.3.4 Test with AddressSanitizer for memory errors

## Summary

| Phase | Description | Progress |
|-------|-------------|----------|
| 1 | Pointer-Based Objects | 21/24 |
| 2 | Trivial Drop Elimination | 2/8 |
| 3 | Standard Loop Form | 12/19 |
| 4 | Lifetime Intrinsics | 7/10 |
| 5 | Bug Fixes | 17/17 |
| 6 | Validation & Benchmarks | 8/14 |
| **Total** | | **67/92** |
