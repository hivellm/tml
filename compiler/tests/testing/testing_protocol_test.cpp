//! # NDJSON Protocol Unit Tests
//!
//! Tests for the NDJSON test protocol serialization and parsing:
//! - JSON escape (2.1.1)
//! - Event serialization round-trip (2.1.1)
//! - Event parsing (2.1.3)
//! - Test list serialization (2.1.5)
//! - Error handling (2.1.3)

#include "testing/testing_protocol.hpp"

#include <gtest/gtest.h>
#include <string>
#include <variant>

using namespace tml::testing;

// ============================================================================
// JSON escape tests
// ============================================================================

TEST(ProtocolTest, JsonEscapeBasicStrings) {
    EXPECT_EQ(json_escape("hello"), "hello");
    EXPECT_EQ(json_escape(""), "");
    EXPECT_EQ(json_escape("test_concat"), "test_concat");
}

TEST(ProtocolTest, JsonEscapeSpecialChars) {
    EXPECT_EQ(json_escape("line1\nline2"), "line1\\nline2");
    EXPECT_EQ(json_escape("col\ttab"), "col\\ttab");
    EXPECT_EQ(json_escape("say \"hi\""), "say \\\"hi\\\"");
    EXPECT_EQ(json_escape("back\\slash"), "back\\\\slash");
    EXPECT_EQ(json_escape("cr\r"), "cr\\r");
}

TEST(ProtocolTest, JsonEscapeControlChars) {
    std::string input(1, '\x01');
    EXPECT_EQ(json_escape(input), "\\u0001");

    std::string nul(1, '\0');
    EXPECT_EQ(json_escape(nul), "\\u0000");
}

// ============================================================================
// Serialization tests — each event type
// ============================================================================

TEST(ProtocolTest, SerializeSuiteStart) {
    SuiteStartEvent e;
    e.name = "core_str";
    e.test_count = 12;
    e.file_count = 3;

    auto json = event_to_json(e);
    EXPECT_NE(json.find("\"event\":\"suite_start\""), std::string::npos);
    EXPECT_NE(json.find("\"name\":\"core_str\""), std::string::npos);
    EXPECT_NE(json.find("\"test_count\":12"), std::string::npos);
    EXPECT_NE(json.find("\"file_count\":3"), std::string::npos);
}

TEST(ProtocolTest, SerializeTestStart) {
    TestStartEvent e;
    e.index = 5;
    e.name = "test_concat";
    e.file = "basic.test.tml";

    auto json = event_to_json(e);
    EXPECT_NE(json.find("\"event\":\"test_start\""), std::string::npos);
    EXPECT_NE(json.find("\"index\":5"), std::string::npos);
    EXPECT_NE(json.find("\"name\":\"test_concat\""), std::string::npos);
    EXPECT_NE(json.find("\"file\":\"basic.test.tml\""), std::string::npos);
}

TEST(ProtocolTest, SerializeTestOutput) {
    TestOutputEvent e;
    e.index = 0;
    e.stream = "stdout";
    e.data = "hello\nworld";

    auto json = event_to_json(e);
    EXPECT_NE(json.find("\"event\":\"test_output\""), std::string::npos);
    EXPECT_NE(json.find("\"stream\":\"stdout\""), std::string::npos);
    // Newline should be escaped
    EXPECT_NE(json.find("hello\\nworld"), std::string::npos);
}

TEST(ProtocolTest, SerializeTestPass) {
    TestPassEvent e;
    e.index = 3;
    e.duration_us = 1523;

    auto json = event_to_json(e);
    EXPECT_NE(json.find("\"event\":\"test_pass\""), std::string::npos);
    EXPECT_NE(json.find("\"index\":3"), std::string::npos);
    EXPECT_NE(json.find("\"duration_us\":1523"), std::string::npos);
}

TEST(ProtocolTest, SerializeTestFail) {
    TestFailEvent e;
    e.index = 1;
    e.name = "test_split";
    e.error = "assert_eq: expected 3, got 2";
    e.file = "basic.test.tml";
    e.line = 42;
    e.exit_code = 1;
    e.duration_us = 892;

    auto json = event_to_json(e);
    EXPECT_NE(json.find("\"event\":\"test_fail\""), std::string::npos);
    EXPECT_NE(json.find("\"name\":\"test_split\""), std::string::npos);
    EXPECT_NE(json.find("\"error\":\"assert_eq: expected 3, got 2\""), std::string::npos);
    EXPECT_NE(json.find("\"line\":42"), std::string::npos);
    EXPECT_NE(json.find("\"exit_code\":1"), std::string::npos);
}

