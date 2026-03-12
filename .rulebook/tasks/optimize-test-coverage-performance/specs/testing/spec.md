# Spec Delta: Testing Framework Performance

## MODIFIED Requirements

### Requirement: Incremental Cache Coverage Support
The test framework SHALL reuse incremental compilation cache (.obj files) for coverage runs when the source file has not changed. The coverage instrumentation is injected at codegen level and MUST NOT invalidate object file caches.

#### Scenario: Warm cache coverage run
Given a full test suite has been compiled previously without coverage
When the user runs `tml test --coverage`
Then the framework SHALL reuse existing .obj files from the incremental cache
And only re-link test EXEs with coverage-instrumented runtime

#### Scenario: Cache separation between coverage and normal modes
Given the user alternates between `tml test` and `tml test --coverage`
When the compilation cache hash is computed
Then the hash SHALL NOT include the coverage flag for compilation artifacts
And the hash SHALL include the coverage flag only for execution/linking artifacts

### Requirement: Thread Budget Control
The test framework SHALL limit total concurrent LLVM compilation threads to `hardware_concurrency()` across all suite compilations. The framework MUST NOT spawn more threads than available CPU cores.

#### Scenario: Thread budget on 8-core machine
Given a machine with 8 hardware threads
When compiling 200 test suites in parallel
Then the total number of concurrent LLVM compilation threads SHALL NOT exceed 8
And the system CPU usage SHALL remain below 90% average

### Requirement: Conditional Library Linking
The test framework SHALL only link OpenSSL libraries (libcrypto, libssl) to test EXEs that import crypto-dependent modules. Test EXEs that do not use crypto MUST NOT link against OpenSSL.

#### Scenario: Non-crypto test suite linking
Given a test suite that only imports `core/str` and `core/fmt`
When the suite is compiled and linked
Then the link command SHALL NOT include libcrypto or libssl
And the resulting EXE SHALL not depend on OpenSSL DLLs

#### Scenario: Crypto test suite linking
Given a test suite that imports `std/crypto`
When the suite is compiled and linked
Then the link command SHALL include libcrypto and libssl

## ADDED Requirements

### Requirement: Coverage Hash Table Sizing
The coverage runtime hash table MUST be sized to maintain a load factor below 60% for the expected function count. With 16000+ tracked functions, the table size SHALL be at least 32000 entries.

#### Scenario: High function count coverage
Given a coverage run tracking 16000 functions
When functions are registered in the coverage hash table
Then no more than 5% of lookups SHALL require more than 3 probe steps
And no functions SHALL be silently dropped due to table exhaustion

### Requirement: Compile-Execute Pipelining
The test coordinator SHOULD begin executing completed test EXEs while other suites are still compiling, rather than waiting for all compilations to complete before starting execution.

#### Scenario: Pipelined execution
Given 200 test suites to compile and execute
When the first suite finishes compilation
Then execution of that suite SHALL begin immediately
And compilation of remaining suites SHALL continue concurrently
