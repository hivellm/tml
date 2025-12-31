#include "cmd_build.hpp"

#include "build_config.hpp"
#include "codegen/c_header_gen.hpp"
#include "codegen/llvm_ir_gen.hpp"
#include "common.hpp"
#include "compiler_setup.hpp"
#include "diagnostic.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/mir_pass.hpp"
#include "object_compiler.hpp"
#include "parser/parser.hpp"
#include "rlib.hpp"
#include "types/checker.hpp"
#include "utils.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#ifndef _WIN32
#include "types/module.hpp"

#include <sys/wait.h>
#endif

namespace fs = std::filesystem;
using namespace tml;

namespace tml::cli {

// Convert a parser::Type to a string representation
static std::string type_to_string(const parser::Type& type) {
    return std::visit(
        [](const auto& t) -> std::string {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, parser::NamedType>) {
                std::string result;
                for (size_t i = 0; i < t.path.segments.size(); ++i) {
                    if (i > 0)
                        result += "::";
                    result += t.path.segments[i];
                }
                if (t.generics.has_value() && !t.generics->args.empty()) {
                    result += "[";
                    for (size_t i = 0; i < t.generics->args.size(); ++i) {
                        if (i > 0)
                            result += ", ";
                        result += type_to_string(*t.generics->args[i]);
                    }
                    result += "]";
                }
                return result;
            } else if constexpr (std::is_same_v<T, parser::RefType>) {
                return (t.is_mut ? "mut ref " : "ref ") + type_to_string(*t.inner);
            } else if constexpr (std::is_same_v<T, parser::PtrType>) {
                return (t.is_mut ? "*mut " : "*const ") + type_to_string(*t.inner);
            } else if constexpr (std::is_same_v<T, parser::ArrayType>) {
                return "[" + type_to_string(*t.element) + "; _]";
            } else if constexpr (std::is_same_v<T, parser::SliceType>) {
                return "[" + type_to_string(*t.element) + "]";
            } else if constexpr (std::is_same_v<T, parser::TupleType>) {
                std::string result = "(";
                for (size_t i = 0; i < t.elements.size(); ++i) {
                    if (i > 0)
                        result += ", ";
                    result += type_to_string(*t.elements[i]);
                }
                result += ")";
                return result;
            } else if constexpr (std::is_same_v<T, parser::FuncType>) {
                std::string result = "func(";
                for (size_t i = 0; i < t.params.size(); ++i) {
                    if (i > 0)
                        result += ", ";
                    result += type_to_string(*t.params[i]);
                }
                result += ")";
                if (t.return_type) {
                    result += " -> " + type_to_string(*t.return_type);
                }
                return result;
            } else if constexpr (std::is_same_v<T, parser::InferType>) {
                return "_";
            } else if constexpr (std::is_same_v<T, parser::DynType>) {
                std::string result = "dyn ";
                if (t.is_mut)
                    result += "mut ";
                for (size_t i = 0; i < t.behavior.segments.size(); ++i) {
                    if (i > 0)
                        result += "::";
                    result += t.behavior.segments[i];
                }
                return result;
            } else {
                return "unknown";
            }
        },
        type.kind);
}

// Generate a unique cache key for a file path (to avoid collisions in parallel tests)
static std::string generate_cache_key(const std::string& path) {
    // Use hash of full path + thread ID to ensure uniqueness
    std::hash<std::string> hasher;
    size_t path_hash = hasher(path);
    size_t thread_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    size_t combined = path_hash ^ (thread_hash << 1);

    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << (combined & 0xFFFFFFFF);
    return oss.str();
}

// ============================================================================
// Diagnostic Helpers
// ============================================================================

// Emit a lexer error using the diagnostic emitter
static void emit_lexer_error(DiagnosticEmitter& emitter, const lexer::LexerError& error) {
    emitter.error("L001", error.message, error.span);
}

// Emit a parser error using the diagnostic emitter
static void emit_parser_error(DiagnosticEmitter& emitter, const parser::ParseError& error) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.code = "P001";
    diag.message = error.message;
    diag.primary_span = error.span;
    diag.notes = error.notes;

    // Convert parser FixItHints to DiagnosticFixIts
    for (const auto& fix : error.fixes) {
        diag.fixes.push_back(DiagnosticFixIt{
            .span = fix.span, .replacement = fix.replacement, .description = fix.description});
    }

    emitter.emit(diag);
}

// Emit a type error using the diagnostic emitter
static void emit_type_error(DiagnosticEmitter& emitter, const types::TypeError& error) {
    emitter.error("T001", error.message, error.span, error.notes);
}

// Emit all lexer errors
static void emit_all_lexer_errors(DiagnosticEmitter& emitter, const lexer::Lexer& lex) {
    for (const auto& error : lex.errors()) {
        emit_lexer_error(emitter, error);
    }
}

// Emit all parser errors
static void emit_all_parser_errors(DiagnosticEmitter& emitter,
                                   const std::vector<parser::ParseError>& errors) {
    for (const auto& error : errors) {
        emit_parser_error(emitter, error);
    }
}

