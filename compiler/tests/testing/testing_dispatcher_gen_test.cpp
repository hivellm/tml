//! # Dispatcher IR Generator Unit Tests
//!
//! Tests that the NDJSON dispatcher generates valid LLVM IR with correct:
//! - Function declarations (tml_test_N, C library functions)
//! - String constants (JSON format strings, test names/files)
//! - Control flow (main, run_all_tests, run_single_test, list_tests)
//! - NDJSON event format strings

#include "testing/testing_dispatcher_gen.hpp"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace tml::testing;

// ============================================================================
// Helper: check if IR contains a substring
// ============================================================================

static bool contains(const std::string& ir, const std::string& needle) {
    return ir.find(needle) != std::string::npos;
}

// ============================================================================
// Basic IR structure
// ============================================================================

TEST(DispatcherGenTest, EmptyTestList) {
    auto ir = generate_ndjson_dispatcher_ir({}, "empty_suite");
    EXPECT_TRUE(contains(ir, "ModuleID"));
    EXPECT_TRUE(contains(ir, "define i32 @main"));
    EXPECT_TRUE(contains(ir, "define i32 @run_all_tests"));
    EXPECT_TRUE(contains(ir, "define i32 @list_tests"));
    EXPECT_TRUE(contains(ir, "define i32 @run_single_test"));
    // Should handle zero tests gracefully
    EXPECT_TRUE(contains(ir, "ret i32"));
}

TEST(DispatcherGenTest, SingleTest) {
    std::vector<DispatcherTestInfo> tests = {
        {"test_hello", "hello.test.tml", 0},
    };

    auto ir = generate_ndjson_dispatcher_ir(tests, "single_suite");

    // Should declare exactly one test function
    EXPECT_TRUE(contains(ir, "declare i32 @tml_test_0()"));
    EXPECT_FALSE(contains(ir, "declare i32 @tml_test_1()"));

    // Should have test name and file constants
    EXPECT_TRUE(contains(ir, "test_hello"));
    EXPECT_TRUE(contains(ir, "hello.test.tml"));
}

TEST(DispatcherGenTest, MultipleTests) {
    std::vector<DispatcherTestInfo> tests = {
        {"test_concat", "basic.test.tml", 0},
        {"test_split", "basic.test.tml", 1},
        {"test_trim", "trim.test.tml", 2},
    };

    auto ir = generate_ndjson_dispatcher_ir(tests, "str_suite");

    // Should declare all three test functions
    EXPECT_TRUE(contains(ir, "declare i32 @tml_test_0()"));
    EXPECT_TRUE(contains(ir, "declare i32 @tml_test_1()"));
    EXPECT_TRUE(contains(ir, "declare i32 @tml_test_2()"));
    EXPECT_FALSE(contains(ir, "declare i32 @tml_test_3()"));

    // Should have all test names
    EXPECT_TRUE(contains(ir, "test_concat"));
    EXPECT_TRUE(contains(ir, "test_split"));
    EXPECT_TRUE(contains(ir, "test_trim"));
}

// ============================================================================
// NDJSON event format strings
// ============================================================================

TEST(DispatcherGenTest, HasSuiteStartFormat) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "my_suite");
    EXPECT_TRUE(contains(ir, "suite_start"));
}

TEST(DispatcherGenTest, HasSuiteEndFormat) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "my_suite");
    EXPECT_TRUE(contains(ir, "suite_end"));
}

TEST(DispatcherGenTest, HasTestStartFormat) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "my_suite");
    EXPECT_TRUE(contains(ir, "test_start"));
}

TEST(DispatcherGenTest, HasTestPassFormat) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "my_suite");
    EXPECT_TRUE(contains(ir, "test_pass"));
}

TEST(DispatcherGenTest, HasTestFailFormat) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "my_suite");
    EXPECT_TRUE(contains(ir, "test_fail"));
}

// ============================================================================
// Command-line parsing
// ============================================================================

TEST(DispatcherGenTest, HasRunAllArgParsing) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "my_suite");
    EXPECT_TRUE(contains(ir, "--run-all"));
    EXPECT_TRUE(contains(ir, "call i32 @run_all_tests"));
}

TEST(DispatcherGenTest, HasListArgParsing) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "my_suite");
    EXPECT_TRUE(contains(ir, "--list"));
    EXPECT_TRUE(contains(ir, "call i32 @list_tests"));
}

