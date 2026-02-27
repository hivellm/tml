# Tasks: Auto-Parallelization

## Progress: 0% (0/32 tasks complete)

## 1. Setup Phase
- [ ] 1.1 Create `src/analysis/` directory structure
- [ ] 1.2 Create `src/transform/` directory structure
- [ ] 1.3 Add `@parallel`, `@sequential`, `@pure` tokens to lexer
- [ ] 1.4 Add annotation parsing to parser

## 2. Purity Analysis Phase
- [ ] 2.1 Define effect types (IO, Mutation, Channel, Pure)
- [ ] 2.2 Implement effect inference for expressions
- [ ] 2.3 Implement effect inference for statements
- [ ] 2.4 Implement effect tracking for function calls
- [ ] 2.5 Implement `@pure` annotation handling
- [ ] 2.6 Add purity info to type environment

## 3. Dependency Analysis Phase
- [ ] 3.1 Implement loop variable extraction
- [ ] 3.2 Implement read/write set computation
- [ ] 3.3 Detect RAW (flow) dependencies
- [ ] 3.4 Detect WAR (anti) dependencies
- [ ] 3.5 Detect WAW (output) dependencies
- [ ] 3.6 Implement array subscript analysis
- [ ] 3.7 Implement basic alias analysis

## 4. Parallel Pattern Detection Phase
- [ ] 4.1 Detect parallelizable for loops
- [ ] 4.2 Detect parallelizable map operations
- [ ] 4.3 Detect parallelizable reduce operations
- [ ] 4.4 Detect independent statement blocks
- [ ] 4.5 Implement cost model for parallelization decisions

## 5. Runtime Implementation Phase
- [ ] 5.1 Implement thread pool with configurable size
- [ ] 5.2 Implement work stealing deque
- [ ] 5.3 Implement parallel_for primitive
- [ ] 5.4 Implement barrier synchronization
- [ ] 5.5 Implement CPU core detection
- [ ] 5.6 Add TML_THREADS environment variable support

## 6. Code Generation Phase
- [ ] 6.1 Generate parallel loop IR
- [ ] 6.2 Implement work chunking
- [ ] 6.3 Add LLVM declarations for parallel runtime
- [ ] 6.4 Handle `@parallel` annotation
- [ ] 6.5 Handle `@sequential` annotation

## 7. Testing Phase
- [ ] 7.1 Write unit tests for purity analysis
- [ ] 7.2 Write unit tests for dependency analysis
- [ ] 7.3 Write integration tests with parallel loops
- [ ] 7.4 Write benchmark comparing parallel vs sequential
- [ ] 7.5 Verify deterministic output

## 8. Documentation Phase
- [ ] 8.1 Document parallel annotations in specs
- [ ] 8.2 Add examples to 14-EXAMPLES.md
- [ ] 8.3 Update CLI documentation with --parallel flag
