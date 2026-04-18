TML_MODULE("compiler")

//! # `tml cc` — Compile C source to a native object
//!
//! Thin C++ driver that reads a file, routes it through the
//! `cc_bridge` FFI, and writes the resulting `.obj` via the existing
//! LLVM backend + LLD integration. Flag semantics mirror `clang -c`
//! where sensible.
//!
//! ## Status
//!
//! Phase24 Phase 3.1: flag parsing and pipeline skeleton. The
//! pipeline calls into `cc_bridge_*`, which currently returns stubs
//! (see `compiler/src/cc/cc_bridge.cpp`) — any invocation fails with
//! the stub diagnostic until Phase 2.2 wires the real dispatch. The
//! CLI surface itself is already stable enough for smoke-testing flag
//! behaviour and diagnostic rendering.

#include "cmd_cc.hpp"

#include "cc/cc_bridge.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

namespace {

// ============================================================================
// Option parsing
// ============================================================================

enum class EmitStage {
    Obj,       ///< Default: compile all the way to .obj
    LlvmIr,    ///< Stop after LLVM IR (write .ll)
    Mir,       ///< Stop after MIR
    Ast,       ///< Stop after parsing (print translation unit)
    Tokens,    ///< Stop after preprocessing (print token stream)
};

struct CcOptions {
    std::string              input;           ///< Positional: source file.
    std::string              output;          ///< -o <path>; empty -> auto.
    std::vector<std::string> include_paths;   ///< -I <path> (repeatable).
    std::vector<std::string> defines;         ///< -D NAME[=VAL] (repeatable).
    std::string              target_triple;   ///< -target <triple>; empty -> host.
    int                      opt_level    = 0; ///< -O0..-O3.
    bool                     compile_only = true; ///< -c always implied (we don't link).
    bool                     debug        = false; ///< -g.
    EmitStage                emit         = EmitStage::Obj;
    bool                     help         = false;
};

constexpr std::string_view kUsage =
    "Usage: tml cc <file.c> [options]\n"
    "\n"
    "Options:\n"
    "  -o <path>          Output path (default: <input-stem>.obj)\n"
    "  -c                 Compile only, do not link (default; accepted for clang parity)\n"
    "  -O0 | -O1 | -O2 | -O3\n"
    "                     Optimization level (default: -O0)\n"
    "  -I <path>          Add an include directory (repeatable)\n"
    "  -D <name>[=<val>]  Predefine a macro (repeatable)\n"
    "  -target <triple>   Target triple (default: host)\n"
    "                     Recognised: x86_64-pc-windows-msvc,\n"
    "                                 x86_64-unknown-linux-gnu,\n"
    "                                 i686-*, aarch64-*\n"
    "  -g                 Emit debug information\n"
    "  --emit=<stage>     Stop after <stage>:\n"
    "                       obj      — default; write object file\n"
    "                       llvm-ir  — write LLVM IR (.ll)\n"
    "                       mir      — print MIR\n"
    "                       ast      — print translation unit\n"
    "                       tokens   — print preproc tokens\n"
    "  -h, --help         Show this message\n";

/// Fetch the argument to a two-token flag (`-o foo.obj`) or the trailing
/// fragment of an `-oFOO`/`-o=foo.obj` form. Advances `i` on success.
bool take_value(int argc, char* argv[], int& i, const char* flag, std::string& out) {
    std::string_view a = argv[i];
    std::string_view f = flag;
    if (a == f) {
        if (i + 1 >= argc) {
            std::fprintf(stderr, "tml cc: %s requires a value\n", flag);
            return false;
        }
        out = argv[++i];
        return true;
    }
    // -Xvalue  (no separator)
    if (a.rfind(f, 0) == 0 && a.size() > f.size() && a[f.size()] != '=') {
        out = std::string(a.substr(f.size()));
        return true;
    }
    // -X=value
    if (a.rfind(std::string(f) + "=", 0) == 0) {
        out = std::string(a.substr(f.size() + 1));
        return true;
    }
    return false;
}

bool parse_options(int argc, char* argv[], CcOptions& opts) {
    // argv[0] = tml.exe, argv[1] = "cc"
    for (int i = 2; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "-h" || a == "--help") {
            opts.help = true;
            continue;
        }
        if (a == "-c") {
            opts.compile_only = true;
            continue;
        }
        if (a == "-g") {
            opts.debug = true;
            continue;
        }
        if (a == "-O0") {
            opts.opt_level = 0;
            continue;
        }
        if (a == "-O1") {
            opts.opt_level = 1;
            continue;
        }
        if (a == "-O2") {
            opts.opt_level = 2;
            continue;
        }
        if (a == "-O3") {
            opts.opt_level = 3;
            continue;
        }
        std::string buf;
        if (take_value(argc, argv, i, "-o", buf)) {
            opts.output = std::move(buf);
            continue;
        }
        if (take_value(argc, argv, i, "-I", buf)) {
            opts.include_paths.push_back(std::move(buf));
            continue;
        }
        if (take_value(argc, argv, i, "-D", buf)) {
            opts.defines.push_back(std::move(buf));
            continue;
        }
        if (take_value(argc, argv, i, "-target", buf)) {
            opts.target_triple = std::move(buf);
            continue;
        }
        if (a.rfind("--emit=", 0) == 0) {
            std::string_view v = a.substr(7);
            if (v == "obj") {
                opts.emit = EmitStage::Obj;
            } else if (v == "llvm-ir") {
                opts.emit = EmitStage::LlvmIr;
            } else if (v == "mir") {
                opts.emit = EmitStage::Mir;
            } else if (v == "ast") {
                opts.emit = EmitStage::Ast;
            } else if (v == "tokens") {
                opts.emit = EmitStage::Tokens;
            } else {
                std::fprintf(stderr, "tml cc: unknown --emit stage '%.*s'\n",
                             static_cast<int>(v.size()), v.data());
                return false;
            }
            continue;
        }
        if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "tml cc: unknown option '%s'\n", argv[i]);
            return false;
        }
        // Positional: first one wins.
        if (opts.input.empty()) {
            opts.input = argv[i];
            continue;
        }
        std::fprintf(stderr, "tml cc: more than one input file is not supported yet "
                             "(got '%s' after '%s')\n",
                     argv[i], opts.input.c_str());
        return false;
    }
    return true;
}