// Emit all type errors
static void emit_all_type_errors(DiagnosticEmitter& emitter,
                                 const std::vector<types::TypeError>& errors) {
    for (const auto& error : errors) {
        emit_type_error(emitter, error);
    }
}

// Emit a codegen error using the diagnostic emitter
static void emit_codegen_error(DiagnosticEmitter& emitter, const codegen::LLVMGenError& error) {
    emitter.error("C001", error.message, error.span, error.notes);
}

// Emit all codegen errors
static void emit_all_codegen_errors(DiagnosticEmitter& emitter,
                                    const std::vector<codegen::LLVMGenError>& errors) {
    for (const auto& error : errors) {
        emit_codegen_error(emitter, error);
    }
}

// ============================================================================
// Hashing Utilities
// ============================================================================

// Generate a content hash for caching compiled object files
static std::string generate_content_hash(const std::string& content) {
    std::hash<std::string> hasher;
    size_t hash = hasher(content);

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

// Generate a combined hash for executable caching (source + all object files)
static std::string generate_exe_hash(const std::string& source_hash,
                                     const std::vector<fs::path>& obj_files) {
    std::hash<std::string> hasher;
    size_t combined_hash = hasher(source_hash);

    // Combine hashes of all object file paths and timestamps
    for (const auto& obj : obj_files) {
        if (fs::exists(obj)) {
            // Include file path and last write time
            combined_hash ^=
                hasher(obj.string()) + 0x9e3779b9 + (combined_hash << 6) + (combined_hash >> 2);
            auto ftime = fs::last_write_time(obj).time_since_epoch().count();
            combined_hash ^= std::hash<decltype(ftime)>{}(ftime) + 0x9e3779b9 +
                             (combined_hash << 6) + (combined_hash >> 2);
        }
    }

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << combined_hash;
    return oss.str();
}

// Fast file copy using hard links when possible, falls back to regular copy
// Hard links are much faster because they don't copy data, just create a new directory entry
static bool fast_copy_file(const fs::path& from, const fs::path& to) {
    try {
        // Remove destination if it exists
        if (fs::exists(to)) {
            fs::remove(to);
        }

        // Try hard link first (instant, no data copy)
        try {
            fs::create_hard_link(from, to);
            return true;
        } catch (...) {
            // Hard link failed (maybe cross-device), fall back to copy
            fs::copy_file(from, to, fs::copy_options::overwrite_existing);
            return true;
        }
    } catch (...) {
        return false;
    }
}

// Find the project root by looking for markers like .git, CLAUDE.md, etc.
static fs::path find_project_root() {
    fs::path current = fs::current_path();

    // Walk up the directory tree looking for project markers
    while (!current.empty() && current != current.parent_path()) {
        // Check for common project markers
        if (fs::exists(current / ".git") || fs::exists(current / "CLAUDE.md") ||
            fs::exists(current / "packages")) {
            return current;
        }
        current = current.parent_path();
    }

    // Fallback to current working directory
    return fs::current_path();
}

// Find or create build directory for a TML project
// Returns: project_root/build/debug or project_root/build/release
static fs::path get_build_dir(bool release = false) {
    // Always use the project root, never create build dirs next to source files
    fs::path project_root = find_project_root();

    // Create build directory structure in project root
    fs::path build_dir = project_root / "build" / (release ? "release" : "debug");
    fs::create_directories(build_dir);

    return build_dir;
}

// Helper to check if any function has @bench decorator
static bool has_bench_functions(const parser::Module& module) {
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            for (const auto& decorator : func.decorators) {
                if (decorator.name == "bench") {
                    return true;
                }
            }
        }
    }
    return false;
}

