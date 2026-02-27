TML_MODULE("test")

//! # NDJSON Test Protocol — Serialization & Parsing
//!
//! Hand-written JSON serialization and parsing. No external JSON library —
//! the protocol is simple enough that a hand-written implementation is smaller,
//! faster, and has zero dependencies.

#include "testing/testing_protocol.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <sstream>

namespace tml::testing {

// ============================================================================
// JSON escape
// ============================================================================

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + s.size() / 8); // slight overallocation

    for (char c : s) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                // Control character — emit \u00XX
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                out += buf;
            } else {
                out += c;
            }
        }
    }
    return out;
}

// ============================================================================
// Serialization helpers
// ============================================================================

static void append_field(std::ostringstream& os, const char* key, const std::string& value,
                         bool comma = true) {
    if (comma)
        os << ',';
    os << '"' << key << "\":\"" << json_escape(value) << '"';
}

static void append_field(std::ostringstream& os, const char* key, int value, bool comma = true) {
    if (comma)
        os << ',';
    os << '"' << key << "\":" << value;
}

static void append_field(std::ostringstream& os, const char* key, int64_t value,
                         bool comma = true) {
    if (comma)
        os << ',';
    os << '"' << key << "\":" << value;
}

// ============================================================================
// Serialization — event to JSON
// ============================================================================

std::string event_to_json(const SuiteStartEvent& e) {
    std::ostringstream os;
    os << "{\"event\":\"" << EVENT_SUITE_START << '"';
    append_field(os, "name", e.name);
    append_field(os, "test_count", e.test_count);
    append_field(os, "file_count", e.file_count);
    os << '}';
    return os.str();
}

std::string event_to_json(const TestStartEvent& e) {
    std::ostringstream os;
    os << "{\"event\":\"" << EVENT_TEST_START << '"';
    append_field(os, "index", e.index);
    append_field(os, "name", e.name);
    append_field(os, "file", e.file);
    os << '}';
    return os.str();
}

std::string event_to_json(const TestOutputEvent& e) {
    std::ostringstream os;
    os << "{\"event\":\"" << EVENT_TEST_OUTPUT << '"';
    append_field(os, "index", e.index);
    append_field(os, "stream", e.stream);
    append_field(os, "data", e.data);
    os << '}';
    return os.str();
}

std::string event_to_json(const TestPassEvent& e) {
    std::ostringstream os;
    os << "{\"event\":\"" << EVENT_TEST_PASS << '"';
    append_field(os, "index", e.index);
    append_field(os, "duration_us", e.duration_us);
    os << '}';
    return os.str();
}

std::string event_to_json(const TestFailEvent& e) {
    std::ostringstream os;
    os << "{\"event\":\"" << EVENT_TEST_FAIL << '"';
    append_field(os, "index", e.index);
    append_field(os, "name", e.name);
    append_field(os, "error", e.error);
    append_field(os, "file", e.file);
    append_field(os, "line", e.line);
    append_field(os, "exit_code", e.exit_code);
    append_field(os, "duration_us", e.duration_us);
    os << '}';
    return os.str();
}

std::string event_to_json(const TestCrashEvent& e) {
    std::ostringstream os;
    os << "{\"event\":\"" << EVENT_TEST_CRASH << '"';
    append_field(os, "index", e.index);
    append_field(os, "name", e.name);
    append_field(os, "signal", e.signal);
    append_field(os, "duration_us", e.duration_us);
    os << '}';
    return os.str();
}

std::string event_to_json(const TestTimeoutEvent& e) {
    std::ostringstream os;
    os << "{\"event\":\"" << EVENT_TEST_TIMEOUT << '"';
    append_field(os, "index", e.index);
    append_field(os, "name", e.name);
    append_field(os, "timeout_ms", e.timeout_ms);
    os << '}';
    return os.str();
}

std::string event_to_json(const TestSkipEvent& e) {
    std::ostringstream os;
    os << "{\"event\":\"" << EVENT_TEST_SKIP << '"';
    append_field(os, "index", e.index);
    append_field(os, "name", e.name);
    append_field(os, "reason", e.reason);
    os << '}';
    return os.str();
}

std::string event_to_json(const CoverageEvent& e) {
    std::ostringstream os;
    os << "{\"event\":\"" << EVENT_COVERAGE << '"';
    os << ",\"functions\":[";
    for (size_t i = 0; i < e.functions.size(); ++i) {
        if (i > 0)
            os << ',';
        os << '"' << json_escape(e.functions[i]) << '"';
    }
    os << "],\"hits\":[";
    for (size_t i = 0; i < e.hits.size(); ++i) {
        if (i > 0)
            os << ',';
        os << e.hits[i];
    }
    os << "]}";
    return os.str();
}

