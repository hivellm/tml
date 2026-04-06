TML_MODULE("mcp")

//! # MCP Analyze Tool — Inline Research Queries
//!
//! Provides `analyze` MCP tool that reads mcp-call-log.jsonl and returns
//! JSON statistics for tool distribution, error rates, check adoption, etc.
//!
//! This allows the LLM to query its own debugging behavior without
//! external scripts.

#include "mcp/mcp_server.hpp"
#include "mcp/mcp_tools.hpp"
#include "mcp/mcp_types.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::mcp {

namespace {

struct LogEntry {
    std::string event;
    std::string session;
    std::string tool;
    bool is_error = false;
    std::string error_type;
    std::string preceded_by;
    std::string test_target;
    int64_t duration_ms = 0;
    int64_t ts = 0;
};

/// Simple JSON string value extractor (no full parser needed for flat objects).
std::string extract_json_string(const std::string& json, const std::string& key) {
    auto search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos)
        return "";
    auto start = pos + search.size();
    auto end = json.find('"', start);
    if (end == std::string::npos)
        return "";
    return json.substr(start, end - start);
}

int64_t extract_json_int(const std::string& json, const std::string& key) {
    auto search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos)
        return 0;
    auto start = pos + search.size();
    // Skip whitespace
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t'))
        start++;
    if (start >= json.size())
        return 0;
    bool neg = json[start] == '-';
    if (neg)
        start++;
    int64_t val = 0;
    while (start < json.size() && json[start] >= '0' && json[start] <= '9') {
        val = val * 10 + (json[start] - '0');
        start++;
    }
    return neg ? -val : val;
}

bool extract_json_bool(const std::string& json, const std::string& key) {
    auto search = "\"" + key + "\":true";
    return json.find(search) != std::string::npos;
}

fs::path find_log() {
    const char* env = std::getenv("TML_MCP_LOG_DIR");
    if (env && env[0]) {
        fs::path p = fs::path(env) / "mcp-call-log.jsonl";
        if (fs::exists(p))
            return p;
    }
    if (fs::exists("docs/papers/llm-ir-debugging/data/mcp-call-log.jsonl"))
        return "docs/papers/llm-ir-debugging/data/mcp-call-log.jsonl";
    if (fs::exists("mcp-call-log.jsonl"))
        return "mcp-call-log.jsonl";
    return {};
}

std::vector<LogEntry> load_entries(const fs::path& log_path) {
    std::vector<LogEntry> entries;
    // Use C fopen with share-read to avoid conflicts with the log writer
    FILE* fp = std::fopen(log_path.string().c_str(), "r");
    if (!fp)
        return entries;
    char line_buf[65536];
    int line_num = 0;
    int parsed = 0;
    while (std::fgets(line_buf, sizeof(line_buf), fp)) {
        line_num++;
        std::string line(line_buf);
        // Remove trailing newline
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (line.empty() || line[0] != '{')
            continue;
        try {
            LogEntry e;
            e.event = extract_json_string(line, "event");
            if (e.event != "tool_call")
                continue;
            e.session = extract_json_string(line, "session");
            e.tool = extract_json_string(line, "tool");
            e.is_error = extract_json_bool(line, "is_error");
            e.error_type = extract_json_string(line, "error_type");
            e.preceded_by = extract_json_string(line, "preceded_by");
            e.test_target = extract_json_string(line, "test_target");
            e.duration_ms = extract_json_int(line, "duration_ms");
            e.ts = extract_json_int(line, "ts");
            entries.push_back(std::move(e));
            parsed++;
        } catch (...) {
            std::fprintf(stderr, "[analyze] skipping bad line %d\n", line_num);
        }
    }
    std::fclose(fp);
    return entries;
}

std::string metric_tool_distribution(const std::vector<LogEntry>& entries) {
    std::map<std::string, int> counts;
    for (const auto& e : entries)
        counts[e.tool]++;
    int total = static_cast<int>(entries.size());

    std::ostringstream ss;
    ss << "{\"total\":" << total << ",\"tools\":{";
    bool first = true;
    for (const auto& [tool, count] : counts) {
        if (!first)
            ss << ",";
        ss << "\"" << tool << "\":{\"count\":" << count
           << ",\"pct\":" << (total > 0 ? 100.0 * count / total : 0.0) << "}";
        first = false;
    }
    ss << "}}";
    return ss.str();
}

std::string metric_error_rate(const std::vector<LogEntry>& entries) {
    std::map<std::string, std::pair<int, int>> by_tool; // total, errors
    for (const auto& e : entries) {
        by_tool[e.tool].first++;
        if (e.is_error || (e.error_type != "none" && !e.error_type.empty())) {
            by_tool[e.tool].second++;
        }
    }
    std::ostringstream ss;
    ss << "{\"tools\":{";
    bool first = true;
    for (const auto& [tool, counts] : by_tool) {
        if (!first)
            ss << ",";
        double rate = counts.first > 0 ? 100.0 * counts.second / counts.first : 0.0;
        ss << "\"" << tool << "\":{\"total\":" << counts.first << ",\"errors\":" << counts.second
           << ",\"rate\":" << rate << "}";
        first = false;
    }
    ss << "}}";
    return ss.str();
}

