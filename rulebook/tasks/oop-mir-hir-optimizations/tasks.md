# Tasks: OOP Optimizations for MIR and HIR

## Phase 1: Class Hierarchy Analysis (CHA)

### 1.1 Data Structures
- [ ] 1.1.1 Create `ClassHierarchy` struct in `compiler/include/hir/analysis/`
- [ ] 1.1.2 Add parent/child relationship maps
- [ ] 1.1.3 Add sealed class tracking set
- [ ] 1.1.4 Add abstract class tracking set
- [ ] 1.1.5 Add final method tracking map

### 1.2 Hierarchy Building
- [ ] 1.2.1 Build parent/child graph during type checking
- [ ] 1.2.2 Track interface implementations per class
- [ ] 1.2.3 Compute transitive subclass closure
- [ ] 1.2.4 Build virtual method resolution table

### 1.3 Query Methods
- [ ] 1.3.1 Implement `get_parent(class_name)` query
- [ ] 1.3.2 Implement `get_children(class_name)` query
- [ ] 1.3.3 Implement `get_all_subclasses(class_name)` query
- [ ] 1.3.4 Implement `get_concrete_subclasses(class_name)` query
- [ ] 1.3.5 Implement `is_sealed(class_name)` query
- [ ] 1.3.6 Implement `is_final_method(class, method)` query
- [ ] 1.3.7 Implement `has_single_implementation(class_name)` query
- [ ] 1.3.8 Add unit tests for class hierarchy analysis

## Phase 2: HIR Devirtualization

### 2.1 Pass Infrastructure
- [ ] 2.1.1 Create `DevirtualizationPass` class in `compiler/src/hir/passes/`
- [ ] 2.1.2 Register pass in HIR pass manager
- [ ] 2.1.3 Add `--devirt` flag for selective enabling
- [ ] 2.1.4 Add devirtualization statistics counter

### 2.2 Exact Type Devirtualization
- [ ] 2.2.1 Track exact types from constructor calls
- [ ] 2.2.2 Propagate exact type through SSA values
- [ ] 2.2.3 Devirtualize calls when exact type known
- [ ] 2.2.4 Handle type narrowing in conditionals

### 2.3 Sealed/Final Devirtualization
- [ ] 2.3.1 Query CHA for sealed classes
- [ ] 2.3.2 Devirtualize all calls on sealed class types
- [ ] 2.3.3 Query CHA for final methods
- [ ] 2.3.4 Devirtualize final method calls regardless of receiver type
- [ ] 2.3.5 Handle inherited final methods

### 2.4 CHA-Based Devirtualization
- [ ] 2.4.1 Query CHA for concrete implementations
- [ ] 2.4.2 Devirtualize when single implementation exists
- [ ] 2.4.3 Handle whole-program analysis mode
- [ ] 2.4.4 Invalidate on dynamic loading (if supported)
- [ ] 2.4.5 Add tests for all devirtualization cases

## Phase 3: Speculative Devirtualization

### 3.1 Type Profiling Infrastructure
- [ ] 3.1.1 Design type profile data format
- [ ] 3.1.2 Implement type profile instrumentation pass
- [ ] 3.1.3 Implement type profile reader
- [ ] 3.1.4 Store hot types per call site

### 3.2 Speculative Transform
- [ ] 3.2.1 Identify polymorphic call sites
- [ ] 3.2.2 Generate type ID comparison
- [ ] 3.2.3 Generate fast path (direct call)
- [ ] 3.2.4 Generate slow path (vtable dispatch)
- [ ] 3.2.5 Handle 2-3 hot types per call site
- [ ] 3.2.6 Add tests for speculative devirtualization

## Phase 4: Virtual Call Inlining

### 4.1 Inlining Extension
- [ ] 4.1.1 Extend inlining pass to recognize devirtualized calls
- [ ] 4.1.2 Compute inlining benefit for virtual methods
- [ ] 4.1.3 Handle recursive virtual calls
- [ ] 4.1.4 Respect `@noinline` on virtual methods

### 4.2 Constructor Inlining
- [ ] 4.2.1 Prioritize constructor inlining
- [ ] 4.2.2 Inline base constructor chains
- [ ] 4.2.3 Fuse constructor initialization stores
- [ ] 4.2.4 Eliminate redundant vtable pointer stores
- [ ] 4.2.5 Add tests for virtual call inlining

## Phase 5: Dead Virtual Method Elimination

### 5.1 Reachability Analysis
- [ ] 5.1.1 Build virtual call graph
- [ ] 5.1.2 Mark reachable virtual methods from entry points
- [ ] 5.1.3 Handle reflection/dynamic dispatch (conservatively)
- [ ] 5.1.4 Track interface method reachability

