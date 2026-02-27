//! # NDJSON Test Protocol
//!
//! Defines the structured event protocol between test dispatcher (subprocess)
//! and coordinator (parent process). Events are newline-delimited JSON (NDJSON)
//! — one JSON object per line, no partial lines.
//!
//! ## Event Flow
//!
//!   suite_start
//!     test_start → (test_output*) → test_pass | test_fail | test_crash | test_timeout | test_skip
//!     ... (repeat per test)
//!   coverage?
//!   suite_end
//!
//! ## Design
//!
//! Inspired by Go's test2json protocol. Every event is self-contained — the
//! coordinator can parse events independently without maintaining state.

#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace tml::testing {

// ============================================================================
// Event types — emitted by dispatcher (subprocess), parsed by coordinator
// ============================================================================

struct SuiteStartEvent {
    std::string name;   // e.g. "core_str"
    int test_count = 0; // total tests in suite
    int file_count = 0; // number of source files
};

struct TestStartEvent {
    int index = 0;
    std::string name; // e.g. "test_concat"
    std::string file; // e.g. "basic.test.tml"
};

struct TestOutputEvent {
    int index = 0;
    std::string stream; // "stdout" or "stderr"
    std::string data;   // output text (may contain newlines, escaped in JSON)
};

struct TestPassEvent {
    int index = 0;
    int64_t duration_us = 0;
};

struct TestFailEvent {
    int index = 0;
    std::string name;
    std::string error; // e.g. "assert_eq: expected 3, got 2"
    std::string file;
    int line = 0;
    int exit_code = 0;
    int64_t duration_us = 0;
};

struct TestCrashEvent {
    int index = 0;
    std::string name;
    std::string signal; // e.g. "SIGSEGV", "ACCESS_VIOLATION"
    int64_t duration_us = 0;
};

struct TestTimeoutEvent {
    int index = 0;
    std::string name;
    int64_t timeout_ms = 0;
};

struct TestSkipEvent {
    int index = 0;
    std::string name;
    std::string reason; // e.g. "windows-only"
};

struct CoverageEvent {
    std::vector<std::string> functions;
    std::vector<int> hits;
};

struct SuiteEndEvent {
    int passed = 0;
    int failed = 0;
    int crashed = 0;
    int timed_out = 0;
    int skipped = 0;
    int64_t duration_us = 0;
};

/// Variant holding any protocol event.
using ProtocolEvent =
    std::variant<SuiteStartEvent, TestStartEvent, TestOutputEvent, TestPassEvent, TestFailEvent,
                 TestCrashEvent, TestTimeoutEvent, TestSkipEvent, CoverageEvent, SuiteEndEvent>;

// ============================================================================
// Event names — used as the "event" field in JSON
// ============================================================================

constexpr const char* EVENT_SUITE_START = "suite_start";
constexpr const char* EVENT_TEST_START = "test_start";
constexpr const char* EVENT_TEST_OUTPUT = "test_output";
constexpr const char* EVENT_TEST_PASS = "test_pass";
constexpr const char* EVENT_TEST_FAIL = "test_fail";
constexpr const char* EVENT_TEST_CRASH = "test_crash";
constexpr const char* EVENT_TEST_TIMEOUT = "test_timeout";
constexpr const char* EVENT_TEST_SKIP = "test_skip";
constexpr const char* EVENT_COVERAGE = "coverage";
constexpr const char* EVENT_SUITE_END = "suite_end";

// ============================================================================
// Serialization — event to JSON line (no trailing newline)
// ============================================================================

/// Serialize any event to a single JSON line (no trailing newline).
std::string event_to_json(const ProtocolEvent& event);

// Convenience overloads for each event type
std::string event_to_json(const SuiteStartEvent& e);
std::string event_to_json(const TestStartEvent& e);
std::string event_to_json(const TestOutputEvent& e);
std::string event_to_json(const TestPassEvent& e);
std::string event_to_json(const TestFailEvent& e);
std::string event_to_json(const TestCrashEvent& e);
std::string event_to_json(const TestTimeoutEvent& e);
std::string event_to_json(const TestSkipEvent& e);
std::string event_to_json(const CoverageEvent& e);
std::string event_to_json(const SuiteEndEvent& e);

// ============================================================================
// Parsing — JSON line to event
// ============================================================================

/// Result of parsing a JSON line.
struct ParseResult {
    bool ok = false;
    ProtocolEvent event;
    std::string error; // non-empty on failure
};

/// Parse a single NDJSON line into a ProtocolEvent.
/// The line should NOT include the trailing newline.
ParseResult parse_json_event(const std::string& line);

// ============================================================================
// Test metadata — for --list mode
// ============================================================================

struct TestMetadata {
    std::string name; // e.g. "test_concat"
    std::string file; // e.g. "basic.test.tml"
    int index = 0;
};

/// Serialize a list of test metadata to a JSON array string.
std::string test_list_to_json(const std::vector<TestMetadata>& tests);

// ============================================================================
// JSON helpers (internal, but exposed for testing)
// ============================================================================

/// Escape a string for JSON (handles \n, \r, \t, \", \\, control chars).
std::string json_escape(const std::string& s);

} // namespace tml::testing
