# Tasks: Memory Safety & Leak Prevention

**Status**: In progress (~45%)

**Priority**: High - Infrastructure reliability

## Phase 1: Compiler Memory Audit

### 1.1 AST Memory Management
- [x] 1.1.1 Audit AST node allocations in parser (uses Box<T>/unique_ptr throughout)
- [x] 1.1.2 Verify AST destruction in ASTNode destructors (RAII via unique_ptr)
- [x] 1.1.3 Check for orphaned nodes in error recovery paths (synchronize* functions, no manual cleanup)
- [x] 1.1.4 Review unique_ptr/shared_ptr usage consistency (85+ make_box calls, no raw new)

### 1.2 MIR Memory Management
- [x] 1.2.1 Audit MIR instruction allocations (value types in std::vector, no raw pointers)
- [x] 1.2.2 Verify MIR function/block cleanup (RAII via std::vector<BasicBlock>)
- [x] 1.2.3 Check MIR optimization pass memory handling (passes work on refs, no ownership)
- [x] 1.2.4 Review MIR cache eviction and cleanup (std::optional return, file-based storage)

### 1.3 Type System Memory
- [x] 1.3.1 Audit TypeInfo allocation in TypeRegistry (shared_ptr<Type>, standard maps)
- [x] 1.3.2 Review GenericInstance memory lifecycle (TypePtr with type_args vectors)
- [x] 1.3.3 Check BehaviorImpl memory management (stored in unordered_map by value)
- [x] 1.3.4 Verify constraint/bound cleanup (RAII via TypeEnv destruction)

### 1.4 Symbol Tables & Scopes
- [x] 1.4.1 Audit Scope allocation/deallocation (Scope::symbols_ is unordered_map)
- [x] 1.4.2 Review SymbolTable cleanup on scope exit (scopes stored in stack, RAII)
- [x] 1.4.3 Check module symbol cleanup (imported_symbols_ map, RAII)
- [x] 1.4.4 Verify import resolution cleanup (RAII via TypeEnv member maps)

### 1.5 Module System
- [ ] 1.5.1 Audit ModuleRegistry memory
- [ ] 1.5.2 Review cached module cleanup
- [ ] 1.5.3 Check dependency graph cleanup
- [ ] 1.5.4 Verify incremental compilation cache cleanup

## Phase 2: Runtime Memory Audit

### 2.1 Core Runtime (essential.c)
- [x] 2.1.1 Audit tml_string_* allocations (i64_to_str/f64_to_str documented, caller frees)
- [ ] 2.1.2 Review tml_array_* allocations
- [ ] 2.1.3 Check tml_slice_* ownership
- [x] 2.1.4 Verify panic/error path cleanup (panic catching uses static buffer, no leak)
- [x] 2.1.5 Review printf format string handling (safe, no dynamic allocation)

### 2.2 Async Runtime (async.c)
- [x] 2.2.1 Audit TmlTask allocation/deallocation (proper NULL checks, freed on completion)
- [x] 2.2.2 Review TmlExecutor cleanup (cleans up all tasks in tml_executor_free)
- [x] 2.2.3 Check TmlChannel buffer management (freed in tml_channel_destroy)
- [ ] 2.2.4 Verify timer/waker cleanup
- [x] 2.2.5 Review spawn/join memory lifecycle (state freed after task completion)

### 2.3 Heap Allocations
- [ ] 2.3.1 Review Heap[T] allocation codegen
- [ ] 2.3.2 Verify Heap[T] deallocation on drop
- [ ] 2.3.3 Check reference counting (Shared[T], Sync[T])
- [ ] 2.3.4 Audit cycle detection (if applicable)

## Phase 3: LLVM/Codegen Memory

### 3.1 LLVM IR Generation
- [ ] 3.1.1 Audit LLVMContext lifecycle
- [ ] 3.1.2 Review LLVMModule cleanup
- [ ] 3.1.3 Check LLVMBuilder memory handling
- [ ] 3.1.4 Verify DIBuilder cleanup (debug info)

### 3.2 Object Compilation
- [ ] 3.2.1 Review TargetMachine lifecycle
- [ ] 3.2.2 Check temporary file cleanup
- [ ] 3.2.3 Verify linker input cleanup

## Phase 4: Tooling Integration

### 4.1 Sanitizer Support
- [x] 4.1.1 Add AddressSanitizer build option (-fsanitize=address)
- [x] 4.1.2 Add LeakSanitizer integration
- [x] 4.1.3 Add MemorySanitizer option for uninitialized reads
- [x] 4.1.4 Create sanitizer test target in CMake (via TML_ENABLE_ASAN/UBSAN/LSAN/MSAN)

### 4.2 Valgrind Support
- [ ] 4.2.1 Create Valgrind suppression file for known false positives
- [ ] 4.2.2 Add `tml test --valgrind` option (Linux only)
- [ ] 4.2.3 Document Valgrind usage