// ============================================================================
// ABI target selection
// ============================================================================

CcAbiTarget abi_for_triple(const std::string& triple) {
    if (triple.empty()) {
        return CC_ABI_TARGET_HOST;
    }
    // Case-insensitive sloppy substring matching — this is the `clang
    // -target` surface, which is already lenient.
    std::string t = triple;
    std::transform(t.begin(), t.end(), t.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (t.find("windows") != std::string::npos && t.find("x86_64") != std::string::npos) {
        return CC_ABI_TARGET_WINDOWS_X64_LLP64;
    }
    if (t.find("linux") != std::string::npos || t.find("unknown-") != std::string::npos) {
        if (t.find("x86_64") != std::string::npos || t.find("amd64") != std::string::npos) {
            return CC_ABI_TARGET_SYSV_AMD64_LP64;
        }
    }
    if (t.find("aarch64") != std::string::npos || t.find("arm64") != std::string::npos) {
        return CC_ABI_TARGET_AARCH64;
    }
    if (t.find("i686") != std::string::npos || t.find("i386") != std::string::npos ||
        t.find("x86-32") != std::string::npos) {
        return CC_ABI_TARGET_I686;
    }
    return CC_ABI_TARGET_HOST;
}

// ============================================================================
// Diagnostic rendering
// ============================================================================

/// Format a severity code into a short leader string. Matches the
/// common clang/rustc pattern (severity colour is left to the caller).
const char* severity_label(int32_t sev) {
    switch (sev) {
    case 0:
        return "note";
    case 1:
        return "warning";
    case 2:
        return "error";
    case 3:
    default:
        return "fatal";
    }
}

/// Print every diagnostic in the sink to stderr and return true if any
/// has severity >= 2 (error).
bool render_diagnostics(const CcDiagnostics* diags) {
    bool any_error = false;
    const size_t n  = cc_bridge_diagnostics_count(diags);
    for (size_t i = 0; i < n; ++i) {
        CcDiagnostic d = cc_bridge_diagnostics_get(diags, i);
        const char*  file = (d.file != nullptr && d.file[0] != '\0') ? d.file : "<cc>";
        if (d.line > 0 && d.column > 0) {
            std::fprintf(stderr, "%s:%d:%d: %s: %s\n", file, d.line, d.column,
                         severity_label(d.severity), d.message);
        } else {
            std::fprintf(stderr, "%s: %s: %s\n", file, severity_label(d.severity), d.message);
        }
        if (d.severity >= 2) {
            any_error = true;
        }
    }
    return any_error;
}

// ============================================================================
// File IO helpers
// ============================================================================

bool read_file(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "tml cc: cannot open '%s' for reading\n", path.c_str());
        return false;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    out = buf.str();
    return true;
}

