# Tasks: OOP Optimizations for MIR and HIR

**Status**: Core Complete (95%) - Arenas, SOO, benchmarks remaining

**Note**: Phase 1 (CHA with final methods + unit tests), Phase 2 (Devirtualization including final methods), Phase 3 (Speculative Devirtualization with heuristic-based type frequency hints), Phase 4 (Virtual Call Inlining + tests), Phase 5 (Dead Method Elimination + tests), Phase 6.1 (Vtable Deduplication), Phase 6.2 (Vtable Splitting hot/cold), Phase 6.3 (Interface Vtable Optimization with deduplication), Phase 7 (Escape Analysis + tests), Phase 8.1 (@value directive with validation), Phase 8.2 (Value class codegen - no vtable, direct dispatch), Phase 8.3.1 (Value Class Detection), Phase 9.1 (Pool[T] type implementation in core library), Phase 9.2 (@pool directive validation), and Phase 12.2 (Trivial Destructor Detection with elision) are implemented. Parser supports `sealed` modifier for final methods. The devirtualization pass now lazily builds the class hierarchy in const query methods.
See `compiler/include/mir/passes/devirtualization.hpp`, `compiler/include/mir/passes/inlining.hpp`, `compiler/include/mir/passes/dead_method_elimination.hpp`, `compiler/include/mir/passes/escape_analysis.hpp`, `compiler/src/codegen/core/class_codegen.cpp`, `compiler/include/codegen/llvm_ir_gen.hpp`, `compiler/src/types/env_lookups.cpp`, `compiler/src/types/checker/core.cpp`, `lib/core/src/pool.tml`, and `compiler/tests/oop_test.cpp` for implementations.

## Phase 1: Class Hierarchy Analysis (CHA)

> **Status**: Complete - Implemented in `DevirtualizationPass`

### 1.1 Data Structures
- [x] 1.1.1 Create `ClassHierarchyInfo` struct (in `devirtualization.hpp`)
- [x] 1.1.2 Add parent/child relationship maps (`base_class`, `subclasses`)
- [x] 1.1.3 Add sealed class tracking (`is_sealed` field)
- [x] 1.1.4 Add abstract class tracking (`is_abstract` field)
- [x] 1.1.5 Add final method tracking map (`final_methods` set in ClassHierarchyInfo)

### 1.2 Hierarchy Building
- [x] 1.2.1 Build parent/child graph during type checking (`build_class_hierarchy()`)
- [x] 1.2.2 Track interface implementations per class (`interfaces` field)
- [x] 1.2.3 Compute transitive subclass closure (`compute_transitive_subclasses()`)
- [x] 1.2.4 Build virtual method resolution table (`is_virtual_method()`)

### 1.3 Query Methods
- [x] 1.3.1 Implement `get_parent(class_name)` query (via `base_class`)
- [x] 1.3.2 Implement `get_children(class_name)` query (via `subclasses`)
- [x] 1.3.3 Implement `get_all_subclasses(class_name)` query (via `all_subclasses`)
- [x] 1.3.4 Implement `get_concrete_subclasses(class_name)` query (via `is_abstract` filter)
- [x] 1.3.5 Implement `is_sealed(class_name)` query (`is_sealed_class()`)
- [x] 1.3.6 Implement `is_final_method(class, method)` query (`is_final_method()`)
- [x] 1.3.7 Implement `has_single_implementation(class_name)` query (`get_single_implementation()`)
- [x] 1.3.8 Add unit tests for class hierarchy analysis (11 tests in `oop_test.cpp`)

## Phase 2: MIR Devirtualization

> **Status**: Complete - See `compiler/src/mir/passes/devirtualization.cpp`

### 2.1 Pass Infrastructure
- [x] 2.1.1 Create `DevirtualizationPass` class in MIR passes
- [x] 2.1.2 Register pass in MIR pass manager
- [x] 2.1.3 Enabled by default at optimization levels
- [x] 2.1.4 Add devirtualization statistics counter (`DevirtualizationStats`)

### 2.2 Exact Type Devirtualization
- [x] 2.2.1 Track exact types from constructor calls (leaf class detection)
- [x] 2.2.2 Propagate exact type through SSA values (via `is_leaf()`)
- [x] 2.2.3 Devirtualize calls when exact type known (`DevirtReason::ExactType`)
- [ ] 2.2.4 Handle type narrowing in conditionals (future)