TEST(ProtocolTest, SerializeTestCrash) {
    TestCrashEvent e;
    e.index = 2;
    e.name = "test_bad";
    e.signal = "SIGSEGV";
    e.duration_us = 0;

    auto json = event_to_json(e);
    EXPECT_NE(json.find("\"event\":\"test_crash\""), std::string::npos);
    EXPECT_NE(json.find("\"signal\":\"SIGSEGV\""), std::string::npos);
}

TEST(ProtocolTest, SerializeTestTimeout) {
    TestTimeoutEvent e;
    e.index = 3;
    e.name = "test_hang";
    e.timeout_ms = 20000;

    auto json = event_to_json(e);
    EXPECT_NE(json.find("\"event\":\"test_timeout\""), std::string::npos);
    EXPECT_NE(json.find("\"timeout_ms\":20000"), std::string::npos);
}

TEST(ProtocolTest, SerializeTestSkip) {
    TestSkipEvent e;
    e.index = 4;
    e.name = "test_platform";
    e.reason = "windows-only";

    auto json = event_to_json(e);
    EXPECT_NE(json.find("\"event\":\"test_skip\""), std::string::npos);
    EXPECT_NE(json.find("\"reason\":\"windows-only\""), std::string::npos);
}

TEST(ProtocolTest, SerializeCoverage) {
    CoverageEvent e;
    e.functions = {"str_concat", "str_split", "str_len"};
    e.hits = {5, 3, 0};

    auto json = event_to_json(e);
    EXPECT_NE(json.find("\"event\":\"coverage\""), std::string::npos);
    EXPECT_NE(json.find("\"functions\":[\"str_concat\",\"str_split\",\"str_len\"]"),
              std::string::npos);
    EXPECT_NE(json.find("\"hits\":[5,3,0]"), std::string::npos);
}

TEST(ProtocolTest, SerializeSuiteEnd) {
    SuiteEndEvent e;
    e.passed = 10;
    e.failed = 1;
    e.crashed = 1;
    e.timed_out = 1;
    e.skipped = 1;
    e.duration_us = 45230;

    auto json = event_to_json(e);
    EXPECT_NE(json.find("\"event\":\"suite_end\""), std::string::npos);
    EXPECT_NE(json.find("\"passed\":10"), std::string::npos);
    EXPECT_NE(json.find("\"failed\":1"), std::string::npos);
    EXPECT_NE(json.find("\"duration_us\":45230"), std::string::npos);
}

// ============================================================================
// Variant serialization
// ============================================================================

TEST(ProtocolTest, VariantSerialization) {
    ProtocolEvent event = TestPassEvent{3, 1000};
    auto json = event_to_json(event);
    EXPECT_NE(json.find("\"event\":\"test_pass\""), std::string::npos);
    EXPECT_NE(json.find("\"index\":3"), std::string::npos);
}

// ============================================================================
// Round-trip tests — serialize then parse, verify fields match
// ============================================================================

TEST(ProtocolTest, RoundtripSuiteStart) {
    SuiteStartEvent orig;
    orig.name = "core_str";
    orig.test_count = 12;
    orig.file_count = 3;

    auto result = parse_json_event(event_to_json(orig));
    ASSERT_TRUE(result.ok) << result.error;
    ASSERT_TRUE(std::holds_alternative<SuiteStartEvent>(result.event));

    auto& parsed = std::get<SuiteStartEvent>(result.event);
    EXPECT_EQ(parsed.name, "core_str");
    EXPECT_EQ(parsed.test_count, 12);
    EXPECT_EQ(parsed.file_count, 3);
}