std::string auto_output_path(const std::string& input, EmitStage stage) {
    fs::path p(input);
    switch (stage) {
    case EmitStage::Obj:
        return p.replace_extension(".obj").string();
    case EmitStage::LlvmIr:
        return p.replace_extension(".ll").string();
    case EmitStage::Mir:
        return p.replace_extension(".mir").string();
    case EmitStage::Ast:
        return p.replace_extension(".ast").string();
    case EmitStage::Tokens:
        return p.replace_extension(".tokens").string();
    }
    return p.replace_extension(".obj").string();
}

// ============================================================================
// NULL-terminated C array helpers (for the cc_bridge ABI)
// ============================================================================

struct CStrArray {
    std::vector<const char*> ptrs;
    explicit CStrArray(const std::vector<std::string>& strs) {
        ptrs.reserve(strs.size() + 1);
        for (const auto& s : strs) {
            ptrs.push_back(s.c_str());
        }
        ptrs.push_back(nullptr);
    }
    const char* const* data() const {
        return ptrs.data();
    }
};

/// Locate a pre-built `cc_driver.exe` in the standard output dirs.
/// Returns an empty path if none is present.
fs::path find_cc_driver() {
    fs::path build = fs::current_path() / "build";
    std::vector<fs::path> candidates = {
        build / "debug" / "bin" / "cc_driver.exe",
        build / "release" / "bin" / "cc_driver.exe",
        build / "debug" / "cc_driver.exe",
        build / "release" / "cc_driver.exe",
    };
    for (const auto& c : candidates) {
        if (fs::exists(c)) {
            return c;
        }
    }
    return {};
}

/// Translate a `CcOptions` into a `cc_driver` argv. The TML-side
/// driver accepts a narrower flag surface than the full `clang -c`
/// shape — flags it doesn't understand get dropped here with a note.
/// Append `arg` to `cmd` with a leading space, quoting only when the
/// argument contains whitespace. On Windows, `std::system` wraps the
/// command line with cmd.exe `/c`, which eats the outermost pair of
/// quotes. Quoting every arg unconditionally would therefore produce
/// a malformed command where cmd.exe treats the joined `<exe>" "<arg>`
/// as one token — that's what breaks `tml cc foo.c` for inputs
/// without spaces.
void cmdline_append(std::string& cmd, const std::string& arg) {
    cmd += " ";
    if (arg.find(' ') != std::string::npos) {
        cmd += "\"" + arg + "\"";
    } else {
        cmd += arg;
    }
}

std::string build_cc_driver_cmdline(const fs::path& exe, const CcOptions& opts) {
    std::string cmd = "\"" + exe.string() + "\"";
    cmdline_append(cmd, opts.input);
    switch (opts.emit) {
    case EmitStage::Obj:
        cmdline_append(cmd, "--emit=pipeline");
        break;
    case EmitStage::LlvmIr:
        cmdline_append(cmd, "--emit=mir"); // cc_driver stops at MIR; LLVM is Phase 4
        break;
    case EmitStage::Mir:
        cmdline_append(cmd, "--emit=mir");
        break;
    case EmitStage::Ast:
        cmdline_append(cmd, "--emit=ast");
        break;
    case EmitStage::Tokens:
        cmdline_append(cmd, "--emit=tokens");
        break;
    }
    if (!opts.target_triple.empty()) {
        cmdline_append(cmd, "--target=" + opts.target_triple);
    }
    if (!opts.output.empty()) {
        cmdline_append(cmd, "-o");
        cmdline_append(cmd, opts.output);
    }
    return cmd;
}

} // namespace

// ============================================================================
// Entry point
// ============================================================================

