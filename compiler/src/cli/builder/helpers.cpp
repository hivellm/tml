TML_MODULE("compiler")

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
// OpenSSL Detection
// ============================================================================

OpenSSLPaths find_openssl() {
    OpenSSLPaths result;

#ifdef _WIN32
    // Check vcpkg_installed first (project-local)
    fs::path project_root = find_project_root();
    fs::path vcpkg_dir = project_root / "vcpkg_installed" / "x64-windows";
    if (fs::exists(vcpkg_dir / "include" / "openssl" / "evp.h")) {
        result.found = true;
        result.include_dir = vcpkg_dir / "include";
        result.lib_dir = vcpkg_dir / "lib";
        result.crypto_lib = "libcrypto.lib";
        result.ssl_lib = "libssl.lib";
        TML_LOG_DEBUG("build", "OpenSSL found via vcpkg: " << vcpkg_dir.string());
        return result;
    }

    // Check standalone OpenSSL install
    fs::path standalone = "C:/Program Files/OpenSSL-Win64";
    if (fs::exists(standalone / "include" / "openssl" / "evp.h")) {
        result.found = true;
        result.include_dir = standalone / "include";
        result.lib_dir = standalone / "lib";
        if (fs::exists(standalone / "lib" / "libcrypto_static.lib")) {
            result.crypto_lib = "libcrypto_static.lib";
            result.ssl_lib = "libssl_static.lib";
        } else {
            result.crypto_lib = "libcrypto.lib";
            result.ssl_lib = "libssl.lib";
        }
        TML_LOG_DEBUG("build", "OpenSSL found standalone: " << standalone.string());
        return result;
    }
#else
    // Unix: check system paths
    if (fs::exists("/usr/include/openssl/evp.h")) {
        result.found = true;
        result.include_dir = "/usr/include";
        result.lib_dir = "/usr/lib";
        result.crypto_lib = "libcrypto.so";
        result.ssl_lib = "libssl.so";
        return result;
    }
    if (fs::exists("/usr/local/include/openssl/evp.h")) {
        result.found = true;
        result.include_dir = "/usr/local/include";
        result.lib_dir = "/usr/local/lib";
        result.crypto_lib = "libcrypto.so";
        result.ssl_lib = "libssl.so";
        return result;
    }
#endif

    return result;
}

bool has_crypto_modules(const std::shared_ptr<types::ModuleRegistry>& registry) {
    if (!registry)
        return false;
    return registry->has_module("std::crypto") || registry->has_module("std::crypto::hash") ||
           registry->has_module("std::crypto::hmac") ||
           registry->has_module("std::crypto::cipher") ||
           registry->has_module("std::crypto::random") ||
           registry->has_module("std::crypto::x509") || registry->has_module("std::crypto::key") ||
           registry->has_module("std::crypto::sign") || registry->has_module("std::crypto::dh") ||
           registry->has_module("std::crypto::ecdh") || registry->has_module("std::crypto::kdf") ||
           registry->has_module("std::crypto::rsa") || registry->has_module("std::hash") ||
           registry->has_module("std::net::tls");
}

// ============================================================================
// Diagnostic Helpers
// ============================================================================

void emit_lexer_error(DiagnosticEmitter& emitter, const lexer::LexerError& error) {
    // Use specific error code if provided, otherwise default to L001
    std::string code = error.code.empty() ? "L001" : error.code;
    emitter.error(code, error.message, error.span);
}

void emit_parser_error(DiagnosticEmitter& emitter, const parser::ParseError& error) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    // Use specific error code if provided, otherwise default to P001
    diag.code = error.code.empty() ? "P001" : error.code;
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
    // Use specific error code if provided, otherwise default to T001
    std::string code = error.code.empty() ? "T001" : error.code;
    emitter.error(code, error.message, error.span, error.notes);
}