TEST(ProtocolTest, RoundtripTestStart) {
    TestStartEvent orig;
    orig.index = 5;
    orig.name = "test_concat";
    orig.file = "basic.test.tml";

    auto result = parse_json_event(event_to_json(orig));
    ASSERT_TRUE(result.ok) << result.error;
    auto& parsed = std::get<TestStartEvent>(result.event);
    EXPECT_EQ(parsed.index, 5);
    EXPECT_EQ(parsed.name, "test_concat");
    EXPECT_EQ(parsed.file, "basic.test.tml");
}

TEST(ProtocolTest, RoundtripTestOutput) {
    TestOutputEvent orig;
    orig.index = 0;
    orig.stream = "stderr";
    orig.data = "error: line 1\nline 2\ttab";

    auto result = parse_json_event(event_to_json(orig));
    ASSERT_TRUE(result.ok) << result.error;
    auto& parsed = std::get<TestOutputEvent>(result.event);
    EXPECT_EQ(parsed.index, 0);
    EXPECT_EQ(parsed.stream, "stderr");
    EXPECT_EQ(parsed.data, "error: line 1\nline 2\ttab");
}

TEST(ProtocolTest, RoundtripTestPass) {
    TestPassEvent orig;
    orig.index = 7;
    orig.duration_us = 123456;

    auto result = parse_json_event(event_to_json(orig));
    ASSERT_TRUE(result.ok) << result.error;
    auto& parsed = std::get<TestPassEvent>(result.event);
    EXPECT_EQ(parsed.index, 7);
    EXPECT_EQ(parsed.duration_us, 123456);
}

TEST(ProtocolTest, RoundtripTestFail) {
    TestFailEvent orig;
    orig.index = 1;
    orig.name = "test_split";
    orig.error = "assert_eq: expected \"hello\", got \"world\"";
    orig.file = "str/split.test.tml";
    orig.line = 42;
    orig.exit_code = 1;
    orig.duration_us = 892;

    auto result = parse_json_event(event_to_json(orig));
    ASSERT_TRUE(result.ok) << result.error;
    auto& parsed = std::get<TestFailEvent>(result.event);
    EXPECT_EQ(parsed.index, 1);
    EXPECT_EQ(parsed.name, "test_split");
    EXPECT_EQ(parsed.error, "assert_eq: expected \"hello\", got \"world\"");
    EXPECT_EQ(parsed.file, "str/split.test.tml");
    EXPECT_EQ(parsed.line, 42);
    EXPECT_EQ(parsed.exit_code, 1);
    EXPECT_EQ(parsed.duration_us, 892);
}

TEST(ProtocolTest, RoundtripTestCrash) {
    TestCrashEvent orig;
    orig.index = 2;
    orig.name = "test_deref_null";
    orig.signal = "ACCESS_VIOLATION";
    orig.duration_us = 50;

    auto result = parse_json_event(event_to_json(orig));
    ASSERT_TRUE(result.ok) << result.error;
    auto& parsed = std::get<TestCrashEvent>(result.event);
    EXPECT_EQ(parsed.index, 2);
    EXPECT_EQ(parsed.signal, "ACCESS_VIOLATION");
}

TEST(ProtocolTest, RoundtripTestTimeout) {
    TestTimeoutEvent orig;
    orig.index = 3;
    orig.name = "test_infinite_loop";
    orig.timeout_ms = 30000;

    auto result = parse_json_event(event_to_json(orig));
    ASSERT_TRUE(result.ok) << result.error;
    auto& parsed = std::get<TestTimeoutEvent>(result.event);
    EXPECT_EQ(parsed.timeout_ms, 30000);
}

TEST(ProtocolTest, RoundtripCoverage) {
    CoverageEvent orig;
    orig.functions = {"fn_a", "fn_b", "fn_c"};
    orig.hits = {10, 0, 5};

    auto result = parse_json_event(event_to_json(orig));
    ASSERT_TRUE(result.ok) << result.error;
    auto& parsed = std::get<CoverageEvent>(result.event);
    ASSERT_EQ(parsed.functions.size(), 3u);
    EXPECT_EQ(parsed.functions[0], "fn_a");
    EXPECT_EQ(parsed.functions[2], "fn_c");
    ASSERT_EQ(parsed.hits.size(), 3u);
    EXPECT_EQ(parsed.hits[0], 10);
    EXPECT_EQ(parsed.hits[1], 0);
    EXPECT_EQ(parsed.hits[2], 5);
}

