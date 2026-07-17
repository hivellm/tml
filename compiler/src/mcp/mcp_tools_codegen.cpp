TML_MODULE("tools")

//! # MCP Compiler Tools — Codegen & Test Handlers
//!
//! Handlers for emit-ir, emit-mir, test, format, and lint tools.
//! Extracted from mcp_tools.cpp to keep individual translation units
//! under ~400 lines.

#include "mcp_tools_internal.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace tml::mcp {

auto handle_emit_ir(const json::JsonValue& params) -> ToolResult {
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    if (!fs::exists(file_path)) {
        return ToolResult::error("File not found: " + file_path);
    }

    // Use subprocess execution — the in-process parse_and_check + codegen lacks
    // full module resolution context, causing crashes on any file with imports.
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " build " << file_path << " --emit-ir";

    // Add optimization level if specified
    auto* optimize_param = params.get("optimize");
    if (optimize_param != nullptr && optimize_param->is_string()) {
        std::string opt = optimize_param->as_string();
        if (opt == "O0" || opt == "O1" || opt == "O2" || opt == "O3") {
            cmd << " -" << opt;
        }
    }

    // Route through the warm daemon (phase40a) — `build --emit-ir` is
    // forwardable and its result is cached by input mtime. The .ll artifact
    // read back below is produced on the cache-miss run and persists on disk.
    auto [output, exit_code] = execute_command(cmd.str(), 120, /*use_daemon=*/true);

    if (exit_code != 0) {
        return ToolResult::error("emit-ir failed (exit code " + std::to_string(exit_code) +
                                 ")\n\n" + strip_ansi(output));
    }

    // The CLI writes IR to build/debug/<module_name>.ll — read it back
    std::string module_name = fs::path(file_path).stem().string();
    std::string ll_path_str = "build/debug/" + module_name + ".ll";

    auto ir_opt = read_source_file(ll_path_str);
    std::string ir = ir_opt.has_value() ? *ir_opt : "(could not read " + ll_path_str + ")";

    if (ir.size() > 500000) {
        ir = ir.substr(0, 500000) + "\n\n[... truncated at 500KB ...]";
    }

    // Apply function filter if specified
    auto* func_param = params.get("function");
    if (func_param != nullptr && func_param->is_string()) {
        std::string func_name = func_param->as_string();
        std::stringstream filtered;
        std::istringstream stream(ir);
        std::string line;
        bool in_function = false;
        int brace_depth = 0;

        while (std::getline(stream, line)) {
            if (!in_function) {
                if (line.find("define") != std::string::npos &&
                    line.find(func_name) != std::string::npos) {
                    in_function = true;
                    brace_depth = 0;
                    filtered << line << "\n";
                    for (char c : line) {
                        if (c == '{')
                            brace_depth++;
                        if (c == '}')
                            brace_depth--;
                    }
                }
            } else {
                filtered << line << "\n";
                for (char c : line) {
                    if (c == '{')
                        brace_depth++;
                    if (c == '}')
                        brace_depth--;
                }
                if (brace_depth <= 0) {
                    in_function = false;
                    filtered << "\n";
                }
            }
        }

        ir = filtered.str();
        if (ir.empty()) {
            ir = "Function '" + func_name + "' not found in IR output.\n";
        }
    }

    // Apply offset/limit for chunked output
    auto* offset_param = params.get("offset");
    auto* limit_param = params.get("limit");

    if ((offset_param != nullptr && offset_param->is_number()) ||
        (limit_param != nullptr && limit_param->is_number())) {
        int64_t offset = 0;
        int64_t limit = -1;
        if (offset_param != nullptr && offset_param->is_number()) {
            offset = offset_param->as_i64();
        }
        if (limit_param != nullptr && limit_param->is_number()) {
            limit = limit_param->as_i64();
        }

        std::istringstream stream(ir);
        std::string line;
        std::stringstream chunked;
        int64_t line_num = 0;
        int64_t total_lines = 0;

        // Count total lines first
        {
            std::istringstream counter(ir);
            std::string l;
            while (std::getline(counter, l))
                total_lines++;
        }

        while (std::getline(stream, line)) {
            if (line_num >= offset && (limit < 0 || line_num < offset + limit)) {
                chunked << line << "\n";
            }
            line_num++;
        }

        ir = chunked.str();

        // Add metadata
        std::stringstream meta;
        meta << "Lines: " << offset << "-"
             << std::min(offset + (limit > 0 ? limit : total_lines), total_lines) << " of "
             << total_lines << "\n\n";
        ir = meta.str() + ir;
    }

    return ToolResult::text(ir);
}