void emit_codegen_error(DiagnosticEmitter& emitter, const codegen::LLVMGenError& error) {
    // Use specific error code if provided, otherwise default to C001
    std::string code = error.code.empty() ? "C001" : error.code;
    emitter.error(code, error.message, error.span, error.notes);
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
    // Check if any non-cascading errors exist
    bool has_root_cause = false;
    for (const auto& error : errors) {
        if (!error.is_cascading) {
            has_root_cause = true;
            break;
        }
    }

    // Deduplicate by (code, line, column) to avoid duplicate errors from different code paths
    std::set<std::tuple<std::string, uint32_t, uint32_t>> seen;
    size_t suppressed = 0;

    for (const auto& error : errors) {
        // Suppress cascading errors when root-cause errors exist
        if (error.is_cascading && has_root_cause) {
            ++suppressed;
            continue;
        }

        // Skip duplicate errors at the same location
        auto key = std::make_tuple(error.code, error.span.start.line, error.span.start.column);
        if (!seen.insert(key).second) {
            ++suppressed;
            continue;
        }

        emit_type_error(emitter, error);
    }

    if (suppressed > 0) {
        std::cerr << "note: " << suppressed
                  << " additional error(s) suppressed (likely caused by previous error)\n";
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
    // Deduplicate by (message, line, column) to avoid duplicate borrow errors
    std::set<std::tuple<std::string, uint32_t, uint32_t>> seen;
    size_t suppressed = 0;

    for (const auto& error : errors) {
        auto key = std::make_tuple(error.message, error.span.start.line, error.span.start.column);
        if (!seen.insert(key).second) {
            ++suppressed;
            continue;
        }
        emit_borrow_error(emitter, error);
    }

    if (suppressed > 0) {
        std::cerr << "note: " << suppressed << " duplicate borrow error(s) suppressed\n";
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
                                                          "sys_peek",
                                                          "sys_sendto_v4",
                                                          "sys_recvfrom_v4",
                                                          "sys_shutdown",
                                                          "sys_close",
                                                          "sys_set_nonblocking",
                                                          "sys_setsockopt",
                                                          "sys_getsockopt",
                                                          "sys_getsockopt_value",
                                                          "sys_setsockopt_timeout",
                                                          "sys_getsockopt_timeout",
                                                          "sys_getsockname_v4",
                                                          "sys_getpeername_v4",
                                                          "sys_sockaddr_get_ip",
                                                          "sys_sockaddr_get_port",
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
                TML_LOG_DEBUG("build", "Including " << name << ": " << obj);
                return;
            }
        }
    };

    // Check for pre-compiled runtime library first (self-contained mode)
    // Disable precompiled runtime when coverage or leak checking is enabled,
    // because those require linking additional runtime components (coverage.c, mem_track.c)
    // Also disable when crypto modules are used, since the precompiled lib doesn't have OpenSSL
    std::string runtime_lib = find_runtime_library();
    bool needs_crypto = has_crypto_modules(registry);
    bool use_precompiled = !runtime_lib.empty() && !CompilerOptions::check_leaks &&
                           !CompilerOptions::coverage && !needs_crypto;

    if (use_precompiled) {
        // Use pre-compiled runtime library (no clang needed)
        objects.push_back(fs::path(runtime_lib));
        TML_LOG_DEBUG("build", "Using pre-compiled runtime: " << runtime_lib);
    } else {
        // Fall back to compiling individual C files with clang
        // Essential runtime (IO functions)
        std::string runtime_path = find_runtime();
        if (!runtime_path.empty()) {
            std::string obj = ensure_c_compiled(runtime_path, deps_cache, clang, verbose);
            objects.push_back(fs::path(obj));
            TML_LOG_DEBUG("build", "Including runtime: " << obj);

            // Runtime is organized into themed subdirectories:
            //   core/, memory/, text/, collections/, math/, time/,
            //   concurrency/, net/, os/, crypto/, diagnostics/
            // find_runtime() returns path to core/essential.c, go up one level for runtime root
            fs::path runtime_dir = fs::path(runtime_path).parent_path().parent_path();

            // diagnostics/ - log.c (required by essential.c, text.c, backtrace.c)
            fs::path log_c = runtime_dir / "diagnostics" / "log.c";
            if (fs::exists(log_c)) {
                std::string log_obj = ensure_c_compiled(to_forward_slashes(log_c.string()),
                                                        deps_cache, clang, verbose);
                objects.push_back(fs::path(log_obj));
                TML_LOG_DEBUG("build", "Including log runtime: " << log_obj);
            }

            // string.c removed — all functions migrated to inline LLVM IR (Phase 31)

            // Determine if memory tracking is enabled
            std::string mem_flags = "";
            if (CompilerOptions::check_leaks) {
                mem_flags = "-DTML_DEBUG_MEMORY";
            }

            // memory/ - mem.c
            fs::path mem_c = runtime_dir / "memory" / "mem.c";
            if (fs::exists(mem_c)) {
                std::string mem_obj = ensure_c_compiled(to_forward_slashes(mem_c.string()),
                                                        deps_cache, clang, verbose, mem_flags);
                objects.push_back(fs::path(mem_obj));
                TML_LOG_DEBUG("build", "Including mem runtime: " << mem_obj);
            }

            // memory/ - mem_track.c (when leak checking is enabled)
            if (CompilerOptions::check_leaks) {
                fs::path mem_track_c = runtime_dir / "memory" / "mem_track.c";
                if (fs::exists(mem_track_c)) {
                    std::string mem_track_obj =
                        ensure_c_compiled(to_forward_slashes(mem_track_c.string()), deps_cache,
                                          clang, verbose, mem_flags);
                    objects.push_back(fs::path(mem_track_obj));
                    TML_LOG_DEBUG("build",
                                  "Including mem_track runtime (leak checking): " << mem_track_obj);
                }
            }

            // time/ - time.c
            fs::path time_c = runtime_dir / "time" / "time.c";
            if (fs::exists(time_c)) {
                std::string time_obj = ensure_c_compiled(to_forward_slashes(time_c.string()),
                                                         deps_cache, clang, verbose);
                objects.push_back(fs::path(time_obj));
                TML_LOG_DEBUG("build", "Including time runtime: " << time_obj);
            }

            // concurrency/ - async.c (async executor, timer, yield, channel)
            fs::path async_c = runtime_dir / "concurrency" / "async.c";
            if (fs::exists(async_c)) {
                std::string async_obj = ensure_c_compiled(to_forward_slashes(async_c.string()),
                                                          deps_cache, clang, verbose);
                objects.push_back(fs::path(async_obj));
                TML_LOG_DEBUG("build", "Including async runtime: " << async_obj);
            }

            // math.c removed — all functions migrated to inline LLVM IR (Phase 32)

            // text.c removed — Text migrated to pure TML (Phase 30)

            // net/ - net.c (included by default for std::net)
            fs::path net_c = runtime_dir / "net" / "net.c";
            if (fs::exists(net_c)) {
                std::string net_obj = ensure_c_compiled(to_forward_slashes(net_c.string()),
                                                        deps_cache, clang, verbose);
                objects.push_back(fs::path(net_obj));
                TML_LOG_DEBUG("build", "Including net runtime: " << net_obj);
            }

            // net/ - dns.c (DNS resolution functions)
            fs::path dns_c = runtime_dir / "net" / "dns.c";
            if (fs::exists(dns_c)) {
                std::string dns_obj = ensure_c_compiled(to_forward_slashes(dns_c.string()),
                                                        deps_cache, clang, verbose);
                objects.push_back(fs::path(dns_obj));
                TML_LOG_DEBUG("build", "Including dns runtime: " << dns_obj);
            }

            // collections/ - collections.c (buffer FFI for crypto/zlib)
            fs::path collections_c = runtime_dir / "collections" / "collections.c";
            if (fs::exists(collections_c)) {
                std::string collections_obj = ensure_c_compiled(
                    to_forward_slashes(collections_c.string()), deps_cache, clang, verbose);
                objects.push_back(fs::path(collections_obj));
                TML_LOG_DEBUG("build", "Including collections runtime: " << collections_obj);
            }

            // concurrency/ - sync.c
            fs::path sync_c = runtime_dir / "concurrency" / "sync.c";
            if (fs::exists(sync_c)) {
                std::string sync_obj = ensure_c_compiled(to_forward_slashes(sync_c.string()),
                                                         deps_cache, clang, verbose);
                objects.push_back(fs::path(sync_obj));
                TML_LOG_DEBUG("build", "Including sync runtime: " << sync_obj);
            }

            // thread.c removed — replaced by sync.c (Phase 30)

            // crypto/ - crypto.c, crypto_key.c, crypto_x509.c, etc.
            // Only include crypto runtime objects when the program actually uses
            // crypto modules. These objects require OpenSSL at link time, so
            // including them unconditionally causes link failures for non-crypto code.
            if (needs_crypto) {
                std::string crypto_extra_flags;
                {
                    auto openssl = find_openssl();
                    if (openssl.found) {
                        crypto_extra_flags = "-DTML_HAS_OPENSSL=1 -I\"" +
                                             to_forward_slashes(openssl.include_dir.string()) +
                                             "\"";
                    }
                }
                fs::path crypto_c = runtime_dir / "crypto" / "crypto.c";
                if (fs::exists(crypto_c)) {
                    std::string crypto_obj =
                        ensure_c_compiled(to_forward_slashes(crypto_c.string()), deps_cache, clang,
                                          verbose, crypto_extra_flags);
                    objects.push_back(fs::path(crypto_obj));
                    TML_LOG_DEBUG("build", "Including crypto runtime: " << crypto_obj);
                }
                fs::path crypto_key_c = runtime_dir / "crypto" / "crypto_key.c";
                if (fs::exists(crypto_key_c)) {
                    std::string crypto_key_obj =
                        ensure_c_compiled(to_forward_slashes(crypto_key_c.string()), deps_cache,
                                          clang, verbose, crypto_extra_flags);
                    objects.push_back(fs::path(crypto_key_obj));
                    TML_LOG_DEBUG("build", "Including crypto_key runtime: " << crypto_key_obj);
                }
                // Compile all additional crypto runtime files
                for (const auto& crypto_file : {"crypto_x509.c", "crypto_dh.c", "crypto_ecdh.c",
                                                "crypto_kdf.c", "crypto_rsa.c", "crypto_sign.c"}) {
                    fs::path crypto_extra_c = runtime_dir / "crypto" / crypto_file;
                    if (fs::exists(crypto_extra_c)) {
                        std::string crypto_extra_obj =
                            ensure_c_compiled(to_forward_slashes(crypto_extra_c.string()),
                                              deps_cache, clang, verbose, crypto_extra_flags);
                        objects.push_back(fs::path(crypto_extra_obj));
                        TML_LOG_DEBUG("build", "Including " << crypto_file
                                                            << " runtime: " << crypto_extra_obj);
                    }
                }

                // net/ - tls.c (TLS/SSL support, requires OpenSSL)
                fs::path tls_c = runtime_dir / "net" / "tls.c";
                if (fs::exists(tls_c)) {
                    std::string tls_obj =
                        ensure_c_compiled(to_forward_slashes(tls_c.string()), deps_cache, clang,
                                          verbose, crypto_extra_flags);
                    objects.push_back(fs::path(tls_obj));
                    TML_LOG_DEBUG("build", "Including tls runtime: " << tls_obj);
                }
            }

            // os/ - os.c
            fs::path os_c = runtime_dir / "os" / "os.c";
            if (fs::exists(os_c)) {
                std::string os_obj = ensure_c_compiled(to_forward_slashes(os_c.string()),
                                                       deps_cache, clang, verbose);
                objects.push_back(fs::path(os_obj));
                TML_LOG_DEBUG("build", "Including os runtime: " << os_obj);
            }

            // search/ - search.c (vector distance functions)
            fs::path search_c = runtime_dir / "search" / "search.c";
            if (fs::exists(search_c)) {
                std::string search_obj = ensure_c_compiled(to_forward_slashes(search_c.string()),
                                                           deps_cache, clang, verbose);
                objects.push_back(fs::path(search_obj));
                TML_LOG_DEBUG("build", "Including search runtime: " << search_obj);
            }

            // diagnostics/ - backtrace.c
            fs::path backtrace_c = runtime_dir / "diagnostics" / "backtrace.c";
            if (fs::exists(backtrace_c)) {
                std::string backtrace_obj = ensure_c_compiled(
                    to_forward_slashes(backtrace_c.string()), deps_cache, clang, verbose);
                objects.push_back(fs::path(backtrace_obj));
                TML_LOG_DEBUG("build", "Including backtrace runtime: " << backtrace_obj);
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

    // NOTE: std::collections runtime is already provided by
    // compiler/runtime/collections/collections.c which is always linked above. Do not link
    // lib/std/runtime/collections.c as it has different struct layouts that cause memory corruption
    // when both are linked.

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

    // Link std::glob runtime if imported
    if (registry->has_module("std::glob")) {
        add_runtime(
            {
                "lib/std/runtime/glob.c",
                "../../../lib/std/runtime/glob.c",
                "F:/Node/hivellm/tml/lib/std/runtime/glob.c",
            },
            "std::glob");
    }

    // text.c removed — Text migrated to pure TML (Phase 22/30)

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
            TML_LOG_DEBUG("build", "Including JSON runtime library: " << json_lib->string());

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
                TML_LOG_DEBUG("build", "Including JSON parser library: " << tml_json_lib.string());
            } else {
                TML_LOG_WARN("build", "tml_json library not found at " << tml_json_lib.string());
            }
        } else {
            TML_LOG_WARN("build", "std::json imported but tml_json_runtime library not found");
        }
    }

    // Link std::zlib runtime if imported (pre-built C library with zlib, brotli, zstd)
    // Check for std::zlib or any submodule
    auto uses_zlib_module = [&registry]() -> bool {
        if (registry->has_module("std::zlib"))
            return true;
        for (const auto& [path, _] : registry->get_all_modules()) {
            if (path.find("std::zlib::") == 0 || path == "std::zlib") {
                return true;
            }
        }
        return false;
    };
    if (uses_zlib_module()) {
        // Find the zlib runtime library (built alongside tml.exe)
        auto find_zlib_runtime = []() -> std::optional<fs::path> {
#ifdef _WIN32
            std::string lib_name = "tml_zlib_runtime.lib";
#else
            std::string lib_name = "libtml_zlib_runtime.a";
#endif
            // Search locations: prioritize release when optimizing, debug otherwise
            std::vector<std::string> search_paths;
            bool is_release = CompilerOptions::optimization_level >= 1;
            if (is_release) {
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

        if (auto zlib_lib = find_zlib_runtime()) {
            objects.push_back(*zlib_lib);
            TML_LOG_DEBUG("build", "Including zlib runtime library: " << zlib_lib->string());

            // Also need to link the underlying compression libraries
            // (zstd, brotli, zlib) which are dependencies of tml_zlib_runtime
            auto find_vcpkg_lib = [](const std::string& lib_name) -> std::optional<fs::path> {
                std::vector<std::string> search_paths = {
                    "src/x64-windows/lib",
                    "src/x64-windows/debug/lib",
                    "../src/x64-windows/lib",
                    "../src/x64-windows/debug/lib",
                };
                for (const auto& path : search_paths) {
                    fs::path lib_path = fs::path(path) / (lib_name + ".lib");
                    if (fs::exists(lib_path)) {
                        return fs::absolute(lib_path);
                    }
                }
                return std::nullopt;
            };

            // Add zstd library
            if (auto zstd_lib = find_vcpkg_lib("zstd")) {
                objects.push_back(*zstd_lib);
                TML_LOG_DEBUG("build", "Including zstd library: " << zstd_lib->string());
            }

            // Add brotli libraries
            for (const auto& brotli_lib_name :
                 {"brotlicommon", "brotlidec", "brotlienc", "brotlicommon-static",
                  "brotlidec-static", "brotlienc-static"}) {
                if (auto brotli_lib = find_vcpkg_lib(brotli_lib_name)) {
                    objects.push_back(*brotli_lib);
                    TML_LOG_DEBUG("build", "Including brotli library: " << brotli_lib->string());
                }
            }

            // Add zlib library
            if (auto zlib_base_lib = find_vcpkg_lib("zlib")) {
                objects.push_back(*zlib_base_lib);
                TML_LOG_DEBUG("build", "Including zlib base library: " << zlib_base_lib->string());
            }
        } else {
            TML_LOG_WARN("build", "std::zlib imported but tml_zlib_runtime library not found");
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
            TML_LOG_DEBUG("build",
                          "Including profiler runtime library: " << profiler_lib->string());

            // Also link tml_log dependency (profiler uses TML_LOG_* macros)
#ifdef _WIN32
            std::string log_lib_name = "tml_log.lib";
#else
            std::string log_lib_name = "libtml_log.a";
#endif
            auto profiler_lib_dir = profiler_lib->parent_path();
            // Search: same dir as profiler, then recursively in parent/cache
            bool log_found = false;
            // Check same directory first
            auto log_same_dir = profiler_lib_dir / log_lib_name;
            if (fs::exists(log_same_dir)) {
                objects.push_back(fs::absolute(log_same_dir));
                log_found = true;
            }
            // Search build cache directory (CMake puts libs in cache/*/Debug/)
            if (!log_found) {
                auto cache_base = profiler_lib_dir.parent_path() / "cache";
                if (fs::exists(cache_base)) {
                    try {
                        for (auto& p : fs::recursive_directory_iterator(cache_base)) {
                            if (p.path().filename().string() == log_lib_name) {
                                objects.push_back(fs::absolute(p.path()));
                                log_found = true;
                                break;
                            }
                        }
                    } catch (...) {}
                }
            }
            if (log_found) {
                TML_LOG_DEBUG("build", "Including log library (profiler dependency)");
            } else {
                TML_LOG_WARN("build", "tml_log library not found (profiler dependency)");
            }
        } else {
            TML_LOG_WARN("build", "std::profiler imported but tml_profiler library not found");
        }
    }

    // Link std::search runtime if imported (pre-built C++ library)
    // Provides BM25, HNSW, TF-IDF vectorizer, and F32 SIMD distance functions
    auto uses_search_module = [&registry]() -> bool {
        if (registry->has_module("std::search"))
            return true;
        for (const auto& [path, _] : registry->get_all_modules()) {
            if (path.find("std::search::") == 0 || path == "std::search") {
                return true;
            }
        }
        return false;
    };
    if (uses_search_module()) {
        auto find_search_runtime = []() -> std::optional<fs::path> {
#ifdef _WIN32
            std::string lib_name = "tml_search_runtime.lib";
#else
            std::string lib_name = "libtml_search_runtime.a";
#endif
            std::vector<std::string> search_paths;
            bool is_release = CompilerOptions::optimization_level >= 1;
            if (is_release) {
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

        if (auto search_lib = find_search_runtime()) {
            objects.push_back(*search_lib);
            TML_LOG_DEBUG("build", "Including search runtime library: " << search_lib->string());

            // Also link tml_search.lib (search_engine.cpp depends on it)
            auto search_lib_dir = search_lib->parent_path();
#ifdef _WIN32
            std::string search_core_name = "tml_search.lib";
#else
            std::string search_core_name = "libtml_search.a";
#endif
            // Search in cache directory (CMake puts libs there)
            bool core_found = false;
            auto cache_base = search_lib_dir.parent_path() / "cache";
            if (fs::exists(cache_base)) {
                try {
                    for (auto& p : fs::recursive_directory_iterator(cache_base)) {
                        if (p.path().filename().string() == search_core_name) {
                            objects.push_back(fs::absolute(p.path()));
                            core_found = true;
                            TML_LOG_DEBUG("build",
                                          "Including search core library: " << p.path().string());
                            break;
                        }
                    }
                } catch (...) {}
            }
            if (!core_found) {
                // Check same directory
                auto same_dir = search_lib_dir / search_core_name;
                if (fs::exists(same_dir)) {
                    objects.push_back(fs::absolute(same_dir));
                    core_found = true;
                }
            }
            if (!core_found) {
                TML_LOG_WARN("build", "tml_search library not found (search runtime dependency)");
            }
        } else {
            TML_LOG_WARN("build", "std::search imported but tml_search_runtime library not found");
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
    d.code = diag.severity == preprocessor::DiagnosticSeverity::Error ? "PP001" : "PP002";
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