TEST(ProtocolTest, RoundtripSuiteEnd) {
    SuiteEndEvent orig;
    orig.passed = 10;
    orig.failed = 2;
    orig.crashed = 1;
    orig.timed_out = 0;
    orig.skipped = 3;
    orig.duration_us = 999999;

    auto result = parse_json_event(event_to_json(orig));
    ASSERT_TRUE(result.ok) << result.error;
    auto& parsed = std::get<SuiteEndEvent>(result.event);
    EXPECT_EQ(parsed.passed, 10);
    EXPECT_EQ(parsed.failed, 2);
    EXPECT_EQ(parsed.crashed, 1);
    EXPECT_EQ(parsed.timed_out, 0);
    EXPECT_EQ(parsed.skipped, 3);
    EXPECT_EQ(parsed.duration_us, 999999);
}

// ============================================================================
// Test list serialization
// ============================================================================

TEST(ProtocolTest, TestListToJson) {
    std::vector<TestMetadata> tests = {
        {"test_concat", "basic.test.tml", 0},
        {"test_split", "basic.test.tml", 1},
        {"test_trim", "trim.test.tml", 2},
    };

    auto json = test_list_to_json(tests);
    EXPECT_EQ(json[0], '[');
    EXPECT_EQ(json.back(), ']');
    EXPECT_NE(json.find("\"name\":\"test_concat\""), std::string::npos);
    EXPECT_NE(json.find("\"file\":\"trim.test.tml\""), std::string::npos);
    EXPECT_NE(json.find("\"index\":2"), std::string::npos);
}

TEST(ProtocolTest, TestListEmpty) {
    auto json = test_list_to_json({});
    EXPECT_EQ(json, "[]");
}

// ============================================================================
// Parser error handling
// ============================================================================

TEST(ProtocolTest, ParseEmptyLine) {
    auto result = parse_json_event("");
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("empty"), std::string::npos);
}

TEST(ProtocolTest, ParseInvalidJson) {
    auto result = parse_json_event("not json at all");
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("invalid JSON"), std::string::npos);
}

TEST(ProtocolTest, ParseUnknownEventType) {
    auto result = parse_json_event("{\"event\":\"unknown_type\"}");
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("unknown event type"), std::string::npos);
}

TEST(ProtocolTest, ParseMissingEventField) {
    auto result = parse_json_event("{\"name\":\"test\"}");
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("unknown event type"), std::string::npos);
}

TEST(ProtocolTest, ParseWithWindowsLineEnding) {
    // Simulate \r at end (Windows line ending)
    std::string line = event_to_json(TestPassEvent{0, 100}) + "\r";
    auto result = parse_json_event(line);
    ASSERT_TRUE(result.ok) << result.error;
    ASSERT_TRUE(std::holds_alternative<TestPassEvent>(result.event));
}

// ============================================================================
// Edge cases
// ============================================================================

TEST(ProtocolTest, EmptyCoverageArrays) {
    CoverageEvent orig;
    // empty functions and hits
    auto result = parse_json_event(event_to_json(orig));
    ASSERT_TRUE(result.ok) << result.error;
    auto& parsed = std::get<CoverageEvent>(result.event);
    EXPECT_TRUE(parsed.functions.empty());
    EXPECT_TRUE(parsed.hits.empty());
}

TEST(ProtocolTest, LargeIndex) {
    TestPassEvent orig;
    orig.index = 9999;
    orig.duration_us = 0;

    auto result = parse_json_event(event_to_json(orig));
    ASSERT_TRUE(result.ok);
    EXPECT_EQ(std::get<TestPassEvent>(result.event).index, 9999);
}

TEST(ProtocolTest, UnicodeInTestName) {
    TestStartEvent orig;
    orig.index = 0;
    orig.name = "test_utf8_\xc3\xa9"; // "test_utf8_é" in UTF-8
    orig.file = "unicode.test.tml";

    auto json = event_to_json(orig);
    auto result = parse_json_event(json);
    ASSERT_TRUE(result.ok) << result.error;
    auto& parsed = std::get<TestStartEvent>(result.event);
    EXPECT_EQ(parsed.name, "test_utf8_\xc3\xa9");
}