### 2.3 Sealed/Final Devirtualization
- [x] 2.3.1 Query CHA for sealed classes (`is_sealed_class()`)
- [x] 2.3.2 Devirtualize all calls on sealed class types (`DevirtReason::SealedClass`)
- [x] 2.3.3 Query CHA for final methods (`is_final_method()`)
- [x] 2.3.4 Devirtualize final method calls regardless of receiver type (`DevirtReason::FinalMethod`)
- [x] 2.3.5 Handle inherited final methods (checked in parent class chain)

### 2.4 CHA-Based Devirtualization
- [x] 2.4.1 Query CHA for concrete implementations (`get_single_implementation()`)
- [x] 2.4.2 Devirtualize when single implementation exists (`DevirtReason::SingleImpl`)
- [ ] 2.4.3 Handle whole-program analysis mode (future)
- [ ] 2.4.4 Invalidate on dynamic loading (if supported)
- [x] 2.4.5 Add tests for all devirtualization cases (in OOP test suite)

## Phase 3: Speculative Devirtualization

> **Status**: Complete (heuristic-based) - See `compiler/src/codegen/core/class_codegen.cpp`

### 3.1 Type Frequency Hints (Heuristic-Based)
- [x] 3.1.1 Initialize type frequency hints from class hierarchy (`init_type_frequency_hints()`)
- [x] 3.1.2 High frequency for sealed classes (95%)
- [x] 3.1.3 High frequency for leaf classes (85%)
- [x] 3.1.4 Zero frequency for abstract classes
- [ ] 3.1.5 (Future) Design profile-guided type data format
- [ ] 3.1.6 (Future) Implement type profile instrumentation pass

### 3.2 Speculative Transform
- [x] 3.2.1 Analyze speculative devirt profitability (`analyze_spec_devirt()`)
- [x] 3.2.2 Generate type ID comparison (vtable pointer comparison)
- [x] 3.2.3 Generate fast path (direct call) (`gen_guarded_virtual_call()`)
- [x] 3.2.4 Generate slow path (vtable dispatch)
- [x] 3.2.5 Generate phi node to merge results
- [x] 3.2.6 Track speculative devirt statistics (`SpecDevirtStats`)

## Phase 4: Virtual Call Inlining

> **Status**: Complete - See `compiler/src/mir/passes/inlining.cpp` and tests in `oop_test.cpp`

### 4.1 Inlining Extension
- [x] 4.1.1 Extend inlining pass to recognize devirtualized calls (`DevirtInfo` in `CallInst`)
- [x] 4.1.2 Compute inlining benefit for virtual methods (threshold bonus + vtable overhead savings)
- [x] 4.1.3 Handle recursive virtual calls (tracked via `inline_depth_`)
- [x] 4.1.4 Respect `@noinline` on virtual methods (`has_noinline_attr()`)

### 4.2 Constructor Inlining
- [x] 4.2.1 Prioritize constructor inlining (`is_constructor_name()` + `constructor_bonus`)
- [x] 4.2.2 Inline base constructor chains (`is_base_constructor_call()` + `base_constructor_bonus`)
- [ ] 4.2.3 Fuse constructor initialization stores (future - requires separate pass)
- [ ] 4.2.4 Eliminate redundant vtable pointer stores (future - requires separate pass)
- [x] 4.2.5 Add tests for virtual call inlining (7 tests in `oop_test.cpp`)

## Phase 5: Dead Virtual Method Elimination

> **Status**: Complete - See `compiler/src/mir/passes/dead_method_elimination.cpp` and tests in `oop_test.cpp`

### 5.1 Reachability Analysis
- [x] 5.1.1 Build virtual call graph (`build_call_graph()`)
- [x] 5.1.2 Mark reachable virtual methods from entry points (`propagate_reachability()`)
- [x] 5.1.3 Handle reflection/dynamic dispatch (conservatively keep interface impls)
- [x] 5.1.4 Track interface method reachability (`is_entry_point()` for interface methods)

### 5.2 Elimination Transform
- [x] 5.2.1 Mark unreachable methods for removal (`get_dead_methods()`)
- [x] 5.2.2 Remove method implementations from codegen (replaced with UnreachableTerm)
- [x] 5.2.3 Replace vtable entries with null/trap (body replaced with unreachable)
- [x] 5.2.4 Track elimination statistics (`DeadMethodStats`)
- [x] 5.2.5 Add tests for dead method elimination (6 tests in `oop_test.cpp`)

