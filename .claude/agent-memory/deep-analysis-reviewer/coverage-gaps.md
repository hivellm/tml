# Coverage System Gaps Analysis (2026-02-28)

## Root Cause: Why Crypto Modules Show Low/Zero Coverage

### Primary: Test Crashes (Heap Corruption)
Three crypto test files crash with STATUS_HEAP_CORRUPTION (0xC0000374):
- `key.test.tml` - crashes -> coverage file never written -> 4/63 (transitive only)
- `sign.test.tml` - crashes -> coverage file never written -> 1/48 (transitive only)
- `rsa.test.tml` - crashes -> coverage file never written -> 0/18

When a test crashes, `tml_coverage_write_file()` (called in dispatcher epilogue) never runs.

The 4 covered functions in crypto/key (KeyType::from_name, PublicKey::size_bits,
PublicKey::destroy, PublicKey::drop) are called TRANSITIVELY by other passing modules:
- x509_test_minimal.tml calls KeyType::from_name (line 71)
- rsa.tml library code calls key.size_bits() (line 230)
- Any PublicKey user calls destroy/drop on cleanup

### Secondary: Full Suite Transient Failures
Tests that pass individually show 0% in full coverage run:
- crypto/x509 (0/48) - passes individually with 100% coverage
- crypto/dh (0/25) - passes individually
- crypto/ecdh (0/27) - passes individually

The full coverage run (03:17 AM) had 45 failures + 8 compilation errors.
These tests likely failed transiently under load (parallel compilation/execution).

### Tertiary: MIR Codegen Missing Coverage
Location: `compiler/src/codegen/mir_codegen.cpp`
- `coverage_enabled` flag is set (query_core.cpp:624) but only used to skip inlining (line 691)
- NO `tml_cover_func()` calls emitted in MIR codegen
- Currently mitigated: tests with TML imports fall back to AST codegen
- Will become a problem as MIR codegen matures

### Quaternary: Legacy EXE Runner Environment Block Bug
Location: `compiler/src/cli/tester/exe_test_execution.cpp` lines 227-233
- `TML_COVERAGE_FILE` appended AFTER double-NUL terminator in Windows env block
- Makes it invisible to CreateProcessA
- NOT triggered by MCP tool (uses coordinator path instead)
- Bug also at lines 399-406 (synchronous subprocess launch)

## Key Files
- `compiler/src/testing/testing_coverage.cpp` - extract_functions(), compute_coverage()
- `compiler/src/testing/testing_coordinator.cpp` - v3 coordinator (correct coverage path)
- `compiler/src/testing/testing_process.cpp` - build_environment_block() (correct)
- `compiler/src/cli/tester/exe_test_execution.cpp` - legacy env block bug
- `compiler/src/codegen/llvm/core/llvm_utils.cpp` - emit_coverage() (AST only)
- `compiler/src/codegen/llvm/decl/impl.cpp` - emit_coverage(type_name + "::" + method.name)
- `compiler/src/codegen/mir_codegen.cpp` - coverage_enabled but unused for instrumentation
- `lib/test/runtime/coverage.c` - tml_cover_func(), tml_coverage_write_file()

## Verification Commands
```
# Check individual test crashes:
mcp__tml__run file=lib/std/tests/crypto/key.test.tml

# Run single test with coverage:
mcp__tml__test path=lib/std/tests/crypto/x509.test.tml coverage=true no_cache=true

# Full coverage run:
mcp__tml__test coverage=true no_cache=true
```