### 5.2 Elimination Transform
- [ ] 5.2.1 Mark unreachable methods for removal
- [ ] 5.2.2 Remove method implementations from codegen
- [ ] 5.2.3 Replace vtable entries with null/trap
- [ ] 5.2.4 Track elimination statistics
- [ ] 5.2.5 Add tests for dead method elimination

## Phase 6: Vtable Optimization

### 6.1 Vtable Deduplication
- [ ] 6.1.1 Hash vtable contents
- [ ] 6.1.2 Identify identical vtables across classes
- [ ] 6.1.3 Share vtable global for identical layouts
- [ ] 6.1.4 Update class type info pointers

### 6.2 Vtable Splitting
- [ ] 6.2.1 Identify hot/cold methods via profiling
- [ ] 6.2.2 Place hot methods in primary vtable
- [ ] 6.2.3 Place cold methods in secondary vtable
- [ ] 6.2.4 Update dispatch to use correct vtable

### 6.3 Interface Vtable Optimization
- [ ] 6.3.1 Compact interface dispatch tables
- [ ] 6.3.2 Remove gaps from sparse interface layouts
- [ ] 6.3.3 Consider itable vs vtable offset approach
- [ ] 6.3.4 Add tests for vtable optimization

## Phase 7: Escape Analysis for Classes

### 7.1 Analysis Extension
- [ ] 7.1.1 Extend escape analysis to handle class instances
- [ ] 7.1.2 Track escape through `this` parameter
- [ ] 7.1.3 Track escape through method return values
- [ ] 7.1.4 Handle escape through field stores

### 7.2 Inheritance Handling
- [ ] 7.2.1 Analyze base class field escapes
- [ ] 7.2.2 Handle virtual method calls conservatively
- [ ] 7.2.3 Refine with devirtualization results
- [ ] 7.2.4 Track conditional escapes

### 7.3 Stack Allocation Transform
- [ ] 7.3.1 Replace malloc with alloca for non-escaping objects
- [ ] 7.3.2 Remove corresponding free calls
- [ ] 7.3.3 Adjust vtable pointer initialization
- [ ] 7.3.4 Track stack allocation statistics
- [ ] 7.3.5 Add tests for class escape analysis

## Phase 8: Value Classes

### 8.1 Directive Support
- [ ] 8.1.1 Add `@value` directive to parser
- [ ] 8.1.2 Validate: no virtual methods allowed
- [ ] 8.1.3 Validate: no inheritance allowed (extends)
- [ ] 8.1.4 Validate: must be sealed
- [ ] 8.1.5 Allow interface implementation

### 8.2 Code Generation
- [ ] 8.2.1 Generate struct layout without vtable pointer
- [ ] 8.2.2 Generate pass-by-value semantics
- [ ] 8.2.3 Generate copy operations for assignment
- [ ] 8.2.4 Inline method calls at compile time

### 8.3 Auto-Inference
- [ ] 8.3.1 Detect sealed classes with no virtual methods
- [ ] 8.3.2 Auto-apply value class optimization
- [ ] 8.3.3 Warn when @value could be applied
- [ ] 8.3.4 Add tests for value classes

## Phase 9: Object Pooling

### 9.1 Pool Type Implementation
- [ ] 9.1.1 Design `Pool[T]` API in core library
- [ ] 9.1.2 Implement acquire() method
- [ ] 9.1.3 Implement release() method
- [ ] 9.1.4 Implement lock-free free list
- [ ] 9.1.5 Implement growth policy (fixed, doubling)

### 9.2 Directive Support
- [ ] 9.2.1 Add `@pool` directive to parser
- [ ] 9.2.2 Parse pool parameters (size, grow, thread_local)
- [ ] 9.2.3 Generate global pool instance
- [ ] 9.2.4 Transform `new()` to `acquire()`
- [ ] 9.2.5 Transform `drop()` to `release()`

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
- [ ] 12.1.1 Detect loop-local object allocations
- [ ] 12.1.2 Verify object doesn't escape loop
- [ ] 12.1.3 Hoist allocation before loop
- [ ] 12.1.4 Replace new() with reset() in loop body
- [ ] 12.1.5 Move destructor after loop

### 12.2 Trivial Destructor Detection
- [ ] 12.2.1 Identify classes with no destructor logic
- [ ] 12.2.2 Identify classes with only trivial field drops
- [ ] 12.2.3 Mark trivially destructible classes
- [ ] 12.2.4 Elide destructor calls for trivial classes

### 12.3 Batch Destruction
- [ ] 12.3.1 Identify arrays of objects
- [ ] 12.3.2 Generate single destructor loop
- [ ] 12.3.3 Vectorize trivial destructor operations
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
