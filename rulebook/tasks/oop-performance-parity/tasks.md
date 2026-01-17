# Tasks: OOP Performance Parity with C++

**Status**: In Progress (10%)

**Priority**: High - Performance critical for OOP adoption

## Phase 1: Stack Promotion for Non-Escaping Objects

### 1.1 Escape Analysis Enhancement
- [ ] 1.1.1 Define `EscapeState` enum (NoEscape, MayEscape, Escapes)
- [ ] 1.1.2 Track object lifetime through all uses in `escape_analysis.cpp`
- [ ] 1.1.3 Identify objects stored to heap/global as escaping
- [ ] 1.1.4 Identify objects returned from function as escaping
- [ ] 1.1.5 Identify objects passed to unknown functions as MayEscape
- [ ] 1.1.6 Mark objects only used locally as NoEscape
- [ ] 1.1.7 Handle sealed classes with special fast-path analysis
- [ ] 1.1.8 Add `@stack_eligible` attribute to MIR allocations
- [ ] 1.1.9 Add unit tests for escape analysis

### 1.2 Stack Promotion Pass
- [ ] 1.2.1 Create `stack_promotion.hpp` header with StackPromotionPass declaration
- [ ] 1.2.2 Create `stack_promotion.cpp` implementing the pass
- [ ] 1.2.3 Transform `AllocInst` to `StackAllocInst` for non-escaping objects
- [ ] 1.2.4 Remove corresponding `tml_free` calls for promoted objects
- [ ] 1.2.5 Handle objects with destructors (call at scope end)
- [ ] 1.2.6 Handle conditional allocations (same stack slot for branches)
- [ ] 1.2.7 Handle loop allocations (per-iteration analysis)
- [ ] 1.2.8 Add pass to standard pipeline after escape analysis
- [ ] 1.2.9 Add tests for stack promotion

### 1.3 Codegen for Stack Allocation
- [ ] 1.3.1 Add codegen path in `class_codegen.cpp` for `StackAllocInst`
- [ ] 1.3.2 Generate `alloca` instead of `call @tml_alloc` for stack objects
- [ ] 1.3.3 Initialize vtable pointer directly on stack object
- [ ] 1.3.4 Ensure proper alignment (8-byte for class objects)
- [ ] 1.3.5 Generate destructor calls at all return paths
- [ ] 1.3.6 Add tests for stack-allocated class IR generation

### 1.4 Enable LLVM SROA
- [ ] 1.4.1 Add SROA pass to LLVM optimization pipeline in `llvm_ir_gen.cpp`
- [ ] 1.4.2 Configure SROA thresholds for aggressive scalar replacement
- [ ] 1.4.3 Verify stack-allocated structs get broken into registers
- [ ] 1.4.4 Add tests verifying scalar replacement in LLVM IR

## Phase 2: Aggressive Devirtualization

### 2.1 Type-Based Devirtualization
- [ ] 2.1.1 Track concrete types through data flow in `devirtualization.cpp`
- [ ] 2.1.2 Devirtualize direct instantiation: `let obj = Circle::new(5.0)`
- [ ] 2.1.3 Devirtualize after type narrowing in `when` expressions
- [ ] 2.1.4 Devirtualize sealed class with single implementation
- [ ] 2.1.5 Replace virtual `MethodCallInst` with direct `CallInst`
- [ ] 2.1.6 Add tests for type-based devirtualization

### 2.2 Inline Devirtualized Methods
- [ ] 2.2.1 Prioritize inlining of devirtualized methods in `inlining.cpp`
- [ ] 2.2.2 Lower inlining threshold for methods < 10 instructions
- [ ] 2.2.3 Always inline single-expression methods (getters/setters)
- [ ] 2.2.4 Handle recursive inlining for method chains
- [ ] 2.2.5 Add tests for devirtualized method inlining

### 2.3 Speculative Devirtualization
- [ ] 2.3.1 Implement type profiling during compilation
- [ ] 2.3.2 Generate guarded devirtualization code
- [ ] 2.3.3 Add heuristics for when speculation is profitable
- [ ] 2.3.4 Emit profile data for hot virtual call sites
- [ ] 2.3.5 Add tests for speculative devirtualization

## Phase 3: Method Chaining Optimization

### 3.1 Builder Pattern Detection
- [ ] 3.1.1 Create `builder_opt.hpp` header
- [ ] 3.1.2 Create `builder_opt.cpp` implementing BuilderOptPass
- [ ] 3.1.3 Detect methods returning `self` type
- [ ] 3.1.4 Track chains of method calls on same object
- [ ] 3.1.5 Mark intermediate objects for elimination
- [ ] 3.1.6 Handle mutable and immutable builders
- [ ] 3.1.7 Add tests for builder pattern detection

### 3.2 Copy Elision
- [ ] 3.2.1 Implement return value optimization (RVO) in `class_codegen.cpp`
- [ ] 3.2.2 Implement named return value optimization (NRVO)
- [ ] 3.2.3 Avoid unnecessary copies when returning objects
- [ ] 3.2.4 Handle chained method returns
- [ ] 3.2.5 Add tests for copy elision

### 3.3 Intermediate Object Elimination
- [ ] 3.3.1 Detect intermediate objects used only in chain
- [ ] 3.3.2 Replace chain with direct field mutations
- [ ] 3.3.3 Handle immutable builder pattern (create, mutate, freeze)
- [ ] 3.3.4 Add tests for intermediate object elimination

## Phase 4: Constructor/Destructor Optimization

### 4.1 Constructor Fusion Enhancement
- [ ] 4.1.1 Detect super() call chains in constructors in `constructor_fusion.cpp`
- [ ] 4.1.2 Fuse initialization across inheritance hierarchy
- [ ] 4.1.3 Eliminate redundant field zeroing
- [ ] 4.1.4 Combine field assignments into single memset/store
- [ ] 4.1.5 Add tests for constructor fusion

