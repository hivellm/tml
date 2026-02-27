//! # Coordinator Unit Tests
//!
//! Tests for the test coordinator's internal logic:
//! - Config defaults
//! - Result struct initialization
//! - NDJSON parsing into SuiteRunResult
//! - Result aggregation

#include "testing/testing_coordinator.hpp"
#include "testing/testing_protocol.hpp"

#include <gtest/gtest.h>
#include <sstream>
#include <string>

using namespace tml::testing;

// ============================================================================
// Config defaults
// ============================================================================

TEST(CoordinatorTest, ConfigDefaults) {
    TestConfig config;
    EXPECT_EQ(config.max_per_suite, 8);
    EXPECT_EQ(config.compile_threads, 0);
    EXPECT_EQ(config.exec_concurrent, 0);
    EXPECT_EQ(config.timeout_seconds, 20);
    EXPECT_FALSE(config.no_cache);
    EXPECT_FALSE(config.verbose);
    EXPECT_FALSE(config.coverage);
    EXPECT_TRUE(config.fail_fast);
    EXPECT_FALSE(config.list_suites);
    EXPECT_TRUE(config.root_dir.empty());
    EXPECT_TRUE(config.patterns.empty());
    EXPECT_TRUE(config.suite_filters.empty());
}

// ============================================================================
// Result struct defaults
// ============================================================================

TEST(CoordinatorTest, TestRunResultDefaults) {
    TestRunResult result;
    EXPECT_EQ(result.total_tests, 0);
    EXPECT_EQ(result.passed, 0);
    EXPECT_EQ(result.failed, 0);
    EXPECT_EQ(result.crashed, 0);
    EXPECT_EQ(result.timed_out, 0);
    EXPECT_EQ(result.skipped, 0);
    EXPECT_EQ(result.compilation_errors, 0);
    EXPECT_EQ(result.total_duration_us, 0);
    EXPECT_TRUE(result.suites.empty());
}

TEST(CoordinatorTest, SuiteRunResultDefaults) {
    SuiteRunResult result;
    EXPECT_EQ(result.test_count, 0);
    EXPECT_EQ(result.passed, 0);
    EXPECT_EQ(result.failed, 0);
    EXPECT_EQ(result.crashed, 0);
    EXPECT_TRUE(result.compile_ok);
    EXPECT_TRUE(result.compile_error.empty());
    EXPECT_EQ(result.compile_time_us, 0);
    EXPECT_EQ(result.exec_time_us, 0);
    EXPECT_TRUE(result.tests.empty());
}

TEST(CoordinatorTest, CoordinatorTestResultDefaults) {
    CoordinatorTestResult result;
    EXPECT_EQ(result.index, 0);
    EXPECT_TRUE(result.name.empty());
    EXPECT_TRUE(result.file.empty());
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.duration_us, 0);
}

// ============================================================================
// Config patterns
// ============================================================================

TEST(CoordinatorTest, ConfigPatterns) {
    TestConfig config;
    config.patterns = {"core/str", "std/json"};
    EXPECT_EQ(config.patterns.size(), 2);
    EXPECT_EQ(config.patterns[0], "core/str");
    EXPECT_EQ(config.patterns[1], "std/json");
}

TEST(CoordinatorTest, ConfigSuiteFilters) {
    TestConfig config;
    config.suite_filters = {"core/str"};
    config.max_per_suite = 1;
    EXPECT_EQ(config.suite_filters.size(), 1);
    EXPECT_EQ(config.max_per_suite, 1);
}

// ============================================================================
// run_tests with nonexistent root returns empty
// ============================================================================

TEST(CoordinatorTest, RunTestsEmptyRoot) {
    TestConfig config;
    config.root_dir = "/nonexistent/path/that/does/not/exist";
    auto result = run_tests(config);
    EXPECT_EQ(result.total_tests, 0);
    EXPECT_EQ(result.passed, 0);
    EXPECT_EQ(result.failed, 0);
    EXPECT_TRUE(result.suites.empty());
}

// ============================================================================
// SuiteRunResult with test results
// ============================================================================