int run_cc(int argc, char* argv[], bool verbose) {
    (void)verbose; // reserved — will gate diagnostic verbosity.

    CcOptions opts;
    if (!parse_options(argc, argv, opts)) {
        std::fprintf(stderr, "\n%s", std::string(kUsage).c_str());
        return 2;
    }
    if (opts.help) {
        std::fprintf(stdout, "%s", std::string(kUsage).c_str());
        return 0;
    }
    if (opts.input.empty()) {
        std::fprintf(stderr, "tml cc: no input file\n\n%s", std::string(kUsage).c_str());
        return 2;
    }

    // Subprocess dispatch — preferred path, mirrors the coverage_cli
    // pattern. If `cc_driver.exe` is present (built via
    // `tml build compiler-tml/src/cc/bin/cc_driver.tml -o build/debug/bin/cc_driver.exe`),
    // forward argv straight to it and return its exit code. Keeps the
    // TML cc pipeline reachable today without any FFI plumbing.
    fs::path driver = find_cc_driver();
    if (!driver.empty()) {
        std::string cmd = build_cc_driver_cmdline(driver, opts);
        return std::system(cmd.c_str());
    }

    // Fallback path — the `cc_bridge` FFI. Currently this returns a
    // stub diagnostic from each pipeline stage; it becomes the real
    // dispatch once Phase 2.2 wires the TML cc modules in-process.
    std::fprintf(stderr,
                 "tml cc: cc_driver.exe not found under build/{debug,release}/bin/ — "
                 "build it with `tml build compiler-tml/src/cc/bin/cc_driver.tml "
                 "-o build/debug/bin/cc_driver.exe`. Falling back to the in-process "
                 "cc_bridge (stubs only).\n");

    std::string source;
    if (!read_file(opts.input, source)) {
        return 1;
    }

    CcDiagnostics* diags = cc_bridge_diagnostics_new();

    // Stage 1 — preprocessing. The `cc_bridge_preproc` stub currently
    // pushes a fatal diagnostic and returns NULL; as soon as the TML
    // cc driver is reachable through the bridge, the same call path
    // lights up end to end.
    CStrArray     includes(opts.include_paths);
    CStrArray     defines(opts.defines);
    CcTokenStream* tokens = cc_bridge_preproc(source.c_str(),
                                              opts.input.c_str(),
                                              includes.data(),
                                              defines.data(),
                                              diags);
    if (tokens == nullptr) {
        render_diagnostics(diags);
        cc_bridge_free_diagnostics(diags);
        return 1;
    }
    if (opts.emit == EmitStage::Tokens) {
        // TODO: render the token stream once `cc_bridge` exposes an
        // enumerator over it. For now, the bridge doesn't provide a
        // token-listing API, so we stop with a user-visible note.
        std::fprintf(stdout, "tml cc --emit=tokens: token stream is opaque "
                             "until cc_bridge exposes an enumerator\n");
        cc_bridge_free_token_stream(tokens);
        cc_bridge_free_diagnostics(diags);
        return 0;
    }

    // Stage 2 — parse. Ownership of `tokens` transfers into the parser
    // on success; on failure, the caller retains it.
    CcTranslationUnit* tu = cc_bridge_parse(tokens, diags);
    if (tu == nullptr) {
        cc_bridge_free_token_stream(tokens);
        render_diagnostics(diags);
        cc_bridge_free_diagnostics(diags);
        return 1;
    }
    if (opts.emit == EmitStage::Ast) {
        std::fprintf(stdout, "tml cc --emit=ast: AST rendering not yet implemented\n");
        cc_bridge_free_translation_unit(tu);
        cc_bridge_free_diagnostics(diags);
        return 0;
    }

    // Stage 3 — lower. Again, ownership transfers on success.
    CcAbiTarget abi  = abi_for_triple(opts.target_triple);
    fs::path    stem = fs::path(opts.input).stem();
    std::string module_name = stem.string();
    CcMirModule* mir = cc_bridge_lower(tu, abi, module_name.c_str(), diags);
    if (mir == nullptr) {
        cc_bridge_free_translation_unit(tu);
        render_diagnostics(diags);
        cc_bridge_free_diagnostics(diags);
        return 1;
    }
    if (opts.emit == EmitStage::Mir) {
        std::fprintf(stdout, "tml cc --emit=mir: MIR rendering not yet implemented\n");
        cc_bridge_free_mir_module(mir);
        cc_bridge_free_diagnostics(diags);
        return 0;
    }

    // Stage 4 — LLVM backend. This path is ready for the MIR once the
    // bridge delivers a real module; today it would get NULL from
    // `cc_bridge_lower` above and not reach here.
    std::string output_path = opts.output.empty()
                                  ? auto_output_path(opts.input, opts.emit)
                                  : opts.output;
    std::fprintf(stderr, "tml cc: backend hand-off to LLVM is not yet wired — "
                         "write to '%s' is the next step once cc_bridge_mir_borrow "
                         "returns a real mir::Module.\n",
                 output_path.c_str());

    cc_bridge_free_mir_module(mir);
    render_diagnostics(diags);
    cc_bridge_free_diagnostics(diags);
    return 1;
}

} // namespace tml::cli