// Helper to get runtime object files as a vector
// Returns a vector of compiled runtime object file paths
static std::vector<fs::path>
get_runtime_objects(const std::shared_ptr<types::ModuleRegistry>& registry,
                    const parser::Module& module, const std::string& deps_cache,
                    const std::string& clang, bool verbose) {
    std::vector<fs::path> objects;

    // Helper to find and compile runtime with caching
    auto add_runtime = [&](const std::vector<std::string>& search_paths, const std::string& name) {
        for (const auto& path : search_paths) {
            if (fs::exists(path)) {
                std::string obj = ensure_c_compiled(to_forward_slashes(fs::absolute(path).string()),
                                                    deps_cache, clang, verbose);
                objects.push_back(fs::path(obj));
                if (verbose) {
                    std::cout << "Including " << name << ": " << obj << "\n";
                }
                return;
            }
        }
    };

    // Essential runtime (IO functions)
    std::string runtime_path = find_runtime();
    if (!runtime_path.empty()) {
        std::string obj = ensure_c_compiled(runtime_path, deps_cache, clang, verbose);
        objects.push_back(fs::path(obj));
        if (verbose) {
            std::cout << "Including runtime: " << obj << "\n";
        }

        // Also include string.c and mem.c by default (commonly used)
        fs::path runtime_dir = fs::path(runtime_path).parent_path();

        fs::path string_c = runtime_dir / "string.c";
        if (fs::exists(string_c)) {
            std::string string_obj = ensure_c_compiled(to_forward_slashes(string_c.string()),
                                                       deps_cache, clang, verbose);
            objects.push_back(fs::path(string_obj));
            if (verbose) {
                std::cout << "Including string runtime: " << string_obj << "\n";
            }
        }

        fs::path mem_c = runtime_dir / "mem.c";
        if (fs::exists(mem_c)) {
            std::string mem_obj =
                ensure_c_compiled(to_forward_slashes(mem_c.string()), deps_cache, clang, verbose);
            objects.push_back(fs::path(mem_obj));
            if (verbose) {
                std::cout << "Including mem runtime: " << mem_obj << "\n";
            }
        }

        // Also include time.c by default (commonly used for timing/sleep)
        fs::path time_c = runtime_dir / "time.c";
        if (fs::exists(time_c)) {
            std::string time_obj =
                ensure_c_compiled(to_forward_slashes(time_c.string()), deps_cache, clang, verbose);
            objects.push_back(fs::path(time_obj));
            if (verbose) {
                std::cout << "Including time runtime: " << time_obj << "\n";
            }
        }
    }

    // Link core module runtimes if they were imported
    if (registry->has_module("core::mem")) {
        add_runtime(
            {
                "lib/core/runtime/mem.c",
                "../../../lib/core/runtime/mem.c",
                "F:/Node/hivellm/tml/lib/core/runtime/mem.c",
            },
            "core::mem");
    }

    // Link time runtime if core::time is imported OR if @bench decorators are present
    if (registry->has_module("core::time") || has_bench_functions(module)) {
        add_runtime(
            {
                "lib/core/runtime/time.c",
                "../../../lib/core/runtime/time.c",
                "F:/Node/hivellm/tml/lib/core/runtime/time.c",
            },
            "core::time");
    }

    if (registry->has_module("core::thread") || registry->has_module("core::sync")) {
        add_runtime(
            {
                "lib/core/runtime/thread.c",
                "../../../lib/core/runtime/thread.c",
                "F:/Node/hivellm/tml/lib/core/runtime/thread.c",
            },
            "core::thread");
    }

    if (registry->has_module("test")) {
        add_runtime(
            {
                "lib/test/runtime/test.c",
                "../../../lib/test/runtime/test.c",
                "F:/Node/hivellm/tml/lib/test/runtime/test.c",
            },
            "test");

        // Also link coverage runtime (part of test module)
        add_runtime(
            {
                "lib/test/runtime/coverage.c",
                "../../../lib/test/runtime/coverage.c",
                "F:/Node/hivellm/tml/lib/test/runtime/coverage.c",
            },
            "test::coverage");
    }

    // Link std::collections runtime if imported
    if (registry->has_module("std::collections")) {
        add_runtime(
            {
                "lib/std/runtime/collections.c",
                "../../../lib/std/runtime/collections.c",
                "F:/Node/hivellm/tml/lib/std/runtime/collections.c",
            },
            "std::collections");
    }

    // Link std::file runtime if imported
    if (registry->has_module("std::file")) {
        add_runtime(
            {
                "lib/std/runtime/file.c",
                "../../../lib/std/runtime/file.c",
                "F:/Node/hivellm/tml/lib/std/runtime/file.c",
            },
            "std::file");
    }

    return objects;
}

// Get the global deps cache directory
static fs::path get_deps_cache_dir() {
    // Always use project root for deps cache
    fs::path project_root = find_project_root();
    fs::path deps = project_root / "build" / "debug" / "deps";
    fs::create_directories(deps);
    return deps;
}

// Get the global run cache directory (for tml run temporary files)
static fs::path get_run_cache_dir() {
    // Always use project root for run cache
    fs::path project_root = find_project_root();
    fs::path cache = project_root / "build" / "debug" / ".run-cache";
    fs::create_directories(cache);
    return cache;
}

