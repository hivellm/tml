TML_MODULE("compiler")

//! # Test Runner Error Explanations
//!
//! Error codes X001-X010 for test compilation, execution, and infrastructure errors.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_testing_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"X001", R"EX(
Test compilation failed [X001]

The test suite could not be compiled. This means the TML test files contain
errors that prevented the compiler from generating a test executable.

Common causes:

1. Syntax error in a test file
2. Type error in test code
3. Missing import or undefined symbol in test
4. Dispatcher code generation failed

How to fix:

Look at the compilation error above X001 for the specific cause. Run the
individual test file to isolate the issue:

    tml test path/to/specific.test.tml

Fix the reported type or syntax errors, then re-run the tests.
)EX"},

        {"X002", R"EX(
Test timeout [X002]

A test exceeded its time limit and was killed. By default, tests have a
100ms execution limit per test.

Common causes:

1. An infinite loop in the test or the code being tested
2. A deadlock (waiting for a lock that is never released)
3. A blocking I/O operation without a timeout
4. Extremely slow computation that should be optimized

How to fix:

- Identify which test timed out (shown in the test output)
- Check for infinite loops, missing loop termination conditions
- Check for deadlocks in concurrent code
- If the test genuinely needs more time, restructure to test smaller units

The timeout is enforced by the test runner subprocess watchdog.
)EX"},

        {"X003", R"EX(
Test crash [X003]

The test process crashed during execution. This indicates a runtime error
that caused the process to terminate abnormally — such as a segfault,
assertion failure, or an unhandled panic.

Common causes:

1. Null pointer dereference
2. Buffer overflow / out-of-bounds memory access
3. Stack overflow (infinite recursion)
4. Unhandled panic or assertion in test or library code
5. A subprocess crash due to an OS signal (SIGSEGV, SIGABRT, etc.)

How to fix:

- Run the individual test to isolate the crash:
    tml test path/to/specific.test.tml

- Use `debug_layers=true` to inspect the generated IR for ABI issues:
    mcp__tml__test with debug_layers=true

- Check for memory safety issues in the code under test
- Look for panics or assertions that may be triggered by edge cases
)EX"},

        {"X004", R"EX(
Failed to launch test subprocess [X004]

The test runner could not launch the test executable as a subprocess. This
is an infrastructure error, not a test logic error.

Common causes:

1. The test executable was not built successfully (see X001)
2. The executable path is incorrect or the file does not exist
3. File permissions prevent execution
4. The system is out of process handles or memory

How to fix:

- Check that compilation succeeded (no X001 errors)
- Verify the build directory is accessible
- Ensure the test executable has execute permissions
- Try a clean rebuild: `tml test --no-cache`
)EX"},

        {"X005", R"EX(
Test assertion failed [X005]

A test assertion did not hold. The test executed successfully but produced
a value that did not match the expected result.

Common causes:

1. The function under test has a bug
2. The expected value in the assertion is wrong
3. An edge case was not handled correctly
4. Floating-point precision differences

How to fix:

Look at the assertion failure output for the specific expected vs actual
values. Fix the function implementation or update the expected values if
the test expectation was wrong.

Example assertion formats:
    assert_eq(actual, expected, "message")
    assert_true(condition, "message")
)EX"},

        {"X006", R"EX(
NDJSON protocol error [X006]

The test runner received malformed NDJSON (Newline-Delimited JSON) output
from the test subprocess. The test protocol uses NDJSON to stream results
from the test executable back to the coordinator.

Common causes:

1. The test subprocess crashed before finishing a JSON event
2. The test executable wrote non-JSON output to stdout
3. A test used `print()` to stdout, corrupting the NDJSON stream

How to fix:

- Remove any direct stdout output (`print()`, `write_line()`) from test code
- Test output should use the test framework (assert_eq, etc.), not print
- If the subprocess crashed, see X003 for crash diagnosis

Note: Use `stderr` for debugging output in tests — it does not interfere
with the NDJSON protocol.
)EX"},

        {"X007", R"EX(
Test runtime archive error [X007]

The test runner could not load or access the compiled runtime archive
needed to link test executables.

Common causes:

1. The runtime library was not built
2. The runtime archive file is corrupted
3. An incompatible version of the runtime is cached

How to fix:

- Rebuild the compiler: `scripts/build.bat --clean`
- Run tests with cache invalidation: `tml test --no-cache`
- Verify the build directory structure is intact
)EX"},

        {"X008", R"EX(
Test cache corrupted [X008]

The incremental test cache contains invalid or corrupted data and could
not be loaded. The test runner will fall back to a full recompile.

Common causes:

1. The cache was written partially due to a previous crash
2. The cache format changed after a compiler update
3. Disk errors corrupted the cache file

How to fix:

This error is usually self-healing — the cache is automatically invalidated
and tests will recompile from scratch. If the error persists:

- Run with `--no-cache` to force a full rebuild
- The cache files are in `build/debug/cache/tests.json`
  and `build/debug/cache/incr/`
)EX"},

        {"X009", R"EX(
No tests found [X009]

The test runner did not find any test files matching the specified criteria.

Common causes:

1. The test path or filter does not match any test files
2. Test files do not have the `.test.tml` extension
3. The specified directory does not contain test files
4. A typo in the suite name or filter

How to fix:

- Verify test files have the `.test.tml` extension
- Check the path or suite name for typos:
    tml test lib/core/tests/str/     # run a specific directory
    tml test --filter "split"         # filter by test name

- Run without filters to see all available tests:
    tml test
)EX"},

        {"X010", R"EX(
Coverage tracking failed [X010]

The test coverage tracking system encountered an error and could not
record or report coverage data.

Common causes:

1. The coverage output directory is not writable
2. The coverage data file is corrupted
3. A concurrent coverage write caused a conflict

How to fix:

- Ensure the `build/coverage/` directory is writable
- Try running without coverage: `tml test` (without `--coverage`)
- Clean the coverage data and retry: run with `--no-cache --coverage`

Coverage data is stored in `build/coverage/` and is optional — tests
still run and pass/fail correctly without coverage tracking.
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
