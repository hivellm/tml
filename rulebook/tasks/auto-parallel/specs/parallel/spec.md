# Auto-Parallelization Specification

## Purpose

The Auto-Parallelization system automatically detects and parallelizes suitable code patterns at compile time, enabling TML programs to efficiently utilize multi-core CPUs without requiring explicit parallel annotations from the programmer.

## Requirements

### Requirement: Purity Analysis
The compiler SHALL analyze functions to determine if they are pure (no side effects).

#### Scenario: Pure arithmetic function
Given a function that only performs arithmetic operations
When analyzing for purity
Then it is marked as pure

#### Scenario: Function with IO
Given a function that calls println or reads files
When analyzing for purity
Then it is marked as impure with IO effect

#### Scenario: Function with mutation
Given a function that modifies global state
When analyzing for purity
Then it is marked as impure with Mutation effect

#### Scenario: Function calling impure function
Given a pure function that calls an impure function
When analyzing for purity
Then it is marked as impure (effect propagates)

#### Scenario: @pure annotation
Given a function annotated with @pure
When analyzing for purity
Then it is trusted as pure without deep analysis

### Requirement: Dependency Analysis
The compiler MUST detect loop-carried dependencies.

#### Scenario: Independent iterations
Given a loop `for i in 0 to n { a[i] = f(b[i]) }`
When analyzing dependencies
Then no loop-carried dependencies are detected

#### Scenario: Flow dependency (RAW)
Given a loop `for i in 1 to n { a[i] = a[i-1] + 1 }`
When analyzing dependencies
Then a RAW dependency is detected (read-after-write)

#### Scenario: Anti dependency (WAR)
Given a loop `for i in 0 to n-1 { a[i] = b[i+1] }`
When analyzing dependencies
Then a WAR dependency is detected (write-after-read)

#### Scenario: Output dependency (WAW)
Given a loop `for i in 0 to n { a[i % 10] = i }`
When analyzing dependencies
Then a WAW dependency is detected (write-after-write)

### Requirement: Parallel Loop Detection
The compiler SHALL detect loops that can be safely parallelized.

#### Scenario: Simple parallel loop
Given a loop with no dependencies and pure body
When detecting parallel patterns
Then the loop is marked as parallelizable

#### Scenario: Loop with reduction
Given a loop `for i in 0 to n { sum = sum + a[i] }`
When detecting parallel patterns
Then the loop is marked as reduction-parallelizable with + operator

#### Scenario: Loop with impure body
Given a loop containing println calls
When detecting parallel patterns
Then the loop is NOT marked as parallelizable

#### Scenario: Nested loops
Given nested loops where outer has no dependencies
When detecting parallel patterns
Then only the outer loop is marked as parallelizable

### Requirement: Parallel Annotations
The compiler SHALL support explicit parallelization control.

#### Scenario: @parallel annotation
Given a loop annotated with @parallel
When compiling
Then the loop is parallelized (error if not possible)

#### Scenario: @sequential annotation
Given a loop annotated with @sequential
When compiling
Then the loop is NOT parallelized even if possible

#### Scenario: @parallel with thread limit
Given a loop annotated with @parallel(threads: 4)
When compiling
Then the loop uses at most 4 threads

### Requirement: Parallel Code Generation
The compiler SHALL generate correct parallel code.

#### Scenario: Parallel for loop
Given a parallelizable for loop
When generating LLVM IR
Then it produces calls to tml_parallel_for with chunked work

#### Scenario: Work distribution
Given a parallel loop with 1000 iterations and 4 threads
When distributing work
Then each thread gets approximately 250 iterations

#### Scenario: Synchronization
Given a parallel loop
When generating code
Then a barrier is inserted after the parallel region

### Requirement: Runtime Thread Pool
The runtime SHALL provide a thread pool for parallel execution.

#### Scenario: Thread pool initialization
Given program startup
When initializing parallel runtime
Then a thread pool is created with CPU core count threads

#### Scenario: TML_THREADS environment variable
Given TML_THREADS=2 in environment
When initializing thread pool
Then only 2 worker threads are created

#### Scenario: Work stealing
Given unbalanced workload between threads
When executing parallel loop
Then idle threads steal work from busy threads

### Requirement: Safety Guarantees
The parallel system MUST maintain safety invariants.

#### Scenario: Deterministic output
Given a parallel loop over deterministic operations
When running multiple times
Then the same output is produced each time

#### Scenario: No data races
Given a parallel loop detected by the compiler
When executing in parallel
Then no data races occur (verified by ThreadSanitizer)

#### Scenario: Resource limits
Given TML_THREADS > CPU cores
When initializing thread pool
Then thread count is capped at CPU core count

### Requirement: Cost Model
The compiler SHOULD use heuristics to avoid overhead.

#### Scenario: Small loop
Given a loop with less than 1000 iterations
When deciding to parallelize
Then sequential execution is preferred (avoid overhead)

#### Scenario: Large loop
Given a loop with more than 10000 iterations
When deciding to parallelize
Then parallel execution is preferred

#### Scenario: Expensive body
Given a loop with computationally expensive body
When deciding to parallelize
Then parallel execution is preferred even for fewer iterations