int run_build(const std::string& path, bool verbose, bool emit_ir_only, bool emit_mir,
              bool no_cache, BuildOutputType output_type, bool emit_header,
              const std::string& output_dir) {
    // no_cache is now used to skip object file caching during compilation

    // Try to load tml.toml manifest
    auto manifest_opt = Manifest::load_from_current_dir();
    if (manifest_opt && verbose) {
        std::cout << "Found tml.toml manifest for project: " << manifest_opt->package.name << "\n";
    }

    // Apply manifest settings if available (command-line flags override)
    // Note: Command-line parameters are already set, so we only apply defaults from manifest
    if (manifest_opt && !manifest_opt->build.validate()) {
        std::cerr << "Warning: Invalid build settings in tml.toml, using defaults\n";
    }

    // Initialize diagnostic emitter for Rust-style error output
    auto& diag = get_diagnostic_emitter();

    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    // Register source content with diagnostic emitter for source snippets
    diag.set_source_content(path, source_code);

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        emit_all_lexer_errors(diag, lex);
        return 1;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        emit_all_parser_errors(diag, errors);
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Initialize module registry
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    // Set source directory for local module resolution (use <module_name> to import sibling .tml
    // files)
    auto source_dir = fs::path(path).parent_path();
    if (source_dir.empty()) {
        source_dir = fs::current_path();
    }
    checker.set_source_directory(source_dir.string());
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        emit_all_type_errors(diag, errors);
        return 1;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    // Emit MIR if requested (early exit before LLVM codegen)
    if (emit_mir) {
        mir::MirBuilder mir_builder(env);
        auto mir_module = mir_builder.build(module);

        // Apply MIR optimizations based on optimization level
        int opt_level = tml::CompilerOptions::optimization_level;
        if (opt_level > 0) {
            mir::OptLevel mir_opt = mir::OptLevel::O0;
            if (opt_level == 1)
                mir_opt = mir::OptLevel::O1;
            else if (opt_level == 2)
                mir_opt = mir::OptLevel::O2;
            else if (opt_level >= 3)
                mir_opt = mir::OptLevel::O3;

            mir::PassManager pm(mir_opt);
            pm.configure_standard_pipeline();
            int passes_changed = pm.run(mir_module);
            if (verbose && passes_changed > 0) {
                std::cout << "  MIR optimization: " << passes_changed << " passes applied\n";
            }
        }

        // Use build directory structure
        fs::path build_dir =
            output_dir.empty() ? get_build_dir(false /* debug */) : fs::path(output_dir);
        fs::create_directories(build_dir);

        fs::path mir_output = build_dir / (module_name + ".mir");
        std::ofstream mir_file(mir_output);
        if (!mir_file) {
            std::cerr << "error: Cannot write to " << mir_output << "\n";
            return 1;
        }
        mir_file << mir::print_module(mir_module);
        mir_file.close();

        std::cout << "emit-mir: " << to_forward_slashes(mir_output.string()) << "\n";
        return 0;
    }

    codegen::LLVMGenOptions options;
    options.emit_comments = verbose;
    options.emit_debug_info = CompilerOptions::debug_info;
    options.debug_level = CompilerOptions::debug_level;
    options.source_file = path;
    if (!CompilerOptions::target_triple.empty()) {
        options.target_triple = CompilerOptions::target_triple;
    }
#ifdef _WIN32
    // Enable DLL export for dynamic libraries on Windows
    options.dll_export = (output_type == BuildOutputType::DynamicLib);
#endif
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
        emit_all_codegen_errors(diag, errors);
        return 1;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    // Use build directory structure (like Rust's target/)
    // If output_dir is specified, use it; otherwise use default build directory
    fs::path build_dir =
        output_dir.empty() ? get_build_dir(false /* debug */) : fs::path(output_dir);
    fs::create_directories(build_dir); // Ensure custom output directory exists

    fs::path ll_output = build_dir / (module_name + ".ll");
    fs::path exe_output = build_dir / module_name;
#ifdef _WIN32
    exe_output += ".exe";
#endif

    std::ofstream ll_file(ll_output);
    if (!ll_file) {
        std::cerr << "error: Cannot write to " << ll_output << "\n";
        return 1;
    }
    ll_file << llvm_ir;
    ll_file.close();

    if (verbose) {
        std::cout << "Generated: " << ll_output << "\n";
    }

    if (emit_ir_only) {
        std::cout << "emit-ir: " << ll_output << "\n";
        return 0;
    }

    std::string clang = find_clang();

    // Create deps cache directory for precompiled runtimes
    fs::path deps_dir = build_dir / "deps";
    fs::create_directories(deps_dir);
    std::string deps_cache = to_forward_slashes(deps_dir.string());

    // Create .cache directory for object files
    fs::path cache_dir = build_dir / ".cache";
    fs::create_directories(cache_dir);

    // Step 1: Compile LLVM IR (.ll) to object file (.o/.obj)
    ObjectCompileOptions obj_options;
    obj_options.optimization_level = tml::CompilerOptions::optimization_level;
    obj_options.debug_info = tml::CompilerOptions::debug_info;
    obj_options.verbose = verbose;
    obj_options.target_triple = tml::CompilerOptions::target_triple;
    obj_options.sysroot = tml::CompilerOptions::sysroot;

    fs::path obj_output = cache_dir / (module_name + get_object_extension());

    // Check if cached object file is valid (unless --no-cache is set)
    bool use_cached_obj = false;
    if (!no_cache && fs::exists(obj_output)) {
        try {
            auto src_time = fs::last_write_time(fs::path(path));
            auto obj_time = fs::last_write_time(obj_output);
            if (obj_time >= src_time) {
                use_cached_obj = true;
                if (verbose) {
                    std::cout << "Using cached object file: " << obj_output << "\n";
                }
            }
        } catch (...) {
            // If we can't check timestamps, recompile
        }
    }

    ObjectCompileResult obj_result;
    if (use_cached_obj) {
        obj_result.success = true;
        obj_result.object_file = obj_output;
    } else {
        obj_result = compile_ll_to_object(ll_output, obj_output, clang, obj_options);
        if (!obj_result.success) {
            std::cerr << "error: " << obj_result.error_message << "\n";
            return 1;
        }

        if (verbose) {
            std::cout << "Generated: " << obj_result.object_file << "\n";
        }
    }

    // Step 2: Collect all object files to link (main .o + runtime .o files)
    std::vector<fs::path> object_files;
    object_files.push_back(obj_result.object_file);

    // Add runtime object files only for executables (not for libraries)
    if (output_type == BuildOutputType::Executable) {
        auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, verbose);
        object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());
    }

    // Step 3: Determine output file extension based on output type
    fs::path final_output;
    LinkOptions::OutputType link_output_type = LinkOptions::OutputType::Executable;

    switch (output_type) {
    case BuildOutputType::Executable:
        final_output = exe_output; // Already has .exe extension
        link_output_type = LinkOptions::OutputType::Executable;
        break;
    case BuildOutputType::StaticLib:
#ifdef _WIN32
        final_output = build_dir / (module_name + ".lib");
#else
        final_output = build_dir / ("lib" + module_name + ".a");
#endif
        link_output_type = LinkOptions::OutputType::StaticLib;
        break;
    case BuildOutputType::DynamicLib:
#ifdef _WIN32
        final_output = build_dir / (module_name + ".dll");
#else
#ifdef __APPLE__
        final_output = build_dir / ("lib" + module_name + ".dylib");
#else
        final_output = build_dir / ("lib" + module_name + ".so");
#endif
#endif
        link_output_type = LinkOptions::OutputType::DynamicLib;
        break;
    case BuildOutputType::RlibLib:
        final_output = build_dir / (module_name + ".rlib");
        // RLIB doesn't use standard linking - we'll create it separately
        break;
    }

    // For RLIB: Create RLIB archive instead of linking
    if (output_type == BuildOutputType::RlibLib) {
        // Create metadata
        RlibMetadata metadata;
        metadata.format_version = "1.0";
        metadata.library.name = module_name;

        // Try to read version from manifest (tml.toml)
        std::string version = "0.1.0";
        fs::path manifest_path = fs::path(path).parent_path() / "tml.toml";
        if (fs::exists(manifest_path)) {
            if (auto manifest = Manifest::load(manifest_path)) {
                version = manifest->package.version;
                if (!manifest->package.name.empty()) {
                    metadata.library.name = manifest->package.name;
                }
            }
        }
        metadata.library.version = version;
        metadata.library.tml_version = "0.1.0";

        // Add module information
        RlibModule rlib_module;
        rlib_module.name = module_name;
        rlib_module.file = object_files[0].filename().string();
        rlib_module.hash = calculate_file_hash(fs::path(path));

        // Extract exports from the module with full type information
        for (const auto& decl : module.decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func_decl = decl->as<parser::FuncDecl>();
                if (func_decl.vis == parser::Visibility::Public) {
                    RlibExport exp;
                    exp.name = func_decl.name;
                    exp.symbol = "tml_" + func_decl.name; // Simple mangling

                    // Build type signature: func(T1, T2, ...) -> RetType
                    std::string type_sig = "func(";
                    for (size_t i = 0; i < func_decl.params.size(); ++i) {
                        if (i > 0)
                            type_sig += ", ";
                        // Extract type name from parameter
                        if (func_decl.params[i].type) {
                            type_sig += type_to_string(*func_decl.params[i].type);
                        } else {
                            type_sig += "_";
                        }
                    }
                    type_sig += ")";
                    if (func_decl.return_type.has_value()) {
                        type_sig += " -> " + type_to_string(**func_decl.return_type);
                    }
                    exp.type = type_sig;
                    exp.is_public = true;
                    rlib_module.exports.push_back(exp);
                }
            } else if (decl->is<parser::StructDecl>()) {
                const auto& struct_decl = decl->as<parser::StructDecl>();
                if (struct_decl.vis == parser::Visibility::Public) {
                    RlibExport exp;
                    exp.name = struct_decl.name;
                    exp.symbol = struct_decl.name;
                    exp.type = "struct";
                    exp.is_public = true;
                    rlib_module.exports.push_back(exp);
                }
            } else if (decl->is<parser::EnumDecl>()) {
                const auto& enum_decl = decl->as<parser::EnumDecl>();
                if (enum_decl.vis == parser::Visibility::Public) {
                    RlibExport exp;
                    exp.name = enum_decl.name;
                    exp.symbol = enum_decl.name;
                    exp.type = "enum";
                    exp.is_public = true;
                    rlib_module.exports.push_back(exp);
                }
            }
        }

        metadata.modules.push_back(rlib_module);

        // Create RLIB
        RlibCreateOptions rlib_opts;
        rlib_opts.verbose = verbose;

        auto rlib_result = create_rlib(object_files, metadata, final_output, rlib_opts);
        if (!rlib_result.success) {
            std::cerr << "error: " << rlib_result.message << "\n";
            return rlib_result.exit_code;
        }
    } else {
        // Standard linking for executables and libraries
        LinkOptions link_options;
        link_options.output_type = link_output_type;
        link_options.verbose = verbose;
        link_options.target_triple = tml::CompilerOptions::target_triple;
        link_options.sysroot = tml::CompilerOptions::sysroot;

        // Add @link libraries from FFI decorators
        for (const auto& lib : llvm_gen.get_link_libs()) {
            // Check if it's a path (contains / or \) or a library name
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                // Full path to library file
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                // Library name - use -l flag
                link_options.link_flags.push_back("-l" + lib);
            }
        }

        auto link_result = link_objects(object_files, final_output, clang, link_options);
        if (!link_result.success) {
            std::cerr << "error: " << link_result.error_message << "\n";
            return 1;
        }
    }

    // Clean up .ll file (keep .o file in cache for potential reuse)
    fs::remove(ll_output);

    std::cout << "build: " << to_forward_slashes(final_output.string()) << "\n";

    // Generate C header if requested (after successful build)
    if (emit_header) {
        codegen::CHeaderGenOptions header_opts;
        codegen::CHeaderGen header_gen(env, header_opts);
        auto header_result = header_gen.generate(module);

        if (!header_result.success) {
            std::cerr << "error: Header generation failed: " << header_result.error_message << "\n";
            return 1;
        }

        // Write header file to same directory as the library/executable
        fs::path header_output = build_dir / (module_name + ".h");

        std::ofstream header_file(header_output);
        if (!header_file) {
            std::cerr << "error: Cannot write to " << header_output << "\n";
            return 1;
        }
        header_file << header_result.header_content;
        header_file.close();

        std::cout << "emit-header: " << to_forward_slashes(header_output.string()) << "\n";
    }

    return 0;
}