TEST(DispatcherGenTest, HasTestIndexArgParsing) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "my_suite");
    EXPECT_TRUE(contains(ir, "--test-index="));
    EXPECT_TRUE(contains(ir, "call i32 @run_single_test"));
}

// ============================================================================
// C library declarations
// ============================================================================

TEST(DispatcherGenTest, HasCLibraryDeclarations) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "my_suite");
    EXPECT_TRUE(contains(ir, "declare i32 @strcmp"));
    EXPECT_TRUE(contains(ir, "declare i32 @strncmp"));
    EXPECT_TRUE(contains(ir, "declare i32 @atoi"));
    EXPECT_TRUE(contains(ir, "declare i32 @printf"));
    EXPECT_TRUE(contains(ir, "declare void @fflush"));
    EXPECT_TRUE(contains(ir, "declare i64 @clock"));
}

// ============================================================================
// Timing support
// ============================================================================

TEST(DispatcherGenTest, HasTimingCalls) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "my_suite");
    // Should call clock() before and after each test
    EXPECT_TRUE(contains(ir, "call i64 @clock()"));
    EXPECT_TRUE(contains(ir, "duration_us"));
}

// ============================================================================
// Suite name in output
// ============================================================================

TEST(DispatcherGenTest, SuiteNameInConstants) {
    std::vector<DispatcherTestInfo> tests = {{"test_x", "x.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "core_str_tests");
    EXPECT_TRUE(contains(ir, "core_str_tests"));
}

// ============================================================================
// Target triple
// ============================================================================

TEST(DispatcherGenTest, HasTargetTriple) {
    auto ir = generate_ndjson_dispatcher_ir({}, "suite");
#ifdef _WIN32
    EXPECT_TRUE(contains(ir, "x86_64-pc-windows-msvc"));
#else
    EXPECT_TRUE(contains(ir, "x86_64-unknown-linux-gnu"));
#endif
}

// ============================================================================
// run_single_test switch
// ============================================================================

TEST(DispatcherGenTest, RunSingleTestSwitchCases) {
    std::vector<DispatcherTestInfo> tests = {
        {"test_a", "a.test.tml", 0},
        {"test_b", "b.test.tml", 1},
        {"test_c", "c.test.tml", 2},
    };
    auto ir = generate_ndjson_dispatcher_ir(tests, "suite");

    // Switch instruction with 3 cases
    EXPECT_TRUE(contains(ir, "switch i32 %index"));
    EXPECT_TRUE(contains(ir, "i32 0, label %test_0"));
    EXPECT_TRUE(contains(ir, "i32 1, label %test_1"));
    EXPECT_TRUE(contains(ir, "i32 2, label %test_2"));
}

// ============================================================================
// Error handling
// ============================================================================

TEST(DispatcherGenTest, InvalidIndexError) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "suite");
    EXPECT_TRUE(contains(ir, "invalid test index"));
}

TEST(DispatcherGenTest, NoArgsError) {
    std::vector<DispatcherTestInfo> tests = {{"test_a", "a.test.tml", 0}};
    auto ir = generate_ndjson_dispatcher_ir(tests, "suite");
    EXPECT_TRUE(contains(ir, "--run-all, --test-index=N, or --list required"));
}

// ============================================================================
// list_tests function structure
// ============================================================================

TEST(DispatcherGenTest, ListTestsEmitsJsonArray) {
    std::vector<DispatcherTestInfo> tests = {
        {"test_concat", "basic.test.tml", 0},
        {"test_split", "split.test.tml", 1},
    };
    auto ir = generate_ndjson_dispatcher_ir(tests, "suite");

    // list_tests should be defined
    EXPECT_TRUE(contains(ir, "define i32 @list_tests()"));
}

// ============================================================================
// run_all_tests phi nodes
// ============================================================================

TEST(DispatcherGenTest, RunAllTestsCounterPhis) {
    std::vector<DispatcherTestInfo> tests = {
        {"test_a", "a.test.tml", 0},
        {"test_b", "b.test.tml", 1},
    };
    auto ir = generate_ndjson_dispatcher_ir(tests, "suite");

    // Second test should have phi nodes for counters
    EXPECT_TRUE(contains(ir, "%pass_cnt_1 = phi i32"));
    EXPECT_TRUE(contains(ir, "%fail_cnt_1 = phi i32"));
}