### 4.2 Destructor Batching
- [ ] 4.2.1 Create `destructor_batch.hpp` header
- [ ] 4.2.2 Create `destructor_batch.cpp` implementing DestructorBatchPass
- [ ] 4.2.3 Identify objects destroyed at same scope exit
- [ ] 4.2.4 Batch destructor calls to reduce call overhead
- [ ] 4.2.5 Optimize destructor chains for inheritance
- [ ] 4.2.6 Add tests for destructor batching

### 4.3 Zero Initialization Elimination
- [ ] 4.3.1 Track which fields are written before read
- [ ] 4.3.2 Eliminate zero initialization for immediately-written fields
- [ ] 4.3.3 Use LLVM's `undef` for uninitialized memory where safe
- [ ] 4.3.4 Add tests for zero initialization elimination

## Phase 5: LLVM Optimization Tuning

### 5.1 Inlining Thresholds
- [ ] 5.1.1 Lower inlining cost threshold for methods in `llvm_ir_gen.cpp`
- [ ] 5.1.2 Always inline methods marked `@inline`
- [ ] 5.1.3 Increase inlining budget for hot paths
- [ ] 5.1.4 Configure PassManagerBuilder for aggressive inlining
- [ ] 5.1.5 Add tests verifying small methods are inlined

### 5.2 Loop Optimization
- [ ] 5.2.1 Enable loop unrolling for small iteration counts
- [ ] 5.2.2 Add loop vectorization hints
- [ ] 5.2.3 Configure LICM (Loop Invariant Code Motion)
- [ ] 5.2.4 Add tests for loop optimizations

### 5.3 Constant Propagation
- [ ] 5.3.1 Propagate constants through virtual calls
- [ ] 5.3.2 Fold known method results
- [ ] 5.3.3 Eliminate dead code after constant folding
- [ ] 5.3.4 Add tests for constant propagation

## Phase 6: Validation and Benchmarking

### 6.1 Benchmark Suite
- [ ] 6.1.1 Add automated benchmark comparison (TML vs C++)
- [ ] 6.1.2 Create CI performance regression tests
- [ ] 6.1.3 Add benchmark report generation
- [ ] 6.1.4 Track performance over time

### 6.2 Memory Safety Verification
- [ ] 6.2.1 Verify stack promotion doesn't cause use-after-scope
- [ ] 6.2.2 Test with AddressSanitizer enabled
- [ ] 6.2.3 Verify destructor calls happen correctly
- [ ] 6.2.4 Test with MemorySanitizer for uninitialized reads

### 6.3 Compatibility Testing
- [ ] 6.3.1 Run full test suite after each phase
- [ ] 6.3.2 Test with real-world TML programs
- [ ] 6.3.3 Verify no change in observable behavior
- [ ] 6.3.4 Test edge cases (nested classes, multiple inheritance)

## Summary

| Phase | Description | Status | Progress |
|-------|-------------|--------|----------|
| 1 | Stack Promotion | Not Started | 0/22 |
| 2 | Aggressive Devirtualization | Not Started | 0/16 |
| 3 | Method Chaining Optimization | Not Started | 0/15 |
| 4 | Constructor/Destructor Opt | Not Started | 0/15 |
| 5 | LLVM Optimization Tuning | Not Started | 0/13 |
| 6 | Validation & Benchmarking | Not Started | 0/12 |
| **Total** | | **0%** | **0/93** |

## Performance Targets

| Benchmark        | Current   | Target   | C++ Ref  | Status         |
|------------------|-----------|----------|----------|----------------|
| Object Creation  | 970 us    | <500 us  | 164 us   | 10.7x improved |
| Virtual Dispatch | 749 us    | <150 us  | 58 us    | Needs work     |
| HTTP Handler     | 546 us    | <100 us  | 18 us    | Needs work     |
| Game Loop        | 1796 us   | <2000 us | 1022 us  | TARGET MET     |
| Deep Inheritance | 289 us    | <100 us  | 37 us    | Needs work     |
| Method Chaining  | 2435 us   | <50 us   | ~0 us    | Needs work     |

## Files to Add

### New MIR Pass Files
- `compiler/include/mir/passes/stack_promotion.hpp`
- `compiler/src/mir/passes/stack_promotion.cpp`
- `compiler/include/mir/passes/builder_opt.hpp`
- `compiler/src/mir/passes/builder_opt.cpp`
- `compiler/include/mir/passes/destructor_batch.hpp`
- `compiler/src/mir/passes/destructor_batch.cpp`

### Test Files
- `compiler/tests/stack_promotion_test.cpp`
- `compiler/tests/devirtualization_test.cpp`
- `compiler/tests/builder_opt_test.cpp`

## Files to Modify

### MIR Passes
- `compiler/src/mir/passes/escape_analysis.cpp` - Enhanced escape tracking
- `compiler/src/mir/passes/devirtualization.cpp` - Type-based devirt
- `compiler/src/mir/passes/inlining.cpp` - Method inlining priority
- `compiler/src/mir/passes/constructor_fusion.cpp` - Enhanced fusion
- `compiler/src/mir/mir_pass.cpp` - Pipeline configuration

### Codegen
- `compiler/src/codegen/core/class_codegen.cpp` - Stack allocation support
- `compiler/src/codegen/llvm_ir_gen.cpp` - LLVM pass configuration

### Include Files
- `compiler/include/mir/mir.hpp` - StackAllocInst definition

## Dependencies

### Internal
- Existing escape analysis pass
- Existing devirtualization pass
- Existing inlining pass
- MIR infrastructure

### External
- LLVM optimization passes (SROA, inlining, etc.)