int run_run(const std::string& path, const std::vector<std::string>& args, bool verbose,
            bool coverage, bool no_cache) {
    // Initialize diagnostic emitter for Rust-style error output
    auto& diag = get_diagnostic_emitter();

    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    // Register source content with diagnostic emitter for source snippets
    diag.set_source_content(path, source_code);

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        emit_all_lexer_errors(diag, lex);
        return 1;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        emit_all_parser_errors(diag, errors);
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Initialize module registry
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    // Set source directory for local module resolution
    {
        auto source_dir = fs::path(path).parent_path();
        if (source_dir.empty()) {
            source_dir = fs::current_path();
        }
        checker.set_source_directory(source_dir.string());
    }
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        emit_all_type_errors(diag, errors);
        return 1;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    options.coverage_enabled = coverage;
    options.emit_debug_info = CompilerOptions::debug_info;
    options.debug_level = CompilerOptions::debug_level;
    options.source_file = path;
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
        emit_all_codegen_errors(diag, errors);
        return 1;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    // Use centralized run cache - NEVER create files inside packages
    fs::path cache_dir = get_run_cache_dir();

    // Calculate content hash for caching
    std::string content_hash = generate_content_hash(source_code);

    fs::path ll_output = cache_dir / (content_hash + ".ll");
    fs::path obj_output = cache_dir / (content_hash + get_object_extension());
    fs::path exe_output = cache_dir / module_name;
#ifdef _WIN32
    exe_output += ".exe";
#endif

    std::string clang = find_clang();
    if (clang.empty() || (!fs::exists(clang) && clang != "clang")) {
        std::cerr << "error: clang not found.\n";
        std::cerr << "Please install LLVM/clang\n";
        return 1;
    }

    // Use global deps cache for precompiled runtimes
    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

    // Check if we have a cached object file
    bool use_cached_obj = fs::exists(obj_output);

    if (use_cached_obj) {
        if (verbose) {
            std::cout << "Using cached object: " << obj_output << "\n";
        }
    } else {
        // Write LLVM IR to file
        std::ofstream ll_file(ll_output);
        if (!ll_file) {
            std::cerr << "error: Cannot write to " << ll_output << "\n";
            return 1;
        }
        ll_file << llvm_ir;
        ll_file.close();

        if (verbose) {
            std::cout << "Generated: " << ll_output << "\n";
        }

        // Compile LLVM IR to object file
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = tml::CompilerOptions::debug_info;
        obj_options.verbose = verbose;
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;

        auto obj_result = compile_ll_to_object(ll_output, obj_output, clang, obj_options);
        if (!obj_result.success) {
            std::cerr << "error: " << obj_result.error_message << "\n";
            fs::remove(ll_output);
            return 1;
        }

        if (verbose) {
            std::cout << "Compiled to: " << obj_result.object_file << "\n";
        }

        // Clean up .ll file (keep .obj for caching)
        fs::remove(ll_output);
    }

    // Collect all object files to link
    std::vector<fs::path> object_files;
    object_files.push_back(obj_output);

    // Add runtime object files
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, verbose);
    object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

    // Generate hash for executable caching (source + all object files)
    std::string exe_hash = generate_exe_hash(content_hash, object_files);
    fs::path cached_exe = cache_dir / (exe_hash + ".exe");

    // Check if we have a cached executable (skip if --no-cache)
    bool use_cached_exe = !no_cache && fs::exists(cached_exe);

    if (use_cached_exe) {
        if (verbose) {
            std::cout << "Using cached executable: " << cached_exe << "\n";
        }
    } else {
        // Link all object files to create executable
        LinkOptions link_options;
        link_options.output_type = LinkOptions::OutputType::Executable;
        link_options.verbose = verbose;
        link_options.target_triple = tml::CompilerOptions::target_triple;
        link_options.sysroot = tml::CompilerOptions::sysroot;

        // Add @link libraries from FFI decorators
        for (const auto& lib : llvm_gen.get_link_libs()) {
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                link_options.link_flags.push_back("-l" + lib);
            }
        }

        // Link to temporary location first
        fs::path temp_exe = cache_dir / (exe_hash + "_link_temp.exe");

        auto link_result = link_objects(object_files, temp_exe, clang, link_options);
        if (!link_result.success) {
            std::cerr << "error: " << link_result.error_message << "\n";
            return 1;
        }

        if (verbose) {
            std::cout << "Linked executable: " << temp_exe << "\n";
        }

        // Try to move to cached location (atomic on same filesystem)
        try {
            if (!fs::exists(cached_exe)) {
                fs::rename(temp_exe, cached_exe);
            } else {
                // Another process created it first
                fs::remove(temp_exe);
            }
        } catch (...) {
            // Clean up temp file on error
            if (fs::exists(temp_exe)) {
                fs::remove(temp_exe);
            }
        }
    }

    // Copy cached exe to final location (use hard link for speed)
    if (!fast_copy_file(cached_exe, exe_output)) {
        std::cerr << "error: Failed to copy cached exe to " << exe_output << "\n";
        return 1;
    }

    std::string exe_path = to_forward_slashes(exe_output.string());
    std::string run_cmd;