## Phase 6: Vtable Optimization

> **Status**: Partially Complete - See `compiler/src/codegen/core/class_codegen.cpp`

### 6.1 Vtable Deduplication
- [x] 6.1.1 Hash vtable contents (`compute_vtable_content_key()`)
- [x] 6.1.2 Identify identical vtables across classes (`vtable_content_to_name_` map)
- [x] 6.1.3 Share vtable global for identical layouts (emit alias instead of duplicate)
- [x] 6.1.4 Update class type info pointers (`class_to_shared_vtable_` tracking)
- [x] 6.1.5 Track deduplication statistics (`VtableDeduplicationStats`)

### 6.2 Vtable Splitting

> **Status**: Complete - See `compiler/src/codegen/core/class_codegen.cpp`

- [x] 6.2.1 Identify hot/cold methods via heuristics and `@hot`/`@cold` decorators (`analyze_vtable_split()`)
- [x] 6.2.2 Place hot methods in primary vtable (accessor patterns: get*, set*, is*, has*, etc.)
- [x] 6.2.3 Place cold methods in secondary vtable (destructors, rarely called methods)
- [x] 6.2.4 Generate split vtable types and globals (`gen_split_vtables()`)
- [x] 6.2.5 Get correct vtable index for dispatch (`get_split_vtable_index()`)
- [x] 6.2.6 Track split statistics (`VtableSplitStats`)

### 6.3 Interface Vtable Optimization

> **Status**: Complete - See `compiler/src/codegen/core/class_codegen.cpp`

- [x] 6.3.1 Compute interface vtable content key (`compute_interface_vtable_key()`)
- [x] 6.3.2 Deduplicate identical interface vtables (emit alias instead of duplicate)
- [x] 6.3.3 Track interface vtable statistics (`InterfaceVtableStats`)
- [ ] 6.3.4 (Future) Remove gaps from sparse interface layouts

## Phase 7: Escape Analysis for Classes

> **Status**: Mostly Complete - See `compiler/src/mir/passes/escape_analysis.cpp`

### 7.1 Analysis Extension
- [x] 7.1.1 Basic escape analysis infrastructure exists (`EscapeAnalysisPass`)
- [x] 7.1.2 Extend for class instance tracking via `this` parameter (`track_this_parameter()`)
- [x] 7.1.3 Track escape through method return values (`analyze_method_call()`)
- [x] 7.1.4 Handle escape through field stores (`analyze_gep()`, `analyze_store()`)

### 7.2 Inheritance Handling
- [x] 7.2.1 Analyze base class field escapes (via GEP tracking)
- [x] 7.2.2 Handle virtual method calls conservatively (`GlobalEscape` for MethodCallInst)
- [x] 7.2.3 Refine with devirtualization results (`devirt_info` in CallInst)
- [ ] 7.2.4 Track conditional escapes (future)

### 7.3 Stack Allocation Transform
- [x] 7.3.1 `StackPromotionPass` exists for stack promotion
- [ ] 7.3.2 Extend to remove corresponding free calls for classes (future)
- [ ] 7.3.3 Adjust vtable pointer initialization for stack objects (future)
- [x] 7.3.4 Track stack allocation statistics (`StackPromotionPass::Stats`)
- [x] 7.3.5 Add tests for class escape analysis (7 tests in `oop_test.cpp`)

## Phase 8: Value Classes

### 8.1 Directive Support

> **Status**: Complete - See `compiler/src/types/checker/core.cpp`

- [x] 8.1.1 Add `@value` directive to parser (existing decorator parser handles it)
- [x] 8.1.2 Validate: no virtual methods allowed (`validate_value_class()`)
- [x] 8.1.3 Validate: can only extend other @value classes (`validate_inheritance()`)
- [x] 8.1.4 Validate: implicitly sealed (set in `register_class_decl()`)
- [x] 8.1.5 Allow interface implementation (no restrictions)

### 8.2 Code Generation

> **Status**: Partially Complete - See `compiler/src/codegen/core/class_codegen.cpp`