std::string event_to_json(const SuiteEndEvent& e) {
    std::ostringstream os;
    os << "{\"event\":\"" << EVENT_SUITE_END << '"';
    append_field(os, "passed", e.passed);
    append_field(os, "failed", e.failed);
    append_field(os, "crashed", e.crashed);
    append_field(os, "timed_out", e.timed_out);
    append_field(os, "skipped", e.skipped);
    append_field(os, "duration_us", e.duration_us);
    os << '}';
    return os.str();
}

std::string event_to_json(const ProtocolEvent& event) {
    return std::visit([](const auto& e) { return event_to_json(e); }, event);
}

// ============================================================================
// Test list serialization (--list mode)
// ============================================================================

std::string test_list_to_json(const std::vector<TestMetadata>& tests) {
    std::ostringstream os;
    os << '[';
    for (size_t i = 0; i < tests.size(); ++i) {
        if (i > 0)
            os << ',';
        os << "{\"name\":\"" << json_escape(tests[i].name) << '"';
        os << ",\"file\":\"" << json_escape(tests[i].file) << '"';
        os << ",\"index\":" << tests[i].index << '}';
    }
    os << ']';
    return os.str();
}

// ============================================================================
// Parsing — minimal JSON line parser
// ============================================================================
//
// We only need to parse the specific JSON format we emit. This is NOT a
// general-purpose JSON parser — it handles the subset we generate.

namespace {

// Skip whitespace, return position after whitespace
size_t skip_ws(const std::string& s, size_t pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t'))
        ++pos;
    return pos;
}

// Parse a JSON string value starting at pos (which should point to opening ")
// Returns the unescaped string and position after closing "
struct StringResult {
    bool ok = false;
    std::string value;
    size_t end_pos = 0;
};

StringResult parse_json_string(const std::string& s, size_t pos) {
    if (pos >= s.size() || s[pos] != '"')
        return {false, "", pos};

    ++pos; // skip opening "
    std::string result;
    result.reserve(32);

    while (pos < s.size()) {
        char c = s[pos];
        if (c == '"') {
            return {true, result, pos + 1};
        }
        if (c == '\\') {
            ++pos;
            if (pos >= s.size())
                return {false, "", pos};
            char esc = s[pos];
            switch (esc) {
            case '"':
                result += '"';
                break;
            case '\\':
                result += '\\';
                break;
            case '/':
                result += '/';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            case 'b':
                result += '\b';
                break;
            case 'f':
                result += '\f';
                break;
            case 'u': {
                // Parse \uXXXX — only ASCII range supported
                if (pos + 4 >= s.size())
                    return {false, "", pos};
                std::string hex = s.substr(pos + 1, 4);
                unsigned int cp = 0;
                for (char h : hex) {
                    cp <<= 4;
                    if (h >= '0' && h <= '9')
                        cp |= (h - '0');
                    else if (h >= 'a' && h <= 'f')
                        cp |= (h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F')
                        cp |= (h - 'A' + 10);
                    else
                        return {false, "", pos};
                }
                if (cp < 0x80) {
                    result += static_cast<char>(cp);
                } else if (cp < 0x800) {
                    result += static_cast<char>(0xC0 | (cp >> 6));
                    result += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    result += static_cast<char>(0xE0 | (cp >> 12));
                    result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (cp & 0x3F));
                }
                pos += 4;
                break;
            }
            default:
                result += esc;
            }
        } else {
            result += c;
        }
        ++pos;
    }
    return {false, "", pos}; // unterminated string
}

// Parse a JSON integer starting at pos
struct IntResult {
    bool ok = false;
    int64_t value = 0;
    size_t end_pos = 0;
};

IntResult parse_json_int(const std::string& s, size_t pos) {
    if (pos >= s.size())
        return {false, 0, pos};

    bool negative = false;
    if (s[pos] == '-') {
        negative = true;
        ++pos;
    }

    if (pos >= s.size() || s[pos] < '0' || s[pos] > '9')
        return {false, 0, pos};

    int64_t val = 0;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
        val = val * 10 + (s[pos] - '0');
        ++pos;
    }

    return {true, negative ? -val : val, pos};
}

// Parse a JSON int array: [1, 2, 3]
struct IntArrayResult {
    bool ok = false;
    std::vector<int> values;
    size_t end_pos = 0;
};