#ifdef _WIN32
    // Windows: use cmd /c to properly handle relative paths with forward slashes
    run_cmd = "cmd /c \"\"" + exe_path + "\"";
    for (const auto& arg : args) {
        run_cmd += " \"" + arg + "\"";
    }
    run_cmd += "\"";
#else
    run_cmd = "\"" + exe_path + "\"";
    for (const auto& arg : args) {
        run_cmd += " \"" + arg + "\"";
    }
#endif

    if (verbose) {
        std::cout << "Running: " << run_cmd << "\n";
    }

    int run_ret = std::system(run_cmd.c_str());

    // Clean up temporary executable (keep .obj in cache for reuse)
    fs::remove(exe_output);

    if (verbose) {
        std::cout << "Cleaned up temporary executable\n";
    }

#ifdef _WIN32
    return run_ret;
#else
    return WEXITSTATUS(run_ret);
#endif
}

int run_run_quiet(const std::string& path, const std::vector<std::string>& args, bool verbose,
                  std::string* output, bool coverage, bool no_cache) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        if (output)
            *output = std::string("compilation error: ") + e.what();
        return EXIT_COMPILATION_ERROR;
    }

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        std::string err_output = "compilation error:\n";
        for (const auto& error : lex.errors()) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) + ": error: " + error.message +
                          "\n";
        }
        if (output)
            *output = err_output;
        return EXIT_COMPILATION_ERROR;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        std::string err_output = "compilation error:\n";
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) + ": error: " + error.message +
                          "\n";
        }
        if (output)
            *output = err_output;
        return EXIT_COMPILATION_ERROR;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        std::string err_output = "compilation error:\n";
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        for (const auto& error : errors) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) + ": error: " + error.message +
                          "\n";
        }
        if (output)
            *output = err_output;
        return EXIT_COMPILATION_ERROR;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    options.coverage_enabled = coverage;
    options.emit_debug_info = CompilerOptions::debug_info;
    options.debug_level = CompilerOptions::debug_level;
    options.source_file = path;
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        std::string err_output = "compilation error:\n";
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
        for (const auto& error : errors) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) +
                          ": codegen error: " + error.message + "\n";
        }
        if (output)
            *output = err_output;
        return EXIT_COMPILATION_ERROR;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    // Use centralized run cache - NEVER create files inside packages
    fs::path cache_dir = get_run_cache_dir();

    // Calculate content hash for caching (unique per source content)
    std::string content_hash = generate_content_hash(source_code);

    // Generate unique file names using cache key + thread ID for exe/output (to avoid race
    // conditions)
    std::string cache_key = generate_cache_key(path);
    std::string unique_name = module_name + "_" + cache_key;

    fs::path ll_output = cache_dir / (content_hash + ".ll");
    fs::path obj_output = cache_dir / (content_hash + get_object_extension());
    fs::path exe_output = cache_dir / unique_name;
    fs::path out_file = cache_dir / (unique_name + "_output.txt");
