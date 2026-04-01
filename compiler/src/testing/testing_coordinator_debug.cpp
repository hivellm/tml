TML_MODULE("test")

//! # Test Coordinator — Debug Layers
//!
//! Implements `emit_debug_layers_for_failures`: on test failure, re-invokes
//! the TML compiler to emit HIR, MIR, and LLVM IR for the failing source file,
//! extracts the relevant function section from each layer, and appends it to
//! the test's error message together with diagnosis hints.
//!
//! All functions in this file are internal to the testing module and are not
//! part of the public testing API.

#include "log/log.hpp"
#include "testing/testing_coordinator_internal.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace tml::testing {

// ============================================================================
// Debug Layers — emit multi-layer IR diagnostics for failing tests
// ============================================================================

/// Find the tml executable (same logic as MCP tools).
static std::string find_tml_exe() {
#ifdef _WIN32
    std::vector<std::string> paths = {
        "build/debug/bin/tml.exe",
        "build/debug/tml.exe",
        "build/release/bin/tml.exe",
        "tml.exe",
    };
#else
    std::vector<std::string> paths = {
        "build/debug/bin/tml",
        "build/debug/tml",
        "build/release/bin/tml",
        "tml",
    };
#endif
    for (const auto& p : paths) {
        if (fs::exists(p)) {
            return fs::absolute(p).string();
        }
    }
    return "tml";
}