auto handle_emit_mir(const json::JsonValue& params) -> ToolResult {
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    if (!fs::exists(file_path)) {
        return ToolResult::error("File not found: " + file_path);
    }

    // Use subprocess execution — the in-process approach lacks full module
    // resolution context, causing crashes on any file with imports.
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " build " << file_path << " --emit-mir";

    // Route through the warm daemon (phase40a) — same rationale as emit-ir.
    auto [output, exit_code] = execute_command(cmd.str(), 120, /*use_daemon=*/true);

    if (exit_code != 0) {
        return ToolResult::error("emit-mir failed (exit code " + std::to_string(exit_code) +
                                 ")\n\n" + strip_ansi(output));
    }

    // The CLI writes MIR to build/debug/<module_name>.mir — read it back
    std::string module_name = fs::path(file_path).stem().string();
    fs::path mir_path = fs::path("build") / "debug" / (module_name + ".mir");
    auto mir_opt = read_source_file(mir_path.string());
    if (!mir_opt.has_value()) {
        return ToolResult::error("emit-mir succeeded but could not read output file: " +
                                 mir_path.string());
    }

    std::string mir = *mir_opt;
    if (mir.size() > 500000) {
        mir = mir.substr(0, 500000) + "\n\n[... truncated at 500KB ...]";
    }
    return ToolResult::text(mir);
}