TEST(CoordinatorTest, SuiteRunResultAggregation) {
    SuiteRunResult suite;
    suite.name = "core_str";
    suite.group = "core";
    suite.test_count = 3;

    CoordinatorTestResult t1;
    t1.index = 0;
    t1.name = "test_concat";
    t1.passed = true;
    t1.duration_us = 1000;

    CoordinatorTestResult t2;
    t2.index = 1;
    t2.name = "test_split";
    t2.passed = true;
    t2.duration_us = 2000;

    CoordinatorTestResult t3;
    t3.index = 2;
    t3.name = "test_trim";
    t3.passed = false;
    t3.exit_code = 1;
    t3.error = "assert_eq failed";
    t3.duration_us = 500;

    suite.tests = {t1, t2, t3};
    suite.passed = 2;
    suite.failed = 1;

    EXPECT_EQ(suite.tests.size(), 3);
    EXPECT_EQ(suite.passed, 2);
    EXPECT_EQ(suite.failed, 1);
    EXPECT_TRUE(suite.tests[0].passed);
    EXPECT_TRUE(suite.tests[1].passed);
    EXPECT_FALSE(suite.tests[2].passed);
    EXPECT_EQ(suite.tests[2].error, "assert_eq failed");
}

// ============================================================================
// TestRunResult aggregation from suites
// ============================================================================

TEST(CoordinatorTest, TestRunResultAggregation) {
    TestRunResult result;

    SuiteRunResult s1;
    s1.name = "core_str";
    s1.test_count = 2;
    s1.passed = 2;
    s1.failed = 0;

    SuiteRunResult s2;
    s2.name = "std_json";
    s2.test_count = 3;
    s2.passed = 1;
    s2.failed = 2;

    result.suites = {s1, s2};

    // Manually aggregate (coordinator does this automatically)
    for (const auto& suite : result.suites) {
        result.total_tests += suite.test_count;
        result.passed += suite.passed;
        result.failed += suite.failed;
    }

    EXPECT_EQ(result.total_tests, 5);
    EXPECT_EQ(result.passed, 3);
    EXPECT_EQ(result.failed, 2);
    EXPECT_EQ(result.suites.size(), 2);
}

// ============================================================================
// Compilation error handling
// ============================================================================

TEST(CoordinatorTest, CompilationErrorSuiteResult) {
    SuiteRunResult suite;
    suite.name = "bad_suite";
    suite.compile_ok = false;
    suite.compile_error = "Type error in test_broken.tml";
    suite.test_count = 5;

    EXPECT_FALSE(suite.compile_ok);
    EXPECT_FALSE(suite.compile_error.empty());
    EXPECT_EQ(suite.test_count, 5);
    EXPECT_EQ(suite.passed, 0);
    EXPECT_EQ(suite.failed, 0);
}

// ============================================================================
// Protocol event → CoordinatorTestResult mapping
// ============================================================================

TEST(CoordinatorTest, TestPassEventMapping) {
    // Verify that TestPassEvent fields match what coordinator expects
    TestPassEvent pass;
    pass.index = 3;
    pass.duration_us = 42000;

    // These are the fields coordinator reads
    EXPECT_EQ(pass.index, 3);
    EXPECT_EQ(pass.duration_us, 42000);
}

TEST(CoordinatorTest, TestFailEventMapping) {
    TestFailEvent fail;
    fail.index = 1;
    fail.name = "test_broken";
    fail.error = "expected 42, got 0";
    fail.exit_code = 1;
    fail.duration_us = 100;

    EXPECT_EQ(fail.index, 1);
    EXPECT_EQ(fail.error, "expected 42, got 0");
    EXPECT_EQ(fail.exit_code, 1);
}

TEST(CoordinatorTest, TestCrashEventMapping) {
    TestCrashEvent crash;
    crash.index = 0;
    crash.name = "test_segfault";
    crash.signal = "SIGSEGV";
    crash.duration_us = 50;

    EXPECT_EQ(crash.index, 0);
    EXPECT_EQ(crash.signal, "SIGSEGV");
}