/// Run a command and capture output (simplified popen wrapper).
static std::string run_capture(const std::string& cmd) {
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (pipe == nullptr) {
        return "[debug-layers] Failed to run: " + cmd;
    }
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        result += buf;
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

/// Read a text file into a string (returns empty on failure).
static std::string read_file_contents(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// Extract only the LLVM IR for a specific function from a .ll file.
/// Searches for `define ... @<func_name>(` and captures until the closing `}`.
static std::string extract_function_ir(const std::string& full_ir, const std::string& func_name) {
    // Search for the function definition
    std::string marker = "@" + func_name + "(";
    auto pos = full_ir.find(marker);
    if (pos == std::string::npos) {
        // Try mangled name patterns: @tml_<func_name> or @test_<func_name>
        marker = "@test_" + func_name + "(";
        pos = full_ir.find(marker);
    }
    if (pos == std::string::npos) {
        // Return first 200 lines as fallback
        std::string truncated;
        int lines = 0;
        for (size_t i = 0; i < full_ir.size() && lines < 200; ++i) {
            truncated += full_ir[i];
            if (full_ir[i] == '\n') {
                lines++;
            }
        }
        if (lines >= 200) {
            truncated += "\n... (truncated, " + std::to_string(full_ir.size()) + " bytes total)\n";
        }
        return truncated;
    }

    // Find the `define` keyword before the marker
    auto line_start = full_ir.rfind('\n', pos);
    if (line_start == std::string::npos) {
        line_start = 0;
    } else {
        line_start++;
    }

    // Find the closing `}` (function-level, at column 0)
    auto end_pos = pos;
    int brace_depth = 0;
    bool in_func = false;
    for (size_t i = line_start; i < full_ir.size(); ++i) {
        if (full_ir[i] == '{') {
            brace_depth++;
            in_func = true;
        } else if (full_ir[i] == '}') {
            brace_depth--;
            if (in_func && brace_depth == 0) {
                end_pos = i + 1;
                break;
            }
        }
    }

    return full_ir.substr(line_start, end_pos - line_start);
}

/// Extract a function from MIR text output by name.
/// MIR functions start with "fn <name>(" and end with "}" at indent 0.
static std::string extract_mir_function(const std::string& full_mir, const std::string& func_name) {
    // Search for "fn <name>" or "fn test_<name>"
    std::string marker = "fn " + func_name + "(";
    auto pos = full_mir.find(marker);
    if (pos == std::string::npos) {
        marker = "fn test_" + func_name + "(";
        pos = full_mir.find(marker);
    }
    if (pos == std::string::npos) {
        // Try substring match
        for (size_t i = 0; i < full_mir.size(); ++i) {
            if (full_mir.substr(i, 3) == "fn " &&
                full_mir.find(func_name, i) < full_mir.find('\n', i)) {
                pos = i;
                break;
            }
        }
    }
    if (pos == std::string::npos) {
        // Return first 100 lines as fallback
        std::string truncated;
        int lines = 0;
        for (size_t i = 0; i < full_mir.size() && lines < 100; ++i) {
            truncated += full_mir[i];
            if (full_mir[i] == '\n') {
                lines++;
            }
        }
        if (lines >= 100) {
            truncated += "\n... (truncated)\n";
        }
        return truncated;
    }

    // Find the start of the line containing "fn"
    auto line_start = full_mir.rfind('\n', pos);
    line_start = (line_start == std::string::npos) ? 0 : line_start + 1;

    // Find closing "}" at brace depth 0
    auto end_pos = pos;
    int brace_depth = 0;
    bool in_func = false;
    for (size_t i = line_start; i < full_mir.size(); ++i) {
        if (full_mir[i] == '{') {
            brace_depth++;
            in_func = true;
        } else if (full_mir[i] == '}') {
            brace_depth--;
            if (in_func && brace_depth == 0) {
                end_pos = i + 1;
                break;
            }
        }
    }

    return full_mir.substr(line_start, end_pos - line_start);
}

/// Analyze error text and emitted IR layers to generate diagnosis hints.
/// Looks for common patterns that indicate which compilation layer has the bug.
static std::string generate_diagnosis_hints(const std::string& error_text,
                                            const std::string& test_name) {
    std::string hints;
    hints += "\n\n=== DIAGNOSIS HINTS ===\n";

    bool has_hir = error_text.find("=== HIR") != std::string::npos;
    bool has_mir = error_text.find("=== MIR") != std::string::npos;
    (void)has_hir; // Used in condition below

    // Pattern: "Could not generate" indicates compilation failure at that layer
    bool hir_failed = error_text.find("Could not generate HIR") != std::string::npos;
    bool mir_failed = error_text.find("Could not generate MIR") != std::string::npos;
    bool ir_failed = error_text.find("Could not generate IR") != std::string::npos;

    if (hir_failed) {
        hints += "Layer: PARSER or TYPE SYSTEM\n";
        hints += "Symptom: HIR generation failed — source didn't parse or type-check\n";
        hints += "Possible causes:\n";
        hints += "  - Syntax error in source file\n";
        hints += "  - Unresolved type or import\n";
        hints += "  - Missing impl for a behavior\n";
        return hints;
    }

    if (mir_failed && has_hir) {
        hints += "Layer: HIR → MIR LOWERING\n";
        hints += "Symptom: HIR succeeded but MIR generation failed\n";
        hints += "Possible causes:\n";
        hints += "  - Monomorphization failure (generic not instantiated)\n";
        hints += "  - Closure capture analysis bug\n";
        hints += "  - Desugaring produced invalid HIR\n";
        return hints;
    }

    if (ir_failed && has_mir) {
        hints += "Layer: CODEGEN (MIR → LLVM IR)\n";
        hints += "Symptom: MIR succeeded but LLVM IR generation failed\n";
        hints += "Possible causes:\n";
        hints += "  - Type layout mismatch in codegen\n";
        hints += "  - Missing instruction handler in MirCodegen\n";
        hints += "  - ABI/calling convention error\n";
        return hints;
    }

    // All layers generated — analyze content patterns
    // Pattern: assertion failure (runtime bug)
    if (error_text.find("assert") != std::string::npos ||
        error_text.find("ASSERT") != std::string::npos) {
        // Check for common codegen patterns in the LLVM IR
        bool has_sret = error_text.find("sret") != std::string::npos;
        bool has_void_call = error_text.find("call void") != std::string::npos;
        bool has_type_mismatch = error_text.find("type mismatch") != std::string::npos ||
                                 error_text.find("invalid type") != std::string::npos;

        if (has_type_mismatch) {
            hints += "Layer: CODEGEN\n";
            hints += "Symptom: Type mismatch in generated LLVM IR\n";
            hints += "Possible causes:\n";
            hints += "  - Struct passed by value when pointer expected (or vice versa)\n";
            hints += "  - sret convention mismatch between caller and callee\n";
            hints += "  - Integer width mismatch (i32 vs i64)\n";
        } else if (has_sret && has_void_call) {
            hints += "Layer: CODEGEN (calling convention)\n";
            hints += "Symptom: sret + void call pattern — possible return value corruption\n";
            hints += "Possible causes:\n";
            hints += "  - Function returns struct but caller expects void\n";
            hints += "  - sret parameter not properly forwarded\n";
        } else {
            hints += "Layer: RUNTIME or LIBRARY\n";
            hints += "Symptom: Assertion failure with all compilation layers looking correct\n";
            hints += "Possible causes:\n";
            hints += "  - Logic error in TML library code\n";
            hints += "  - C runtime function returning wrong value\n";
            hints += "  - Memory layout mismatch between TML and C runtime\n";
        }
    } else if (error_text.find("exit code") != std::string::npos ||
               error_text.find("crashed") != std::string::npos) {
        hints += "Layer: CODEGEN or RUNTIME\n";
        hints += "Symptom: Process crashed (non-assertion failure)\n";
        hints += "Possible causes:\n";
        hints += "  - Null pointer dereference from incorrect codegen\n";
        hints += "  - Stack corruption from ABI mismatch\n";
        hints += "  - Use-after-free from incorrect drop ordering\n";
    } else {
        hints += "Layer: UNKNOWN\n";
        hints += "Symptom: Test failed but pattern not recognized\n";
        hints += "Action: Compare HIR → MIR → LLVM IR manually for the failing function\n";
    }

    if (!test_name.empty()) {
        hints += "Focus function: ";
        hints += test_name;
        hints += "\n";
    }

    return hints;
}

/// For each failing test, emit MIR + LLVM IR and append to the test's error message.
void emit_debug_layers_for_failures(TestRunResult& result) {
    std::string tml_exe = find_tml_exe();
    std::set<std::string> already_emitted; // Avoid duplicate for same source file

    for (auto& suite : result.suites) {
        for (auto& test : suite.tests) {
            if (test.passed || test.file.empty()) {
                continue;
            }

            // Normalize the source file path
            std::string src_file = test.file;
            std::replace(src_file.begin(), src_file.end(), '\\', '/');

            if (already_emitted.contains(src_file)) {
                test.error += "\n\n=== DEBUG LAYERS ===\n(see above — same source file)\n";
                continue;
            }
            already_emitted.insert(src_file);

            fs::path src_path(src_file);
            std::string stem = src_path.stem().string();

            // --- HIR Layer ---
            TML_LOG_INFO("test", "[debug-layers] Emitting HIR for: " << src_file);
            std::string hir_cmd = "\"";
            hir_cmd += tml_exe;
            hir_cmd += "\" build \"";
            hir_cmd += src_file;
            hir_cmd += "\" --emit-hir --legacy 2>&1";
            run_capture(hir_cmd);

            fs::path hir_path = fs::path("build/debug") / (stem + ".hir");
            if (fs::exists(hir_path)) {
                std::string full_hir = read_file_contents(hir_path);
                if (!full_hir.empty()) {
                    // HIR uses "func <name>" syntax, same extraction as MIR
                    std::string hir_section;
                    if (!test.name.empty()) {
                        hir_section = extract_mir_function(full_hir, test.name);
                    } else {
                        hir_section = extract_mir_function(full_hir, "");
                    }
                    test.error += "\n\n=== HIR (--debug-layers) ===\n";
                    test.error += hir_section;
                }
            } else {
                test.error += "\n\n=== HIR ===\n[debug-layers] Could not generate HIR\n";
            }

            // --- MIR Layer ---
            TML_LOG_INFO("test", "[debug-layers] Emitting MIR for: " << src_file);
            std::string mir_cmd = "\"";
            mir_cmd += tml_exe;
            mir_cmd += "\" build \"";
            mir_cmd += src_file;
            mir_cmd += "\" --emit-mir --legacy 2>&1";
            run_capture(mir_cmd);

            fs::path mir_path = fs::path("build/debug") / (stem + ".mir");
            if (fs::exists(mir_path)) {
                std::string full_mir = read_file_contents(mir_path);
                if (!full_mir.empty()) {
                    std::string mir_section;
                    if (!test.name.empty()) {
                        mir_section = extract_mir_function(full_mir, test.name);
                    } else {
                        mir_section = extract_mir_function(full_mir, "");
                    }
                    test.error += "\n\n=== MIR (--debug-layers) ===\n";
                    test.error += mir_section;
                }
            } else {
                test.error += "\n\n=== MIR ===\n[debug-layers] Could not generate MIR\n";
            }

            // --- LLVM IR Layer ---
            TML_LOG_INFO("test", "[debug-layers] Emitting LLVM IR for: " << src_file);
            std::string ir_cmd = "\"";
            ir_cmd += tml_exe;
            ir_cmd += "\" build \"";
            ir_cmd += src_file;
            ir_cmd += "\" --emit-ir --legacy 2>&1";
            run_capture(ir_cmd);

            fs::path ll_path = fs::path("build/debug") / (stem + ".ll");
            if (fs::exists(ll_path)) {
                std::string full_ir = read_file_contents(ll_path);
                if (!full_ir.empty()) {
                    std::string ir_section;
                    if (!test.name.empty()) {
                        ir_section = extract_function_ir(full_ir, test.name);
                    } else {
                        ir_section = extract_function_ir(full_ir, "");
                    }
                    test.error += "\n\n=== LLVM IR (--debug-layers) ===\n";
                    test.error += ir_section;
                }
            } else {
                test.error += "\n\n=== LLVM IR ===\n[debug-layers] Could not generate IR\n";
            }

            // --- Diagnosis Hints ---
            // Analyze the error message and emitted layers to suggest the likely bug layer
            test.error += generate_diagnosis_hints(test.error, test.name);
        }
    }
}

} // namespace tml::testing