### 4.3 CI Integration
- [ ] 4.3.1 Add sanitizer build to CI pipeline
- [ ] 4.3.2 Add memory leak check gate
- [ ] 4.3.3 Create memory benchmark tracking

### 4.4 Runtime Memory Tracking
- [x] 4.4.1 Create mem_track.h/.c tracking runtime
- [x] 4.4.2 Integrate with mem.c via TML_DEBUG_MEMORY flag
- [x] 4.4.3 Add --check-leaks / --no-check-leaks CLI flags
- [x] 4.4.4 Enable leak checking by default in debug builds
- [x] 4.4.5 Disable leak checking in release builds automatically

## Phase 5: Fixes & Improvements

### 5.1 RAII Patterns
- [ ] 5.1.1 Convert raw pointers to unique_ptr where appropriate
- [ ] 5.1.2 Add custom deleters where needed
- [ ] 5.1.3 Use make_unique/make_shared consistently
- [ ] 5.1.4 Document ownership in header comments

### 5.2 Error Path Cleanup
- [ ] 5.2.1 Add cleanup in parser error recovery
- [ ] 5.2.2 Add cleanup in type checker errors
- [ ] 5.2.3 Add cleanup in codegen errors
- [ ] 5.2.4 Review exception safety

### 5.3 Resource Management
- [ ] 5.3.1 Review file handle cleanup
- [ ] 5.3.2 Review process handle cleanup (Windows)
- [ ] 5.3.3 Review DLL/shared library unloading
- [ ] 5.3.4 Review temporary directory cleanup

## Phase 6: OOP Memory Management

### 6.1 Class Instance Memory
- [ ] 6.1.1 Audit class allocation in constructors
- [ ] 6.1.2 Verify vtable pointer initialization
- [ ] 6.1.3 Check base class constructor memory handling
- [ ] 6.1.4 Review constructor chain temporary allocations
- [ ] 6.1.5 Verify destructor call order in inheritance

### 6.2 Virtual Dispatch Memory
- [ ] 6.2.1 Audit vtable global constant allocation
- [ ] 6.2.2 Review vtable pointer storage in objects
- [ ] 6.2.3 Check interface dispatch table memory
- [ ] 6.2.4 Verify vtable deduplication doesn't leak

### 6.3 Object Pooling Memory
- [ ] 6.3.1 Audit Pool[T] free list management
- [ ] 6.3.2 Review pool growth allocation
- [ ] 6.3.3 Check pool cleanup on program exit
- [ ] 6.3.4 Verify thread-local pool cleanup on thread exit
- [ ] 6.3.5 Test pool memory under high churn

### 6.4 Arena Allocator Memory
- [ ] 6.4.1 Audit Arena bump pointer management
- [ ] 6.4.2 Review Arena chunk allocation
- [ ] 6.4.3 Check Arena reset clears all allocations
- [ ] 6.4.4 Verify nested Arena cleanup order
- [ ] 6.4.5 Test Arena with destructor callbacks

### 6.5 Value Classes
- [ ] 6.5.1 Verify @value classes have no heap allocation
- [ ] 6.5.2 Check pass-by-value copy correctness
- [ ] 6.5.3 Review inline storage memory layout

## Phase 7: Testing & Verification

### 7.1 Memory Tests
- [ ] 7.1.1 Create stress test for repeated compilation
- [ ] 7.1.2 Create test for large file compilation
- [ ] 7.1.3 Create test for many small files
- [ ] 7.1.4 Create test for error recovery paths

### 7.2 Verification
- [ ] 7.2.1 Run full test suite under ASan
- [ ] 7.2.2 Run full test suite under Valgrind
- [ ] 7.2.3 Verify zero leaks on clean exit
- [ ] 7.2.4 Document any accepted leaks (with justification)

### 7.3 OOP-Specific Memory Tests
- [ ] 7.3.1 Create stress test for class instantiation/destruction
- [ ] 7.3.2 Create test for deep inheritance chains
- [ ] 7.3.3 Create test for pool acquire/release cycles
- [ ] 7.3.4 Create test for arena alloc/reset cycles
- [ ] 7.3.5 Benchmark memory usage: OOP vs struct equivalents

## Validation

- [ ] V.1 Zero memory leaks in normal compilation paths
- [ ] V.2 Zero memory leaks in error recovery paths
- [ ] V.3 Sanitizer builds pass all tests
- [ ] V.4 Memory usage stays bounded during long sessions
- [ ] V.5 All allocations have clear ownership documentation
- [ ] V.6 Class constructors don't leak on error
- [ ] V.7 Pool/Arena memory correctly reclaimed
- [ ] V.8 No vtable memory fragmentation