#ifdef _WIN32
    exe_output += ".exe";
#endif

    std::string clang = find_clang();
    if (clang.empty() || (!fs::exists(clang) && clang != "clang")) {
        if (output)
            *output = "compilation error: clang not found";
        return EXIT_COMPILATION_ERROR;
    }

    // Use global deps cache for precompiled runtimes
    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

    // Check if we have a cached object file
    bool use_cached_obj = fs::exists(obj_output);

    if (!use_cached_obj) {
        // Write LLVM IR to file
        std::ofstream ll_file(ll_output);
        if (!ll_file) {
            if (output)
                *output = "compilation error: Cannot write to " + ll_output.string();
            return EXIT_COMPILATION_ERROR;
        }
        ll_file << llvm_ir;
        ll_file.close();

        // Compile LLVM IR to object file
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = tml::CompilerOptions::debug_info;
        obj_options.verbose = false; // Always quiet for tests
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;

        auto obj_result = compile_ll_to_object(ll_output, obj_output, clang, obj_options);
        if (!obj_result.success) {
            if (output)
                *output = "compilation error: " + obj_result.error_message;
            fs::remove(ll_output);
            return EXIT_COMPILATION_ERROR;
        }

        // Clean up .ll file (keep .obj for caching)
        fs::remove(ll_output);
    }

    // Collect all object files to link
    std::vector<fs::path> object_files;
    object_files.push_back(obj_output);

    // Add runtime object files
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, verbose);
    object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

    // Generate hash for executable caching (source + all object files)
    std::string exe_hash = generate_exe_hash(content_hash, object_files);
    fs::path cached_exe = cache_dir / (exe_hash + ".exe");

    // Check if we have a cached executable (skip if --no-cache)
    bool use_cached_exe = !no_cache && fs::exists(cached_exe);

    if (!use_cached_exe) {
        // Link all object files to create executable
        LinkOptions link_options;
        link_options.output_type = LinkOptions::OutputType::Executable;
        link_options.verbose = false; // Always quiet for tests
        link_options.target_triple = tml::CompilerOptions::target_triple;
        link_options.sysroot = tml::CompilerOptions::sysroot;

        // Add @link libraries from FFI decorators
        for (const auto& lib : llvm_gen.get_link_libs()) {
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                link_options.link_flags.push_back("-l" + lib);
            }
        }

        // Link to a unique temporary location (avoid race conditions)
        std::string temp_key = generate_cache_key(path);
        fs::path temp_exe = cache_dir / (exe_hash + "_" + temp_key + "_temp.exe");

        auto link_result = link_objects(object_files, temp_exe, clang, link_options);
        if (!link_result.success) {
            if (output)
                *output = "compilation error: " + link_result.error_message;
            return EXIT_COMPILATION_ERROR;
        }

        // Try to move to cached location (may fail if another thread already did it)
        try {
            if (!fs::exists(cached_exe)) {
                fs::rename(temp_exe, cached_exe);
            } else {
                // Another thread created it first, just remove our temp
                fs::remove(temp_exe);
            }
        } catch (...) {
            // Race condition - another thread created it, clean up our temp
            if (fs::exists(temp_exe)) {
                fs::remove(temp_exe);
            }
        }
    }

    // Copy cached exe to final unique location (use hard link for speed in parallel tests)
    if (!fast_copy_file(cached_exe, exe_output)) {
        if (output)
            *output = "error: Failed to copy cached exe";
        return 1;
    }

    // Run with output redirected to a file
    // Use native path format for Windows compatibility
    std::string exe_native = exe_output.string();
    std::string out_native = out_file.string();
    std::string run_cmd;