- [x] 8.2.1 Generate struct layout without vtable pointer (skip vtable ptr for @value)
- [ ] 8.2.2 Generate pass-by-value semantics (future: stack allocation by default)
- [ ] 8.2.3 Generate copy operations for assignment (future)
- [x] 8.2.4 Direct dispatch for method calls (no vtable lookup for @value classes)

### 8.3 Auto-Inference

> **Status**: Partially Complete - See `compiler/src/types/env_lookups.cpp`

- [x] 8.3.1 Detect sealed classes with no virtual methods (`is_value_class_candidate()`)
- [ ] 8.3.2 Auto-apply value class optimization (requires codegen changes)
- [ ] 8.3.3 Warn when @value could be applied
- [x] 8.3.4 Add tests for value classes (6 tests in `oop_test.cpp`)

## Phase 9: Object Pooling

### 9.1 Pool Type Implementation

> **Status**: Complete - See `lib/core/src/pool.tml`

- [x] 9.1.1 Design `Pool[T]` API in core library (GrowthPolicy enum, PoolStats struct)
- [x] 9.1.2 Implement acquire() method (lock-free pop from free list)
- [x] 9.1.3 Implement release() method (lock-free push to free list)
- [x] 9.1.4 Implement lock-free free list using atomic CAS operations
- [x] 9.1.5 Implement growth policy (Fixed, Doubling, Linear)
- [x] 9.1.6 Implement pool statistics tracking (hits, misses, grows)
- [x] 9.1.7 Implement memory block tracking for cleanup
- [x] 9.1.8 Implement Drop behavior for Pool[T]

### 9.2 Directive Support

> **Status**: Validation Complete - See `compiler/src/types/checker/core.cpp`

- [x] 9.2.1 Add `@pool` directive to parser (existing decorator parser handles it)
- [x] 9.2.2 Validate: @pool and @value are mutually exclusive (`validate_pool_class()`)
- [x] 9.2.3 Validate: @pool classes cannot be abstract (`validate_pool_class()`)
- [x] 9.2.4 Add tests for @pool validation (5 tests in `oop_test.cpp`)
- [ ] 9.2.5 Parse pool parameters (size, grow, thread_local)
- [ ] 9.2.6 Generate global pool instance
- [ ] 9.2.7 Transform `new()` to `acquire()`
- [ ] 9.2.8 Transform `drop()` to `release()`

### 9.3 Thread-Local Pools
- [ ] 9.3.1 Implement thread-local pool storage
- [ ] 9.3.2 Handle thread termination cleanup
- [ ] 9.3.3 Implement pool statistics per thread
- [ ] 9.3.4 Add tests for object pooling

## Phase 10: Arena Allocators

### 10.1 Arena Type Implementation
- [ ] 10.1.1 Design `Arena` API in core library
- [ ] 10.1.2 Implement bump pointer allocation
- [ ] 10.1.3 Implement `alloc[T]()` method
- [ ] 10.1.4 Implement `reset()` method
- [ ] 10.1.5 Implement `reset_with_dtors()` method

### 10.2 Nested Arenas
- [ ] 10.2.1 Implement parent/child arena relationship
- [ ] 10.2.2 Enforce child reset before parent
- [ ] 10.2.3 Handle arena scope with defer
- [ ] 10.2.4 Track arena memory usage

### 10.3 Codegen Integration
- [ ] 10.3.1 Detect arena allocation context
- [ ] 10.3.2 Generate bump pointer increment
- [ ] 10.3.3 Skip destructor generation for arena objects
- [ ] 10.3.4 Add tests for arena allocation

## Phase 11: Small Object Optimization (SOO)

### 11.1 Size Calculation
- [ ] 11.1.1 Calculate class size at compile time
- [ ] 11.1.2 Include vtable pointer in size
- [ ] 11.1.3 Include inherited fields in size
- [ ] 11.1.4 Define SOO threshold (64 bytes default)

### 11.2 Inline Storage Transform
- [ ] 11.2.1 Detect small class fields
- [ ] 11.2.2 Generate inline storage layout
- [ ] 11.2.3 Handle vtable pointer for inline objects
- [ ] 11.2.4 Generate copy semantics for inline

### 11.3 Container Optimization
- [ ] 11.3.1 Apply SOO to Maybe[T] with small T
- [ ] 11.3.2 Apply SOO to Outcome[T, E] with small types
- [ ] 11.3.3 Track SOO statistics
- [ ] 11.3.4 Add tests for small object optimization

