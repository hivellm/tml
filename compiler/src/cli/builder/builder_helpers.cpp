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
#include "cli/builder/native_lib_resolver.hpp"
#include "cli/builder/platform.hpp"
#include "package/package_registry.hpp"

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

// Resolve the root used for TML build artifacts. Rust-style: when a workspace
// is active, artifacts land in `<workspace_root>/target/` so they never mix
// with the C++ compiler's CMake output in `<repo>/build/`. Without a workspace
// we fall back to the legacy `<project_root>/build/` layout for back-compat
// with standalone `.tml` files compiled from arbitrary directories.
static fs::path get_tml_artifact_root() {
    const auto& ws_root = tml::pkg::PackageRegistry::instance().workspace_root();
    if (!ws_root.empty()) {
        return ws_root / "target";
    }
    return find_project_root() / "build";
}

fs::path get_build_dir(bool release) {
    fs::path build_dir = get_tml_artifact_root() / (release ? "release" : "debug");
    fs::create_directories(build_dir);
    return build_dir;
}

fs::path get_deps_cache_dir() {
    fs::path deps = get_tml_artifact_root() / "debug" / "deps";
    fs::create_directories(deps);
    return deps;
}

fs::path get_run_cache_dir() {
    fs::path cache = get_tml_artifact_root() / "debug" / "cache" / "run";
    fs::create_directories(cache);
    return cache;
}

// ============================================================================
// OpenSSL Detection
// ============================================================================

OpenSSLPaths find_openssl() {
    OpenSSLPaths result;

    // Check NativeLibResolver Tier 0 first (compiler-bundled native libs)
    {
        auto platform = Platform::detect();
        NativeLibResolver resolver(platform);
        auto compiler_native = resolver.get_compiler_native_dir();
        if (!compiler_native.empty() && fs::exists(compiler_native)) {
            if (fs::exists(compiler_native / "libcrypto.lib")) {
                result.found = true;
                result.lib_dir = compiler_native;
                result.crypto_lib = "libcrypto.lib";
                result.ssl_lib = "libssl.lib";
                TML_LOG_DEBUG("build", "OpenSSL found via compiler-bundled native libs: "
                                           << compiler_native.string());
                return result;
            }
        }
    }

    // Legacy: vcpkg search (DEPRECATED)
    TML_LOG_DEBUG("build", "[native] DEPRECATED: searching vcpkg for OpenSSL. "
                           "Will use compiler-bundled native libs in future.");

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

SQLite3Paths find_sqlite3() {
    SQLite3Paths result;

    // Check NativeLibResolver Tier 0 first (compiler-bundled native libs)
    {
        auto platform = Platform::detect();
        NativeLibResolver resolver(platform);
        auto compiler_native = resolver.get_compiler_native_dir();
        if (!compiler_native.empty() && fs::exists(compiler_native)) {
            fs::path lib = compiler_native / "sqlite3.lib";
            if (fs::exists(lib)) {
                result.found = true;
                result.lib_path = lib;
                TML_LOG_DEBUG("build",
                              "SQLite3 found via compiler-bundled native libs: " << lib.string());
                return result;
            }
        }
    }

    // Legacy: vcpkg search (DEPRECATED)
    TML_LOG_DEBUG("build", "[native] DEPRECATED: searching vcpkg for SQLite3. "
                           "Will use compiler-bundled native libs in future.");

#ifdef _WIN32
    // Check vcpkg_installed (project-local)
    std::vector<fs::path> search_dirs = {
        "vcpkg_installed/x64-windows/lib",
        "../vcpkg_installed/x64-windows/lib",
        "F:/Node/hivellm/tml/vcpkg_installed/x64-windows/lib",
        "src/x64-windows/lib",
        "../src/x64-windows/lib",
    };
    for (const auto& dir : search_dirs) {
        fs::path lib = dir / "sqlite3.lib";
        if (fs::exists(lib)) {
            result.found = true;
            result.lib_path = fs::absolute(lib);
            return result;
        }
    }
#else
    // Unix: check vcpkg_installed, then system paths
    std::vector<fs::path> search_dirs = {
        "vcpkg_installed/x64-linux/lib",
        "../vcpkg_installed/x64-linux/lib",
        "/usr/lib",
        "/usr/local/lib",
        "/usr/lib/x86_64-linux-gnu",
    };
    for (const auto& dir : search_dirs) {
        fs::path lib = dir / "libsqlite3.a";
        if (fs::exists(lib)) {
            result.found = true;
            result.lib_path = fs::absolute(lib);
            return result;
        }
        // Also check .so
        fs::path so = dir / "libsqlite3.so";
        if (fs::exists(so)) {
            result.found = true;
            result.lib_path = fs::absolute(so);
            return result;
        }
    }
#endif

    return result;
}

bool has_crypto_modules(const std::shared_ptr<types::ModuleRegistry>& registry) {
    if (!registry)
        return false;
    // Check modules that depend on OpenSSL-backed C runtime (crypto.c + tls.c).
    // Note: std::hash does NOT belong here — its externs resolve to hash.c which is
    // compiled unconditionally and has zero OpenSSL dependencies.
    if (registry->has_module("std::net::tls") || registry->has_module("std::http::connection") ||
        registry->has_module("std::http::client"))
        return true;
    // Check any std::crypto submodule (covers constants, error, and future additions)
    for (const auto& [path, _] : registry->get_all_modules()) {
        if (path == "std::crypto" || path.find("std::crypto::") == 0)
            return true;
    }
    return false;
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

void emit_all_type_warnings(DiagnosticEmitter& emitter,
                            const std::vector<types::TypeError>& warnings) {
    for (const auto& w : warnings) {
        std::string code = w.code.empty() ? "S001" : w.code;
        emitter.warning(code, w.message, w.span, w.notes);
    }
}

void emit_hir_error(DiagnosticEmitter& emitter, const hir::HirError& error) {
    emitter.error(error.code, error.message, error.span, error.notes);
}

void emit_all_hir_errors(DiagnosticEmitter& emitter, const std::vector<hir::HirError>& errors) {
    for (const auto& error : errors) {
        emit_hir_error(emitter, error);
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
    case borrow::BorrowErrorCode::TempDroppedWhileBorrowed:
        diag.code = "B028";
        break;
    case borrow::BorrowErrorCode::CannotMoveFromRef:
        diag.code = "B029";
        break;
    case borrow::BorrowErrorCode::BorrowBeyondScope:
        diag.code = "B030";
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