IntArrayResult parse_json_int_array(const std::string& s, size_t pos) {
    if (pos >= s.size() || s[pos] != '[')
        return {false, {}, pos};
    ++pos;
    pos = skip_ws(s, pos);

    std::vector<int> values;
    if (pos < s.size() && s[pos] == ']')
        return {true, values, pos + 1};

    while (pos < s.size()) {
        pos = skip_ws(s, pos);
        auto ir = parse_json_int(s, pos);
        if (!ir.ok)
            return {false, {}, pos};
        values.push_back(static_cast<int>(ir.value));
        pos = skip_ws(s, ir.end_pos);
        if (pos < s.size() && s[pos] == ']')
            return {true, values, pos + 1};
        if (pos < s.size() && s[pos] == ',') {
            ++pos;
            continue;
        }
        return {false, {}, pos};
    }
    return {false, {}, pos};
}

// Parse a JSON string array: ["a", "b"]
struct StringArrayResult {
    bool ok = false;
    std::vector<std::string> values;
    size_t end_pos = 0;
};

StringArrayResult parse_json_string_array(const std::string& s, size_t pos) {
    if (pos >= s.size() || s[pos] != '[')
        return {false, {}, pos};
    ++pos;
    pos = skip_ws(s, pos);

    std::vector<std::string> values;
    if (pos < s.size() && s[pos] == ']')
        return {true, values, pos + 1};

    while (pos < s.size()) {
        pos = skip_ws(s, pos);
        auto sr = parse_json_string(s, pos);
        if (!sr.ok)
            return {false, {}, pos};
        values.push_back(std::move(sr.value));
        pos = skip_ws(s, sr.end_pos);
        if (pos < s.size() && s[pos] == ']')
            return {true, values, pos + 1};
        if (pos < s.size() && s[pos] == ',') {
            ++pos;
            continue;
        }
        return {false, {}, pos};
    }
    return {false, {}, pos};
}

// Simple key-value pair collection from a JSON object
struct KVMap {
    struct Value {
        enum Type { STRING, INTEGER, STRING_ARRAY, INT_ARRAY };
        Type type;
        std::string str_val;
        int64_t int_val = 0;
        std::vector<std::string> str_arr;
        std::vector<int> int_arr;
    };
    std::vector<std::pair<std::string, Value>> entries;

    const Value* get(const std::string& key) const {
        for (auto& [k, v] : entries) {
            if (k == key)
                return &v;
        }
        return nullptr;
    }

    std::string get_str(const std::string& key, const std::string& def = "") const {
        auto* v = get(key);
        return (v && v->type == Value::STRING) ? v->str_val : def;
    }

    int64_t get_int(const std::string& key, int64_t def = 0) const {
        auto* v = get(key);
        return (v && v->type == Value::INTEGER) ? v->int_val : def;
    }

    int get_int32(const std::string& key, int def = 0) const {
        return static_cast<int>(get_int(key, def));
    }
};

// Parse a flat JSON object into a KVMap
struct ObjectResult {
    bool ok = false;
    KVMap map;
    size_t end_pos = 0;
};

ObjectResult parse_json_object(const std::string& s, size_t pos) {
    pos = skip_ws(s, pos);
    if (pos >= s.size() || s[pos] != '{')
        return {false, {}, pos};
    ++pos;

    KVMap map;

    while (true) {
        pos = skip_ws(s, pos);
        if (pos >= s.size())
            return {false, {}, pos};
        if (s[pos] == '}')
            return {true, map, pos + 1};

        // Parse key
        auto key = parse_json_string(s, pos);
        if (!key.ok)
            return {false, {}, pos};
        pos = skip_ws(s, key.end_pos);

        // Expect colon
        if (pos >= s.size() || s[pos] != ':')
            return {false, {}, pos};
        ++pos;
        pos = skip_ws(s, pos);

        // Parse value — detect type by first character
        if (pos >= s.size())
            return {false, {}, pos};

        KVMap::Value val;
        if (s[pos] == '"') {
            auto sr = parse_json_string(s, pos);
            if (!sr.ok)
                return {false, {}, pos};
            val.type = KVMap::Value::STRING;
            val.str_val = std::move(sr.value);
            pos = sr.end_pos;
        } else if (s[pos] == '[') {
            // Peek at first non-whitespace element to determine array type
            size_t peek = skip_ws(s, pos + 1);
            if (peek < s.size() && s[peek] == '"') {
                auto ar = parse_json_string_array(s, pos);
                if (!ar.ok)
                    return {false, {}, pos};
                val.type = KVMap::Value::STRING_ARRAY;
                val.str_arr = std::move(ar.values);
                pos = ar.end_pos;
            } else {
                auto ar = parse_json_int_array(s, pos);
                if (!ar.ok)
                    return {false, {}, pos};
                val.type = KVMap::Value::INT_ARRAY;
                val.int_arr = std::move(ar.values);
                pos = ar.end_pos;
            }
        } else if (s[pos] == '-' || (s[pos] >= '0' && s[pos] <= '9')) {
            auto ir = parse_json_int(s, pos);
            if (!ir.ok)
                return {false, {}, pos};
            val.type = KVMap::Value::INTEGER;
            val.int_val = ir.value;
            pos = ir.end_pos;
        } else {
            return {false, {}, pos}; // unexpected value type
        }

        map.entries.emplace_back(key.value, std::move(val));

        pos = skip_ws(s, pos);
        if (pos < s.size() && s[pos] == ',') {
            ++pos;
            continue;
        }
        // no comma → must be closing brace
    }
}

} // anonymous namespace