## Phase 12: Destructor Optimization

### 12.1 Loop Hoisting
- [ ] 12.1.1 Detect loop-local object allocations (future)
- [ ] 12.1.2 Verify object doesn't escape loop (future)
- [ ] 12.1.3 Hoist allocation before loop (future)
- [ ] 12.1.4 Replace new() with reset() in loop body (future)
- [ ] 12.1.5 Move destructor after loop (future)

### 12.2 Trivial Destructor Detection

> **Status**: Complete - See `compiler/src/types/env_lookups.cpp` and `compiler/src/codegen/core/drop.cpp`

- [x] 12.2.1 Identify classes with no destructor logic (`is_trivially_destructible()`)
- [x] 12.2.2 Identify classes with only trivial field drops (recursive check)
- [x] 12.2.3 Mark trivially destructible classes (TypeEnv API)
- [x] 12.2.4 Elide destructor calls for trivial classes (`type_needs_drop()` in `register_for_drop()`)

### 12.3 Batch Destruction
- [ ] 12.3.1 Identify arrays of objects (future)
- [ ] 12.3.2 Generate single destructor loop (future)
- [ ] 12.3.3 Vectorize trivial destructor operations (future)
- [ ] 12.3.4 Add tests for destructor optimization

## Phase 13: Cache-Friendly Layout

### 13.1 Field Reordering
- [ ] 13.1.1 Collect field access frequency from profiling
- [ ] 13.1.2 Place hot fields at start of layout
- [ ] 13.1.3 Reorder by size to reduce padding
- [ ] 13.1.4 Respect `@cold` directive for cold fields

### 13.2 Alignment Optimization
- [ ] 13.2.1 Align class start to cache line when beneficial
- [ ] 13.2.2 Pack related fields within cache lines
- [ ] 13.2.3 Add prefetch hints for sequential access
- [ ] 13.2.4 Track padding reduction statistics
- [ ] 13.2.5 Add tests for layout optimization

## Phase 14: Monomorphization for Classes

### 14.1 Detection
- [ ] 14.1.1 Identify generic functions with class type parameters
- [ ] 14.1.2 Track instantiation sites with sealed classes
- [ ] 14.1.3 Identify devirtualization opportunities

### 14.2 Specialization
- [ ] 14.2.1 Generate specialized function variant
- [ ] 14.2.2 Apply devirtualization in specialized version
- [ ] 14.2.3 Inline virtual calls in specialized version
- [ ] 14.2.4 Share code when optimization doesn't benefit
- [ ] 14.2.5 Add tests for class monomorphization

## Phase 15: Integration and Benchmarking

### 15.1 Pass Pipeline
- [ ] 15.1.1 Order OOP passes optimally in pipeline
- [ ] 15.1.2 Add pass dependencies (CHA before devirt)
- [ ] 15.1.3 Configure passes for optimization levels
- [ ] 15.1.4 Document pass interactions

### 15.2 Benchmark Suite
- [ ] 15.2.1 Create virtual dispatch microbenchmark
- [ ] 15.2.2 Create object creation throughput benchmark
- [ ] 15.2.3 Create HTTP request simulation benchmark
- [ ] 15.2.4 Create game loop simulation benchmark
- [ ] 15.2.5 Create deep inheritance chain benchmark

### 15.3 Comparison Benchmarks
- [ ] 15.3.1 Port benchmarks to Rust for comparison
- [ ] 15.3.2 Port benchmarks to C++ for comparison
- [ ] 15.3.3 Measure memory usage (heap vs stack)
- [ ] 15.3.4 Document performance results

## Validation

- [ ] V.1 Devirtualization rate > 90% for sealed classes
- [ ] V.2 Virtual call overhead < 5% vs direct call (when devirtualized)
- [ ] V.3 Stack allocation rate > 80% for local objects
- [ ] V.4 Pool hit rate > 95% in high-churn benchmarks
- [ ] V.5 Arena allocation 10x faster than malloc
- [ ] V.6 100K req/s HTTP benchmark without memory growth
- [ ] V.7 Compile time increase < 10%
- [ ] V.8 All existing tests pass
- [ ] V.9 Debug info preserved through optimizations
- [ ] V.10 No regressions in non-OOP code performance
