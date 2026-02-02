//! # Builder Helper Functions
//!
//! This file contains shared utility functions used across the build system.
//! Includes type serialization, cache key generation, file operations, and
//! diagnostic helpers.
//!
//! ## Contents
//!
//! - **Type Utilities**: `type_to_string()` for RLIB type signatures
//! - **Cache Key Generation**: Content hashing for incremental builds
//! - **File Utilities**: Project root detection, directory management
//! - **Diagnostic Helpers**: Error emission for all compiler phases
//! - **Module Helpers**: Runtime object collection for linking

#include "builder_internal.hpp"

namespace tml::cli::build {

// ============================================================================
// Type Utilities
// ============================================================================

/// Converts a parser Type to its string representation.
///
/// Used for generating type signatures in RLIB metadata.
/// Handles all TML type variants: named, ref, ptr, array, tuple, func, etc.
std::string type_to_string(const parser::Type& type) {
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
                    bool first = true;
                    for (const auto& arg : t.generics->args) {
                        if (!first)
                            result += ", ";
                        first = false;
                        if (arg.is_type()) {
                            result += type_to_string(*arg.as_type());
                        } else {
                            result += "<const>"; // Placeholder for const generic
                        }
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

// ============================================================================
// Cache Key Generation
// ============================================================================

std::string generate_cache_key(const std::string& path) {
    // Use hash of full path + thread ID to ensure uniqueness
    std::hash<std::string> hasher;
    size_t path_hash = hasher(path);
    size_t thread_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    size_t combined = path_hash ^ (thread_hash << 1);

    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << (combined & 0xFFFFFFFF);
    return oss.str();
}

std::string generate_content_hash(const std::string& content) {
    std::hash<std::string> hasher;
    size_t hash = hasher(content);

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

std::string generate_exe_hash(const std::string& source_hash,
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

// ============================================================================
// File Utilities
// ============================================================================

bool fast_copy_file(const fs::path& from, const fs::path& to) {
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

fs::path find_project_root() {
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

fs::path get_build_dir(bool release) {
    // Always use the project root, never create build dirs next to source files
    fs::path project_root = find_project_root();

    // Create build directory structure in project root
    fs::path build_dir = project_root / "build" / (release ? "release" : "debug");
    fs::create_directories(build_dir);

    return build_dir;
}

fs::path get_deps_cache_dir() {
    // Always use project root for deps cache
    fs::path project_root = find_project_root();
    fs::path deps = project_root / "build" / "debug" / "deps";
    fs::create_directories(deps);
    return deps;
}

fs::path get_run_cache_dir() {
    // Always use project root for run cache
    fs::path project_root = find_project_root();
    fs::path cache = project_root / "build" / "debug" / ".run-cache";
    fs::create_directories(cache);
    return cache;
}

// ============================================================================
// Diagnostic Helpers
// ============================================================================

void emit_lexer_error(DiagnosticEmitter& emitter, const lexer::LexerError& error) {
    emitter.error("L001", error.message, error.span);
}

void emit_parser_error(DiagnosticEmitter& emitter, const parser::ParseError& error) {
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

void emit_type_error(DiagnosticEmitter& emitter, const types::TypeError& error) {
    emitter.error("T001", error.message, error.span, error.notes);
}

void emit_codegen_error(DiagnosticEmitter& emitter, const codegen::LLVMGenError& error) {
    emitter.error("C001", error.message, error.span, error.notes);
}

void emit_all_lexer_errors(DiagnosticEmitter& emitter, const lexer::Lexer& lex) {
    for (const auto& error : lex.errors()) {
        emit_lexer_error(emitter, error);
    }
}

void emit_all_parser_errors(DiagnosticEmitter& emitter,
                            const std::vector<parser::ParseError>& errors) {
    for (const auto& error : errors) {
        emit_parser_error(emitter, error);
    }
}

void emit_all_type_errors(DiagnosticEmitter& emitter, const std::vector<types::TypeError>& errors) {
    for (const auto& error : errors) {
        emit_type_error(emitter, error);
    }
}

void emit_all_codegen_errors(DiagnosticEmitter& emitter,
                             const std::vector<codegen::LLVMGenError>& errors) {
    for (const auto& error : errors) {
        emit_codegen_error(emitter, error);
    }
}

void emit_borrow_error(DiagnosticEmitter& emitter, const borrow::BorrowError& error) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;

    // Use error code from the error struct for more specific codes
    // Map BorrowErrorCode to string code
    switch (error.code) {
    case borrow::BorrowErrorCode::UseAfterMove:
        diag.code = "B001";
        break;
    case borrow::BorrowErrorCode::MoveWhileBorrowed:
        diag.code = "B002";
        break;
    case borrow::BorrowErrorCode::AssignNotMutable:
        diag.code = "B003";
        break;
    case borrow::BorrowErrorCode::AssignWhileBorrowed:
        diag.code = "B004";
        break;
    case borrow::BorrowErrorCode::BorrowAfterMove:
        diag.code = "B005";
        break;
    case borrow::BorrowErrorCode::MutBorrowNotMutable:
        diag.code = "B006";
        break;
    case borrow::BorrowErrorCode::MutBorrowWhileImmut:
        diag.code = "B007";
        break;
    case borrow::BorrowErrorCode::DoubleMutBorrow:
        diag.code = "B008";
        break;
    case borrow::BorrowErrorCode::ImmutBorrowWhileMut:
        diag.code = "B009";
        break;
    case borrow::BorrowErrorCode::ReturnLocalRef:
        diag.code = "B010";
        break;
    case borrow::BorrowErrorCode::PartialMove:
        diag.code = "B011";
        break;
    case borrow::BorrowErrorCode::OverlappingBorrow:
        diag.code = "B012";
        break;
    case borrow::BorrowErrorCode::UseWhileBorrowed:
        diag.code = "B013";
        break;
    case borrow::BorrowErrorCode::ClosureCapturesMoved:
        diag.code = "B014";
        break;
    case borrow::BorrowErrorCode::ClosureCaptureConflict:
        diag.code = "B015";
        break;
    case borrow::BorrowErrorCode::PartiallyMovedValue:
        diag.code = "B016";
        break;
    case borrow::BorrowErrorCode::ReborrowOutlivesOrigin:
        diag.code = "B017";
        break;
    case borrow::BorrowErrorCode::AmbiguousReturnLifetime:
        diag.code = "E031";
        break;
    case borrow::BorrowErrorCode::InteriorMutWarning:
        diag.code = "W001";
        break;
    default:
        diag.code = "B099";
        break;
    }

    diag.message = error.message;
    diag.primary_span = error.span;
    diag.notes = error.notes;

    // Add related span as a secondary label if present
    if (error.related_span) {
        // Use the specific related_message if provided, otherwise use a generic message
        std::string label_message = error.related_message.value_or("related location here");
        diag.labels.push_back(DiagnosticLabel{
            .span = *error.related_span, .message = label_message, .is_primary = false});
    }

    // Add suggestions as notes
    for (const auto& suggestion : error.suggestions) {
        std::string note = "help: " + suggestion.message;
        if (suggestion.fix) {
            note += ": `" + *suggestion.fix + "`";
        }
        diag.notes.push_back(note);
    }

    emitter.emit(diag);
}

void emit_all_borrow_errors(DiagnosticEmitter& emitter,
                            const std::vector<borrow::BorrowError>& errors) {
    for (const auto& error : errors) {
        emit_borrow_error(emitter, error);
    }
}

// ============================================================================
// Module Helpers
// ============================================================================

bool has_bench_functions(const parser::Module& module) {
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

// Check if module uses socket lowlevel functions (requires net.c runtime)
bool has_socket_functions(const parser::Module& module) {
    // Names matching std::net::sys TML declarations (compiler adds tml_ prefix)
    static const std::vector<std::string> socket_funcs = {"sys_socket",
                                                          "sys_bind_v4",
                                                          "sys_bind_v6",
                                                          "sys_listen",
                                                          "sys_accept_v4",
                                                          "sys_connect_v4",
                                                          "sys_connect_v6",
                                                          "sys_send",
                                                          "sys_recv",
                                                          "sys_sendto_v4",
                                                          "sys_recvfrom_v4",
                                                          "sys_shutdown",
                                                          "sys_close",
                                                          "sys_set_nonblocking",
                                                          "sys_setsockopt",
                                                          "sys_getsockopt",
                                                          "sys_setsockopt_timeout",
                                                          "sys_getsockopt_timeout",
                                                          "sys_getsockname_v4",
                                                          "sys_getpeername_v4",
                                                          "sys_get_last_error",
                                                          "sys_wsa_startup",
                                                          "sys_wsa_cleanup"};

    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            if (func.is_unsafe) { // lowlevel func has is_unsafe = true
                for (const auto& socket_func : socket_funcs) {
                    if (func.name == socket_func) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

std::vector<fs::path> get_runtime_objects(const std::shared_ptr<types::ModuleRegistry>& registry,
                                          const parser::Module& module,
                                          const std::string& deps_cache, const std::string& clang,
                                          bool verbose) {
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

    // Check for pre-compiled runtime library first (self-contained mode)
    std::string runtime_lib = find_runtime_library();
    bool use_precompiled = !runtime_lib.empty() && !CompilerOptions::check_leaks;

    if (use_precompiled) {
        // Use pre-compiled runtime library (no clang needed)
        objects.push_back(fs::path(runtime_lib));
        if (verbose) {
            std::cout << "Using pre-compiled runtime: " << runtime_lib << "\n";
        }
    } else {
        // Fall back to compiling individual C files with clang
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

            // Determine if memory tracking is enabled
            std::string mem_flags = "";
            if (CompilerOptions::check_leaks) {
                mem_flags = "-DTML_DEBUG_MEMORY";
            }

            fs::path mem_c = runtime_dir / "mem.c";
            if (fs::exists(mem_c)) {
                std::string mem_obj = ensure_c_compiled(to_forward_slashes(mem_c.string()),
                                                        deps_cache, clang, verbose, mem_flags);
                objects.push_back(fs::path(mem_obj));
                if (verbose) {
                    std::cout << "Including mem runtime: " << mem_obj << "\n";
                }
            }

            // Include memory tracking runtime when leak checking is enabled
            if (CompilerOptions::check_leaks) {
                fs::path mem_track_c = runtime_dir / "mem_track.c";
                if (fs::exists(mem_track_c)) {
                    std::string mem_track_obj =
                        ensure_c_compiled(to_forward_slashes(mem_track_c.string()), deps_cache,
                                          clang, verbose, mem_flags);
                    objects.push_back(fs::path(mem_track_obj));
                    if (verbose) {
                        std::cout << "Including mem_track runtime (leak checking): "
                                  << mem_track_obj << "\n";
                    }
                }
            }

            // Also include time.c by default (commonly used for timing/sleep)
            fs::path time_c = runtime_dir / "time.c";
            if (fs::exists(time_c)) {
                std::string time_obj = ensure_c_compiled(to_forward_slashes(time_c.string()),
                                                         deps_cache, clang, verbose);
                objects.push_back(fs::path(time_obj));
                if (verbose) {
                    std::cout << "Including time runtime: " << time_obj << "\n";
                }
            }

            // Include async.c for async/await executor support
            fs::path async_c = runtime_dir / "async.c";
            if (fs::exists(async_c)) {
                std::string async_obj = ensure_c_compiled(to_forward_slashes(async_c.string()),
                                                          deps_cache, clang, verbose);
                objects.push_back(fs::path(async_obj));
                if (verbose) {
                    std::cout << "Including async runtime: " << async_obj << "\n";
                }
            }

            // Include math.c for math builtins (sqrt, pow, int_to_float, etc.)
            fs::path math_c = runtime_dir / "math.c";
            if (fs::exists(math_c)) {
                std::string math_obj = ensure_c_compiled(to_forward_slashes(math_c.string()),
                                                         deps_cache, clang, verbose);
                objects.push_back(fs::path(math_obj));
                if (verbose) {
                    std::cout << "Including math runtime: " << math_obj << "\n";
                }
            }

            // Include text.c for Text type (used by template literals $"...")
            fs::path text_c = runtime_dir / "text.c";
            if (fs::exists(text_c)) {
                std::string text_obj = ensure_c_compiled(to_forward_slashes(text_c.string()),
                                                         deps_cache, clang, verbose);
                objects.push_back(fs::path(text_obj));
                if (verbose) {
                    std::cout << "Including text runtime: " << text_obj << "\n";
                }
            }

            // Include net.c by default (commonly used by std::net, and needed for test suites)
            // The cost of including it is minimal and prevents hard-to-debug linking errors
            fs::path net_c = runtime_dir / "net.c";
            if (fs::exists(net_c)) {
                std::string net_obj = ensure_c_compiled(to_forward_slashes(net_c.string()),
                                                        deps_cache, clang, verbose);
                objects.push_back(fs::path(net_obj));
                if (verbose) {
                    std::cout << "Including net runtime: " << net_obj << "\n";
                }
            }

            // Include collections.c (needed by string.c for list_* functions)
            fs::path collections_c = runtime_dir / "collections.c";
            if (fs::exists(collections_c)) {
                std::string collections_obj = ensure_c_compiled(
                    to_forward_slashes(collections_c.string()), deps_cache, clang, verbose);
                objects.push_back(fs::path(collections_obj));
                if (verbose) {
                    std::cout << "Including collections runtime: " << collections_obj << "\n";
                }
            }

            // Include sync.c for Mutex, RwLock, Condvar runtime functions
            fs::path sync_c = runtime_dir / "sync.c";
            if (fs::exists(sync_c)) {
                std::string sync_obj = ensure_c_compiled(to_forward_slashes(sync_c.string()),
                                                         deps_cache, clang, verbose);
                objects.push_back(fs::path(sync_obj));
                if (verbose) {
                    std::cout << "Including sync runtime: " << sync_obj << "\n";
                }
            }

            // Include thread.c for thread management functions
            fs::path thread_c = runtime_dir / "thread.c";
            if (fs::exists(thread_c)) {
                std::string thread_obj = ensure_c_compiled(to_forward_slashes(thread_c.string()),
                                                           deps_cache, clang, verbose);
                objects.push_back(fs::path(thread_obj));
                if (verbose) {
                    std::cout << "Including thread runtime: " << thread_obj << "\n";
                }
            }

            // Include crypto.c for cryptographic functions (CSPRNG, etc.)
            fs::path crypto_c = runtime_dir / "crypto.c";
            if (fs::exists(crypto_c)) {
                std::string crypto_obj = ensure_c_compiled(to_forward_slashes(crypto_c.string()),
                                                           deps_cache, clang, verbose);
                objects.push_back(fs::path(crypto_obj));
                if (verbose) {
                    std::cout << "Including crypto runtime: " << crypto_obj << "\n";
                }
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

    // Link coverage runtime if coverage is enabled (even without test module)
    if (CompilerOptions::coverage && !registry->has_module("test")) {
        add_runtime(
            {
                "lib/test/runtime/coverage.c",
                "../../../lib/test/runtime/coverage.c",
                "F:/Node/hivellm/tml/lib/test/runtime/coverage.c",
            },
            "coverage");
    }

    // NOTE: std::collections runtime is already provided by compiler/runtime/collections.c
    // which is always linked above. Do not link lib/std/runtime/collections.c as it has
    // different struct layouts that cause memory corruption when both are linked.

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

    // Link std::text runtime if imported (Text type with SSO)
    // Skip if using precompiled tml_runtime.lib which already includes text.c
    if (registry->has_module("std::text") && !use_precompiled) {
        add_runtime(
            {
                "compiler/runtime/text.c",
                "runtime/text.c",
                "../runtime/text.c",
                "F:/Node/hivellm/tml/compiler/runtime/text.c",
            },
            "std::text");
    }

    // Note: net.c is now included by default (see above), so no conditional linking needed

    // Link std::json runtime if imported (pre-built C++ library)
    // Check for std::json or any submodule (std::json::types, std::json::builder, etc.)
    auto uses_json_module = [&registry]() -> bool {
        if (registry->has_module("std::json"))
            return true;
        for (const auto& [path, _] : registry->get_all_modules()) {
            if (path.find("std::json::") == 0 || path == "std::json") {
                return true;
            }
        }
        return false;
    };
    if (uses_json_module()) {
        // Find the JSON runtime library (built alongside tml.exe)
        auto find_json_runtime = []() -> std::optional<fs::path> {
#ifdef _WIN32
            std::string lib_name = "tml_json_runtime.lib";
#else
            std::string lib_name = "libtml_json_runtime.a";
#endif
            // Search locations: prioritize release when optimizing, debug otherwise
            std::vector<std::string> search_paths;
            bool is_release = CompilerOptions::optimization_level >= 1;
            if (is_release) {
                // Release mode: prioritize optimized libraries
                search_paths = {
                    ".",
                    "build/release",
                    "build/debug",
                    "../build/release",
                    "../build/debug",
                    "F:/Node/hivellm/tml/build/release",
                    "F:/Node/hivellm/tml/build/debug",
                };
            } else {
                // Debug mode: prioritize debug libraries
                search_paths = {
                    ".",
                    "build/debug",
                    "build/release",
                    "../build/debug",
                    "../build/release",
                    "F:/Node/hivellm/tml/build/debug",
                    "F:/Node/hivellm/tml/build/release",
                };
            }

            for (const auto& search_path : search_paths) {
                fs::path lib_path = fs::path(search_path) / lib_name;
                if (fs::exists(lib_path)) {
                    return fs::absolute(lib_path);
                }
            }
            return std::nullopt;
        };

        if (auto json_lib = find_json_runtime()) {
            objects.push_back(*json_lib);
            if (verbose) {
                std::cout << "Including JSON runtime library: " << json_lib->string() << "\n";
            }

            // Also need to link tml_json.lib which contains the actual JSON parser
            // (tml_json_runtime.lib depends on it)
            auto json_lib_dir = json_lib->parent_path();
#ifdef _WIN32
            auto tml_json_lib = json_lib_dir / "tml_json.lib";
#else
            auto tml_json_lib = json_lib_dir / "libtml_json.a";
#endif
            if (fs::exists(tml_json_lib)) {
                objects.push_back(tml_json_lib);
                if (verbose) {
                    std::cout << "Including JSON parser library: " << tml_json_lib.string() << "\n";
                }
            } else if (verbose) {
                std::cout << "Warning: tml_json library not found at " << tml_json_lib.string()
                          << "\n";
            }
        } else if (verbose) {
            std::cout << "Warning: std::json imported but tml_json_runtime library not found\n";
        }
    }

    // Link std::profiler runtime if imported (pre-built C++ library)
    if (registry->has_module("std::profiler")) {
        // Find the profiler runtime library (built alongside tml.exe)
        auto find_profiler_runtime = []() -> std::optional<fs::path> {
#ifdef _WIN32
            std::string lib_name = "tml_profiler.lib";
#else
            std::string lib_name = "libtml_profiler.a";
#endif
            // Search locations: prioritize release when optimizing, debug otherwise
            std::vector<std::string> search_paths;
            if (CompilerOptions::optimization_level >= 1) {
                // Release mode: prioritize optimized libraries
                search_paths = {
                    ".",
                    "build/release",
                    "build/debug",
                    "../build/release",
                    "../build/debug",
                    "F:/Node/hivellm/tml/build/release",
                    "F:/Node/hivellm/tml/build/debug",
                };
            } else {
                // Debug mode: prioritize debug libraries
                search_paths = {
                    ".",
                    "build/debug",
                    "build/release",
                    "../build/debug",
                    "../build/release",
                    "F:/Node/hivellm/tml/build/debug",
                    "F:/Node/hivellm/tml/build/release",
                };
            }

            for (const auto& search_path : search_paths) {
                fs::path lib_path = fs::path(search_path) / lib_name;
                if (fs::exists(lib_path)) {
                    return fs::absolute(lib_path);
                }
            }
            return std::nullopt;
        };

        if (auto profiler_lib = find_profiler_runtime()) {
            objects.push_back(*profiler_lib);
            if (verbose) {
                std::cout << "Including profiler runtime library: " << profiler_lib->string()
                          << "\n";
            }
        } else if (verbose) {
            std::cout << "Warning: std::profiler imported but tml_profiler library not found\n";
        }
    }

    return objects;
}

// ============================================================================
// Preprocessor Helpers
// ============================================================================

void emit_preprocessor_diagnostic(DiagnosticEmitter& emitter,
                                  const preprocessor::PreprocessorDiagnostic& diag,
                                  const std::string& filename) {
    Diagnostic d;
    d.code = diag.severity == preprocessor::DiagnosticSeverity::Error ? "P001" : "P002";
    d.severity = diag.severity == preprocessor::DiagnosticSeverity::Error
                     ? DiagnosticSeverity::Error
                     : DiagnosticSeverity::Warning;
    d.message = diag.message;
    // Create a span for the error location
    SourceLocation loc;
    loc.file = filename;
    loc.line = static_cast<uint32_t>(diag.line);
    loc.column = static_cast<uint32_t>(diag.column);
    loc.offset = 0;
    loc.length = 1;
    d.primary_span = SourceSpan{.start = loc, .end = loc};
    emitter.emit(d);
}

void emit_all_preprocessor_diagnostics(DiagnosticEmitter& emitter,
                                       const preprocessor::PreprocessorResult& result,
                                       const std::string& filename) {
    for (const auto& diag : result.diagnostics) {
        emit_preprocessor_diagnostic(emitter, diag, filename);
    }
}

preprocessor::Preprocessor get_configured_preprocessor(const BuildOptions& options) {
    preprocessor::PreprocessorConfig config = preprocessor::Preprocessor::host_config();

    // Set build mode
    if (options.release || options.optimization_level >= 2) {
        config.build_mode = preprocessor::BuildMode::Release;
    } else if (options.debug) {
        config.build_mode = preprocessor::BuildMode::Debug;
    }

    // Parse target if specified
    if (!options.target.empty()) {
        config = preprocessor::Preprocessor::parse_target_triple(options.target);
        // Preserve build mode from options
        if (options.release || options.optimization_level >= 2) {
            config.build_mode = preprocessor::BuildMode::Release;
        }
    }

    // Add user defines
    for (const auto& def : options.defines) {
        // Parse SYMBOL or SYMBOL=VALUE
        auto eq_pos = def.find('=');
        if (eq_pos != std::string::npos) {
            config.defines[def.substr(0, eq_pos)] = def.substr(eq_pos + 1);
        } else {
            config.defines[def] = "";
        }
    }

    return preprocessor::Preprocessor(config);
}

preprocessor::PreprocessorResult preprocess_source(const std::string& source,
                                                   const std::string& filename,
                                                   const BuildOptions& options) {
    auto pp = get_configured_preprocessor(options);
    return pp.process(source, filename);
}

} // namespace tml::cli::build
