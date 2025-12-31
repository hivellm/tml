// cmd_test.cpp - Entry point for test command
// Implementation split into tester/ directory for maintainability:
//   - tester/tester_internal.hpp  - Shared header with types and declarations
//   - tester/helpers.cpp          - Utilities (ColorOutput, format_duration, etc.)
//   - tester/discovery.cpp        - Test file discovery
//   - tester/execution.cpp        - Test execution (in-process and process-based)
//   - tester/output.cpp           - Result formatting and printing
//   - tester/benchmark.cpp        - Benchmark functionality
//   - tester/run.cpp              - Main run_test function and argument parsing

// This file is intentionally minimal - all implementation is in tester/
