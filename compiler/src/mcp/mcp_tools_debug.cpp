TML_MODULE("tools")

//! # MCP Compiler Tools — Debug/Inspector Handlers
//!
//! Tool definitions and handlers for inspect, profile, and debug tools.
//! Extracted from mcp_tools.cpp to keep individual translation units
//! under ~400 lines.

#include "mcp_tools_internal.hpp"

#include <filesystem>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace tml::mcp {

// ============================================================================
// inspect Tool
// ============================================================================

auto make_inspect_tool() -> Tool {
    return Tool{.name = "inspect",
                .description = "Run a TML program with Chrome DevTools inspector enabled. "
                               "Returns the WebSocket URL for connecting DevTools.",
                .parameters = {
                    {"file", "string", "Path to the source file", true},
                    {"port", "number", "Inspector port (default: 9229)", false},
                    {"brk", "boolean", "Break before user code (wait for debugger)", false},
                }};
}

auto handle_inspect(const json::JsonValue& params) -> ToolResult {
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    if (!fs::exists(file_path)) {
        return ToolResult::error("File not found: " + file_path);
    }

    std::string tml_exe = get_tml_executable();
    std::stringstream cmd;
    cmd << tml_exe << " run " << file_path << " --inspect";

    auto* port_param = params.get("port");
    if (port_param != nullptr && port_param->is_number()) {
        cmd << " --inspect-port=" << port_param->as_i64();
    }

    auto* brk_param = params.get("brk");
    if (brk_param != nullptr && brk_param->is_bool() && brk_param->as_bool()) {
        cmd << " --inspect-brk";
    }

    auto [output, exit_code] = execute_command(cmd.str());
    std::string clean_output = strip_ansi(output);

    std::stringstream result;
    result << "Inspector started.\n";
    result << "Exit code: " << exit_code << "\n";
    if (!clean_output.empty()) {
        result << "\n--- Output ---\n" << clean_output;
    }

    if (exit_code == 0) {
        return ToolResult::text(result.str());
    }
    return ToolResult::error(result.str());
}

// ============================================================================
// profile Tool
// ============================================================================

auto make_profile_tool() -> Tool {
    return Tool{
        .name = "profile",
        .description = "Run a TML program with CPU profiling enabled, "
                       "generating a .cpuprofile file for Chrome DevTools. "
                       "Optionally generate a flame graph.",
        .parameters = {
            {"file", "string", "Path to the source file", true},
            {"output", "string",
             "Output path for .cpuprofile (default: build/debug/profile.cpuprofile)", false},
            {"flamegraph", "boolean", "Also generate an ASCII flame graph from the profile", false},
            {"svg", "string", "Generate SVG flame graph to this path", false},
        }};
}

auto handle_profile(const json::JsonValue& params) -> ToolResult {
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    if (!fs::exists(file_path)) {
        return ToolResult::error("File not found: " + file_path);
    }

    std::string tml_exe = get_tml_executable();
    std::string profile_path = "build/debug/profile.cpuprofile";

    auto* output_param = params.get("output");
    if (output_param != nullptr && output_param->is_string()) {
        profile_path = output_param->as_string();
    }

    // Run with profiling
    std::stringstream cmd;
    cmd << tml_exe << " run " << file_path << " --profile=" << profile_path;

    auto [output, exit_code] = execute_command(cmd.str());
    std::string clean_output = strip_ansi(output);

    std::stringstream result;
    result << "Profiling complete.\n";
    result << "Profile saved to: " << profile_path << "\n";
    result << "Exit code: " << exit_code << "\n";

    if (!clean_output.empty()) {
        result << "\n--- Program Output ---\n" << clean_output;
    }

    // Generate ASCII flame graph if requested
    auto* flame_param = params.get("flamegraph");
    if (flame_param != nullptr && flame_param->is_bool() && flame_param->as_bool()) {
        std::stringstream fg_cmd;
        fg_cmd << tml_exe << " profile flamegraph " << profile_path << " --ascii";
        auto [fg_output, fg_exit] = execute_command(fg_cmd.str());
        if (fg_exit == 0 && !fg_output.empty()) {
            result << "\n--- Flame Graph ---\n" << fg_output;
        }
    }

    // Generate SVG flame graph if requested
    auto* svg_param = params.get("svg");
    if (svg_param != nullptr && svg_param->is_string()) {
        std::string svg_path = svg_param->as_string();
        std::stringstream svg_cmd;
        svg_cmd << tml_exe << " profile flamegraph " << profile_path << " -o " << svg_path;
        auto [svg_output, svg_exit] = execute_command(svg_cmd.str());
        if (svg_exit == 0) {
            result << "\nSVG flame graph saved to: " << svg_path << "\n";
        }
    }

    if (exit_code == 0) {
        return ToolResult::text(result.str());
    }
    return ToolResult::error(result.str());
}

// ============================================================================
// debug Tool
// ============================================================================

auto make_debug_tool() -> Tool {
    return Tool{.name = "debug",
                .description = "Run a TML program with debug info and backtrace enabled. "
                               "Shows stack traces on panics.",
                .parameters = {
                    {"file", "string", "Path to the source file", true},
                    {"args", "array", "Arguments to pass to the program", false},
                    {"backtrace", "boolean", "Enable backtrace on panic (default: true)", false},
                    {"check_leaks", "boolean", "Enable memory leak checking", false},
                }};
}

auto handle_debug(const json::JsonValue& params) -> ToolResult {
    auto* file_param = params.get("file");
    if (file_param == nullptr || !file_param->is_string()) {
        return ToolResult::error("Missing or invalid 'file' parameter");
    }
    std::string file_path = file_param->as_string();

    if (!fs::exists(file_path)) {
        return ToolResult::error("File not found: " + file_path);
    }

    std::string tml_exe = get_tml_executable();

    // Backtrace is enabled by default; only omit if explicitly set to false
    auto* backtrace_param = params.get("backtrace");
    bool enable_backtrace =
        (backtrace_param == nullptr) || (!backtrace_param->is_bool()) || backtrace_param->as_bool();

    std::stringstream cmd;
    cmd << tml_exe << " run " << file_path;

    if (enable_backtrace) {
        cmd << " --backtrace";
    }

    auto* leaks_param = params.get("check_leaks");
    if (leaks_param != nullptr && leaks_param->is_bool() && leaks_param->as_bool()) {
        cmd << " --check-leaks";
    }

    auto* args_param = params.get("args");
    if (args_param != nullptr && args_param->is_array()) {
        for (const auto& arg : args_param->as_array()) {
            if (arg.is_string()) {
                cmd << " " << arg.as_string();
            }
        }
    }

    auto [output, exit_code] = execute_command(cmd.str());
    std::string clean_output = strip_ansi(output);

    std::stringstream result;
    result << "Exit code: " << exit_code << "\n";
    if (!clean_output.empty()) {
        result << "\n--- Output ---\n" << clean_output;
    }

    if (exit_code == 0) {
        return ToolResult::text(result.str());
    }
    return ToolResult::error(result.str());
}

} // namespace tml::mcp
