# Changelog

All notable changes to the TML test framework.

## [Unreleased]

### Added
- Panic catching support (when exception handling is implemented)
- Parallel test execution
- Coverage analysis
- Property-based testing
- Snapshot testing

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