std::string metric_check_adoption(const std::vector<LogEntry>& entries) {
    int test_calls = 0, preceded = 0;
    for (const auto& e : entries) {
        if (e.tool == "test") {
            test_calls++;
            if (e.preceded_by == "check")
                preceded++;
        }
    }
    double rate = test_calls > 0 ? 100.0 * preceded / test_calls : 0.0;
    std::ostringstream ss;
    ss << "{\"test_calls\":" << test_calls << ",\"preceded_by_check\":" << preceded
       << ",\"adoption_pct\":" << rate << "}";
    return ss.str();
}

std::string metric_debug_layers_usage(const std::vector<LogEntry>& entries) {
    // Count from raw log — can't distinguish auto vs explicit from LogEntry alone
    int test_calls = 0;
    for (const auto& e : entries) {
        if (e.tool == "test")
            test_calls++;
    }
    std::ostringstream ss;
    ss << "{\"test_calls\":" << test_calls << "}";
    return ss.str();
}

std::string metric_test_target_hotspots(const std::vector<LogEntry>& entries) {
    std::map<std::string, std::pair<int, int>> by_target; // total, failures
    for (const auto& e : entries) {
        if (e.tool != "test")
            continue;
        std::string target = e.test_target.empty() ? "unknown" : e.test_target;
        by_target[target].first++;
        if (e.is_error || (e.error_type != "none" && !e.error_type.empty())) {
            by_target[target].second++;
        }
    }
    std::ostringstream ss;
    ss << "{\"targets\":{";
    bool first = true;
    for (const auto& [target, counts] : by_target) {
        if (!first)
            ss << ",";
        double rate = counts.first > 0 ? 100.0 * counts.second / counts.first : 0.0;
        ss << "\"" << target << "\":{\"total\":" << counts.first << ",\"failed\":" << counts.second
           << ",\"fail_rate\":" << rate << "}";
        first = false;
    }
    ss << "}}";
    return ss.str();
}

} // anonymous namespace

// ============================================================================
// Tool definition + handler
// ============================================================================

auto make_analyze_tool() -> Tool {
    return Tool{.name = "analyze",
                .description = "Analyze MCP call logs for research metrics. "
                               "Returns JSON statistics about LLM debugging behavior.",
                .parameters = {
                    {"metric", "string",
                     "Metric: tool_distribution, error_rate, check_adoption, "
                     "debug_layers_usage, test_target_hotspots",
                     true},
                }};
}

auto handle_analyze(const json::JsonValue& params) -> ToolResult {
    try {
        std::fprintf(stderr, "[analyze] handler entered\n");
        std::fflush(stderr);

        auto* metric_val = params.get("metric");
        if (!metric_val) {
            return ToolResult::error("Missing required parameter: metric");
        }
        std::string metric = metric_val->as_string();
        std::fprintf(stderr, "[analyze] metric=%s\n", metric.c_str());
        std::fflush(stderr);

        std::fprintf(stderr, "[analyze] cwd=%s\n", fs::current_path().string().c_str());
        std::fflush(stderr);
        auto log_path = find_log();
        if (log_path.empty()) {
            return ToolResult::error("mcp-call-log.jsonl not found. "
                                     "Set TML_MCP_LOG_DIR or run from project root.");
        }
        std::fprintf(stderr, "[analyze] log=%s\n", log_path.string().c_str());
        std::fflush(stderr);

        auto entries = load_entries(log_path);
        std::fprintf(stderr, "[analyze] loaded %zu entries\n", entries.size());
        std::fflush(stderr);

        if (entries.empty()) {
            return ToolResult::text("{\"entries\":0,\"log\":\"" + log_path.string() + "\"}");
        }

        std::string result;
        if (metric == "tool_distribution") {
            result = metric_tool_distribution(entries);
        } else if (metric == "error_rate") {
            result = metric_error_rate(entries);
        } else if (metric == "check_adoption") {
            result = metric_check_adoption(entries);
        } else if (metric == "debug_layers_usage") {
            result = metric_debug_layers_usage(entries);
        } else if (metric == "test_target_hotspots") {
            result = metric_test_target_hotspots(entries);
        } else {
            return ToolResult::error("Unknown metric: " + metric +
                                     ". Valid: tool_distribution, error_rate, check_adoption, "
                                     "debug_layers_usage, test_target_hotspots");
        }

        std::fprintf(stderr, "[analyze] done, result size=%zu\n", result.size());
        std::fflush(stderr);
        return ToolResult::text(result);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[analyze] EXCEPTION: %s\n", e.what());
        std::fflush(stderr);
        return ToolResult::error(std::string("analyze crashed: ") + e.what());
    } catch (...) {
        std::fprintf(stderr, "[analyze] UNKNOWN EXCEPTION\n");
        std::fflush(stderr);
        return ToolResult::error("analyze crashed with unknown exception");
    }
}

} // namespace tml::mcp
