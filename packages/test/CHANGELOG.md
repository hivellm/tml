# Changelog

All notable changes to the TML test framework.

## [Unreleased]

## [0.3.0] - 2025-12-23### Added- **Module system integration** - Assertions now require `use test` import- **Full enum pattern matching support** - `when` expressions work correctly with enums- Test package examples with `use test`### Changed- **BREAKING**: Assertion functions removed from global scope  - `assert`, `assert_eq_i32`, etc. now require `use test`  - All test files must include `use test` at the top- Improved pattern matching codegen for enum values- Fixed enum value creation and comparison in LLVM backend### Fixed- Enum pattern matching in `when` expressions now returns correct values- Pattern match arms properly store and load results- Type conversion (i1 ↔ i32) handled correctly in pattern matching### Test Results- 9/10 tests passing (90% success rate)- All compiler tests: PASSED- All runtime tests except collections: PASSED- Known issue: collections.test.tml (pre-existing runtime bug)

### Planned
- Panic catching support (when exception handling is implemented)
- Parallel test execution
- Coverage analysis
- Property-based testing
- Snapshot testing
- Test fixtures and setup/teardown hooks

## [0.2.0] - 2025-12-23

### Added
- Complete runner module implementation with utilities
  - Stats tracking (add_passed, add_failed, add_ignored)
  - Metadata creation and management
  - Test result printing functions
  - Test summary generation
- Complete benchmark module with timing infrastructure
  - BenchResult type for benchmark data
  - Iteration calculation based on samples
  - Time formatting (ns, us, ms, s)
  - Simple benchmark runner
- Complete report module with output formatting
  - Multiple format support (Pretty, Quiet, Verbose)
  - Progress indicators and bars
  - Duration formatting utilities
  - Colored output stubs (for future)
- Comprehensive usage example (usage_example.test.tml)
- Updated documentation and implementation status

### Technical Details
- Uses builtin timing functions (time_ns, time_us, time_ms)
- Compatible with current compiler capabilities
- Supports basic closures and function types
- All modules export public APIs properly

## [0.1.0] - 2025-01-01

### Added
- Initial release
- Core assertion functions (assert, assert_eq, assert_ne, assert_gt, etc.)
- Test runner with discovery and execution
- Benchmark support with automatic iteration detection
- Multiple output formats (Pretty, Quiet, Json)
- Test decorators (@test, @should_panic, @ignore, @bench)
- Integration with `tml test` CLI command
- Comprehensive documentation and examples

### Implementation Status
- ✅ Test package structure
- ✅ Assertion module (12+ functions)
- ✅ Test runner module
- ✅ Benchmarking module
- ✅ Reporting module with multiple formats
- ✅ CLI integration
- ⚠️ Parallel execution (sequential for now)
- ❌ Panic catching (requires exception handling)