auto handle_test(const json::JsonValue& params) -> ToolResult {
    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " test";

    // Add path if specified
    auto* path_param = params.get("path");
    if (path_param != nullptr && path_param->is_string()) {
        cmd << " " << path_param->as_string();
    }

    // Add filter if specified (maps to --filter=X for file path substring matching)
    auto* filter_param = params.get("filter");
    if (filter_param != nullptr && filter_param->is_string()) {
        cmd << " --filter=" << filter_param->as_string();
    }

    // Add suite filter (maps to --suite=X for suite group filtering)
    // e.g., suite="core/str" runs only str tests from lib/core
    auto* suite_param = params.get("suite");
    if (suite_param != nullptr && suite_param->is_string()) {
        cmd << " --suite=" << suite_param->as_string();
    }

    // Add release flag
    auto* release_param = params.get("release");
    if (release_param != nullptr && release_param->is_bool() && release_param->as_bool()) {
        cmd << " --release";
    }

    // Add coverage flag
    auto* coverage_param = params.get("coverage");
    if (coverage_param != nullptr && coverage_param->is_bool() && coverage_param->as_bool()) {
        cmd << " --coverage";
    }

    // Add profile flag
    auto* profile_param = params.get("profile");
    if (profile_param != nullptr && profile_param->is_bool() && profile_param->as_bool()) {
        cmd << " --profile";
    }

    // Always add --verbose: without it, tml test produces no stdout/stderr output
    // (all output is INFO-level log messages that only appear with --verbose).
    // The MCP test tool needs parseable output for both structured and text modes.
    cmd << " --verbose";

    // Add no-cache flag
    auto* no_cache_param = params.get("no_cache");
    if (no_cache_param != nullptr && no_cache_param->is_bool() && no_cache_param->as_bool()) {
        cmd << " --no-cache";
    }

    // Add fail-fast / no-fail-fast flag
    auto* fail_fast_param = params.get("fail_fast");
    if (fail_fast_param != nullptr && fail_fast_param->is_bool()) {
        if (fail_fast_param->as_bool()) {
            cmd << " --fail-fast";
        } else {
            cmd << " --no-fail-fast";
        }
    }

    // debug-layers: enabled by default (Condition B for LLM debugging research).
    // Can be explicitly disabled via debug_layers=false or TML_DEBUG_LAYERS=0.
    // When enabled, test failures include multi-layer IR diagnostics (HIR + MIR + LLVM IR).
    auto* debug_layers_param = params.get("debug_layers");
    bool debug_layers = true; // Default ON for Condition B
    if (debug_layers_param != nullptr && debug_layers_param->is_bool()) {
        debug_layers = debug_layers_param->as_bool();
    } else {
        const char* env_val = std::getenv("TML_DEBUG_LAYERS");
        if (env_val != nullptr && std::string(env_val) == "0") {
            debug_layers = false;
        }
    }
    if (debug_layers) {
        cmd << " --debug-layers";
    }

    // Timeout: 300s for normal tests, 600s for coverage/full suite
    auto* cov_check = params.get("coverage");
    bool is_full_suite = (path_param == nullptr || !path_param->is_string()) &&
                         (suite_param == nullptr || !suite_param->is_string()) &&
                         (filter_param == nullptr || !filter_param->is_string());
    int test_timeout = (cov_check && cov_check->is_bool() && cov_check->as_bool()) ? 600
                       : is_full_suite                                             ? 600
                                                                                   : 300;
    auto [output, exit_code] = execute_command(cmd.str(), test_timeout);

    // Check if structured output requested
    auto* structured_param = params.get("structured");
    if (structured_param != nullptr && structured_param->is_bool() && structured_param->as_bool()) {
        // Parse test output for structured results
        std::stringstream result;
        result << "{";

        // Parse v3 coordinator output format:
        //   "  Tests:   N" / "  Passed:  N" / "  Failed:  N"
        //   "  Crashed: N" / "  Compile errors: N"
        //   "FAIL group/name (file) [exit N]"
        //   "COMPILE ERROR suite: message"
        int total = 0, passed = 0, failed = 0, crashed = 0, compile_errors = 0;
        std::vector<std::string> failures;
        std::vector<std::string> timeouts;
        std::vector<std::string> diagnostics; // full error messages with file:line:col

        auto extract_number = [](const std::string& l, const std::string& key) -> int {
            auto pos = l.find(key);
            if (pos == std::string::npos)
                return -1;
            auto val_start = pos + key.size();
            while (val_start < l.size() && l[val_start] == ' ')
                ++val_start;
            try {
                return std::stoi(l.substr(val_start));
            } catch (...) {
                return -1;
            }
        };

        // Strip ANSI escape codes (\033[...m) so diagnostics are clean text
        auto strip_ansi_local = [](const std::string& s) -> std::string {
            std::string r;
            r.reserve(s.size());
            for (size_t i = 0; i < s.size();) {
                if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
                    i += 2;
                    while (i < s.size() && s[i] != 'm')
                        ++i;
                    if (i < s.size())
                        ++i;
                } else {
                    r += s[i++];
                }
            }
            return r;
        };

        // Escape a string for JSON embedding
        auto json_esc = [](const std::string& s) -> std::string {
            std::string r;
            for (char c : s) {
                if (c == '"')
                    r += "\\\"";
                else if (c == '\\')
                    r += "\\\\";
                else if (c == '\n')
                    r += "\\n";
                else if (c == '\r') {
                } else if (c == '\t')
                    r += "\\t";
                else
                    r += c;
            }
            return r;
        };

        std::istringstream stream(output);
        std::string line;
        bool in_fail_detail = false; // collecting indented error lines after a test FAIL
        while (std::getline(stream, line)) {
            std::string clean = strip_ansi_local(line);

            int v;
            if ((v = extract_number(clean, "Passed:")) >= 0)
                passed = v;
            if ((v = extract_number(clean, "Failed:")) >= 0)
                failed = v;
            if ((v = extract_number(clean, "Tests:")) >= 0)
                total = v;
            if ((v = extract_number(clean, "Crashed:")) >= 0)
                crashed = v;
            if ((v = extract_number(clean, "Compile errors:")) >= 0)
                compile_errors = v;

            // ── Diagnostics: capture errors with file:line:col detail ──────
            // [compile] SKIP file:line:col: error — per-file compile error
            if (clean.find("[compile] SKIP ") != std::string::npos) {
                auto pos = clean.find("[compile] SKIP ");
                diagnostics.push_back(clean.substr(pos + 15));
                in_fail_detail = false;
            }
            // [compile] FAIL suite.exe: error (N/M) — suite-level compile failure
            else if (clean.find("[compile] FAIL ") != std::string::npos) {
                auto pos = clean.find("[compile] FAIL ");
                std::string d = clean.substr(pos + 15);
                auto paren = d.rfind(" (");
                if (paren != std::string::npos)
                    d = d.substr(0, paren);
                if (!d.empty())
                    diagnostics.push_back(d);
                in_fail_detail = false;
            }
            // COMPILE ERROR suite: error (from reporter — failed suites)
            else if (clean.find("COMPILE ERROR") != std::string::npos) {
                auto pos = clean.find("COMPILE ERROR");
                diagnostics.push_back(clean.substr(pos));
                in_fail_detail = false;
            }
            // FAIL group/name (file) [exit N] — test failure header
            else if (clean.find("FAIL ") != std::string::npos &&
                     clean.find("[compile]") == std::string::npos &&
                     clean.find("COMPILE") == std::string::npos) {
                auto fail_pos = clean.find("FAIL ");
                diagnostics.push_back(clean.substr(fail_pos));
                in_fail_detail = true;
            }
            // Indented error detail following a test FAIL line
            else if (in_fail_detail) {
                // Skip log prefix "HH:MM:SS.mmm LEVEL [module] " to reach message content
                auto bracket = clean.find("] ");
                std::string msg =
                    (bracket != std::string::npos) ? clean.substr(bracket + 2) : clean;
                if (!msg.empty() && msg[0] == ' ') {
                    auto first = msg.find_first_not_of(' ');
                    if (first != std::string::npos)
                        diagnostics.push_back("  " + msg.substr(first));
                } else {
                    in_fail_detail = false;
                }
            } else {
                in_fail_detail = false;
            }
            // ─────────────────────────────────────────────────────────────

            // Collect failure names (backwards-compat field)
            if (clean.find("FAIL") != std::string::npos &&
                clean.find("COMPILE") == std::string::npos) {
                auto fail_pos = clean.find("FAIL");
                if (fail_pos != std::string::npos) {
                    auto name_start = clean.find_first_not_of(' ', fail_pos + 4);
                    if (name_start != std::string::npos) {
                        auto paren_pos = clean.find(" (", name_start);
                        auto end_pos = (paren_pos != std::string::npos) ? paren_pos : clean.size();
                        failures.push_back(clean.substr(name_start, end_pos - name_start));
                    }
                }
            }
            // Collect timeout details
            if (clean.find("TIMEOUT:") != std::string::npos) {
                auto timeout_pos = clean.find("TIMEOUT:");
                auto detail_start = clean.find_first_not_of(' ', timeout_pos);
                if (detail_start != std::string::npos)
                    timeouts.push_back(clean.substr(detail_start));
            }
            // Compile errors also added to failures list
            if (clean.find("COMPILE ERROR") != std::string::npos) {
                auto colon_pos = clean.find("COMPILE ERROR");
                if (colon_pos != std::string::npos) {
                    auto name_start = clean.find_first_not_of(' ', colon_pos + 13);
                    if (name_start != std::string::npos)
                        failures.push_back("[compile] " + clean.substr(name_start));
                }
            }
        }

        // If total wasn't explicitly set, derive from passed+failed
        if (total == 0 && (passed > 0 || failed > 0))
            total = passed + failed + crashed;

        int skipped = total - passed - failed - crashed;
        if (skipped < 0)
            skipped = 0;

        result << "\"total\":" << total << ",";
        result << "\"passed\":" << passed << ",";
        result << "\"failed\":" << failed << ",";
        result << "\"crashed\":" << crashed << ",";
        result << "\"skipped\":" << skipped << ",";
        result << "\"compile_errors\":" << compile_errors << ",";
        result << "\"failures\":[";
        for (size_t i = 0; i < failures.size(); ++i) {
            if (i > 0)
                result << ",";
            result << "\"" << json_esc(failures[i]) << "\"";
        }
        result << "],";
        result << "\"timeouts\":[";
        for (size_t i = 0; i < timeouts.size(); ++i) {
            if (i > 0)
                result << ",";
            result << "\"" << json_esc(timeouts[i]) << "\"";
        }
        result << "],";
        result << "\"diagnostics\":[";
        for (size_t i = 0; i < diagnostics.size(); ++i) {
            if (i > 0)
                result << ",";
            result << "\"" << json_esc(diagnostics[i]) << "\"";
        }
        result << "],";

        // Build docs hints from the raw output so the LLM can look up types
        // mentioned in compiler diagnostics without reading source files.
        {
            auto candidates = extract_hint_candidates(output);
            std::vector<std::string> hint_items;
            // Known type quick-reference hints
            if (output.find("Outcome") != std::string::npos ||
                output.find("Result") != std::string::npos) {
                hint_items.push_back("docs_get(id=\\\"core::error::Outcome\\\")");
            }
            if (output.find("Maybe") != std::string::npos ||
                output.find("Option") != std::string::npos) {
                hint_items.push_back("docs_get(id=\\\"core::types::option::Maybe\\\")");
            }
            if (output.find("Iterator") != std::string::npos) {
                hint_items.push_back("docs_list(module=\\\"core::iter\\\")");
            }
            // Per-candidate search hints (up to 5 additional)
            int count = 0;
            for (const auto& c : candidates) {
                if (count >= 5)
                    break;
                if (c.size() < 3 || c.size() >= 60)
                    continue;
                hint_items.push_back("docs_search(query=\\\"" + json_esc(c) + "\\\")");
                ++count;
            }
            result << "\"docs_hints\":[";
            for (size_t i = 0; i < hint_items.size(); ++i) {
                if (i > 0)
                    result << ",";
                result << "\"" << hint_items[i] << "\"";
            }
            result << "]";
        }

        result << "}";

        return ToolResult::text(result.str());
    }

    std::stringstream result;
    if (exit_code == 0) {
        result << "Tests passed!\n";
    } else {
        result << "Tests failed (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Test Output ---\n" << output;
    }

    if (exit_code != 0) {
        // Append docs hints for types mentioned in test output so the LLM
        // can look up APIs that appear in compiler diagnostics.
        std::string stripped = strip_ansi(output);
        result << build_docs_hints(stripped);
        return ToolResult::error(result.str());
    }

    return ToolResult::text(result.str());
}

auto handle_format(const json::JsonValue& params) -> ToolResult {
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " format " << file_path;

    auto* check_param = params.get("check");
    if (check_param != nullptr && check_param->is_bool() && check_param->as_bool()) {
        cmd << " --check";
    }

    auto [output, exit_code] = execute_command(cmd.str());

    std::stringstream result;
    if (exit_code == 0) {
        result << "Format successful!\n";
    } else {
        result << "Format issues found (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Output ---\n" << output;
    }

    return ToolResult::text(result.str());
}

auto handle_lint(const json::JsonValue& params) -> ToolResult {
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " lint " << file_path;

    auto* fix_param = params.get("fix");
    if (fix_param != nullptr && fix_param->is_bool() && fix_param->as_bool()) {
        cmd << " --fix";
    }

    auto [output, exit_code] = execute_command(cmd.str());

    std::stringstream result;
    if (exit_code == 0) {
        result << "Lint passed!\n";
    } else {
        result << "Lint found issues (exit code " << exit_code << ")\n";
    }

    if (!output.empty()) {
        result << "\n--- Output ---\n" << output;
    }

    // Lint errors are not fatal - return text even with non-zero exit
    return ToolResult::text(result.str());
}

} // namespace tml::mcp