#ifdef _WIN32
    // Windows: use cmd /c to properly handle redirection
    // Format: cmd /c "\"exe_path\" args > \"output_path\" 2>&1"
    run_cmd = "cmd /c \"\"" + exe_native + "\"";
    for (const auto& arg : args) {
        run_cmd += " \"" + arg + "\"";
    }
    run_cmd += " > \"" + out_native + "\" 2>&1\"";
#else
    run_cmd = "\"" + exe_native + "\"";
    for (const auto& arg : args) {
        run_cmd += " \"" + arg + "\"";
    }
    run_cmd += " > \"" + out_native + "\" 2>&1";
#endif

    int run_ret = std::system(run_cmd.c_str());

    // Read captured output
    if (output && fs::exists(out_file)) {
        std::ifstream ifs(out_file);
        *output =
            std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    }

    // Clean up temporary files (keep .obj in cache for reuse)
    fs::remove(out_file);
    fs::remove(exe_output);

#ifdef _WIN32
    return run_ret;
#else
    return WEXITSTATUS(run_ret);
#endif
}

// ============================================================================
// Extended Build with Timing Support
// ============================================================================

int run_build_ex(const std::string& path, const BuildOptions& options) {
    // For now, delegate to the standard build function
    // Full timing integration requires refactoring the build pipeline
    return run_build(path, options.verbose, options.emit_ir_only, options.emit_mir,
                     options.no_cache, options.output_type, options.emit_header,
                     options.output_dir);
}

} // namespace tml::cli