// ============================================================================
// Public parse function
// ============================================================================

ParseResult parse_json_event(const std::string& line) {
    // Strip trailing \r if present (Windows line endings)
    std::string clean = line;
    if (!clean.empty() && clean.back() == '\r')
        clean.pop_back();

    if (clean.empty())
        return {false, {}, "empty line"};

    auto obj = parse_json_object(clean, 0);
    if (!obj.ok)
        return {false, {}, "invalid JSON"};

    auto& m = obj.map;
    std::string event_type = m.get_str("event");

    if (event_type == EVENT_SUITE_START) {
        SuiteStartEvent e;
        e.name = m.get_str("name");
        e.test_count = m.get_int32("test_count");
        e.file_count = m.get_int32("file_count");
        return {true, e, ""};
    }

    if (event_type == EVENT_TEST_START) {
        TestStartEvent e;
        e.index = m.get_int32("index");
        e.name = m.get_str("name");
        e.file = m.get_str("file");
        return {true, e, ""};
    }

    if (event_type == EVENT_TEST_OUTPUT) {
        TestOutputEvent e;
        e.index = m.get_int32("index");
        e.stream = m.get_str("stream");
        e.data = m.get_str("data");
        return {true, e, ""};
    }

    if (event_type == EVENT_TEST_PASS) {
        TestPassEvent e;
        e.index = m.get_int32("index");
        e.duration_us = m.get_int("duration_us");
        return {true, e, ""};
    }

    if (event_type == EVENT_TEST_FAIL) {
        TestFailEvent e;
        e.index = m.get_int32("index");
        e.name = m.get_str("name");
        e.error = m.get_str("error");
        e.file = m.get_str("file");
        e.line = m.get_int32("line");
        e.exit_code = m.get_int32("exit_code");
        e.duration_us = m.get_int("duration_us");
        return {true, e, ""};
    }

    if (event_type == EVENT_TEST_CRASH) {
        TestCrashEvent e;
        e.index = m.get_int32("index");
        e.name = m.get_str("name");
        e.signal = m.get_str("signal");
        e.duration_us = m.get_int("duration_us");
        return {true, e, ""};
    }

    if (event_type == EVENT_TEST_TIMEOUT) {
        TestTimeoutEvent e;
        e.index = m.get_int32("index");
        e.name = m.get_str("name");
        e.timeout_ms = m.get_int("timeout_ms");
        return {true, e, ""};
    }

    if (event_type == EVENT_TEST_SKIP) {
        TestSkipEvent e;
        e.index = m.get_int32("index");
        e.name = m.get_str("name");
        e.reason = m.get_str("reason");
        return {true, e, ""};
    }

    if (event_type == EVENT_COVERAGE) {
        CoverageEvent e;
        auto* funcs = m.get("functions");
        if (funcs && funcs->type == KVMap::Value::STRING_ARRAY)
            e.functions = funcs->str_arr;
        auto* hits = m.get("hits");
        if (hits && hits->type == KVMap::Value::INT_ARRAY)
            e.hits = hits->int_arr;
        return {true, e, ""};
    }

    if (event_type == EVENT_SUITE_END) {
        SuiteEndEvent e;
        e.passed = m.get_int32("passed");
        e.failed = m.get_int32("failed");
        e.crashed = m.get_int32("crashed");
        e.timed_out = m.get_int32("timed_out");
        e.skipped = m.get_int32("skipped");
        e.duration_us = m.get_int("duration_us");
        return {true, e, ""};
    }

    return {false, {}, "unknown event type: " + event_type};
}

} // namespace tml::testing
