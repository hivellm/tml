TML_MODULE("compiler")

//! # Runtime Object Collection
//!
//! Contains `get_runtime_objects()` — collects all C runtime object files and
//! pre-built libraries that must be linked into a compiled TML executable.
//!
//! This includes:
//! - Core C runtime (essential.c, mem.c, net.c, …)
//! - Module-specific runtimes (core::mem, core::time, test, …)
//! - Pre-built library dependencies (JSON, zlib, profiler, search, sqlite3)

#include "builder_internal.hpp"

namespace tml::cli::build {

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
    std::string runtime_lib = find_runtime_library();
    bool use_precompiled =
        !runtime_lib.empty() && !CompilerOptions::check_leaks && !CompilerOptions::coverage;

    if (use_precompiled) {
        // Use pre-compiled runtime library (no clang needed)
        objects.push_back(fs::path(runtime_lib));
        TML_LOG_INFO("build", "[runtime] Using pre-compiled runtime: " << runtime_lib);
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

            // diagnostics/ - console.c (global state for std::console timers/counters)
            fs::path console_c = runtime_dir / "diagnostics" / "console.c";
            if (fs::exists(console_c)) {
                std::string console_obj = ensure_c_compiled(to_forward_slashes(console_c.string()),
                                                            deps_cache, clang, verbose);
                objects.push_back(fs::path(console_obj));
                TML_LOG_DEBUG("build", "Including console runtime: " << console_obj);
            }

            // diagnostics/ - inspector.c (Chrome DevTools Protocol inspector)
            fs::path inspector_c = runtime_dir / "diagnostics" / "inspector.c";
            if (fs::exists(inspector_c)) {
                std::string inspector_obj = ensure_c_compiled(
                    to_forward_slashes(inspector_c.string()), deps_cache, clang, verbose);
                objects.push_back(fs::path(inspector_obj));
                TML_LOG_DEBUG("build", "Including inspector runtime: " << inspector_obj);
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

            // core/ - essential_cpuid.c (CPUID/XGETBV detection)
            fs::path cpuid_c = runtime_dir / "core" / "essential_cpuid.c";
            if (fs::exists(cpuid_c)) {
                std::string cpuid_obj = ensure_c_compiled(to_forward_slashes(cpuid_c.string()),
                                                          deps_cache, clang, verbose);
                objects.push_back(fs::path(cpuid_obj));
                TML_LOG_DEBUG("build", "Including cpuid runtime: " << cpuid_obj);
            }

            // core/ - essential_tracy.c (Tracy profiler FFI bindings)
            fs::path tracy_c = runtime_dir / "core" / "essential_tracy.c";
            if (fs::exists(tracy_c)) {
                std::string tracy_obj = ensure_c_compiled(to_forward_slashes(tracy_c.string()),
                                                          deps_cache, clang, verbose);
                objects.push_back(fs::path(tracy_obj));
                TML_LOG_DEBUG("build", "Including tracy runtime: " << tracy_obj);
            }

            // core/ - essential_utf8.c (UTF-8 byte-to-string encoding)
            fs::path utf8_c = runtime_dir / "core" / "essential_utf8.c";
            if (fs::exists(utf8_c)) {
                std::string utf8_obj = ensure_c_compiled(to_forward_slashes(utf8_c.string()),
                                                         deps_cache, clang, verbose);
                objects.push_back(fs::path(utf8_obj));
                TML_LOG_DEBUG("build", "Including utf8 runtime: " << utf8_obj);
            }

            // core/ - essential_random.c (random seed generation)
            fs::path random_c = runtime_dir / "core" / "essential_random.c";
            if (fs::exists(random_c)) {
                std::string random_obj = ensure_c_compiled(to_forward_slashes(random_c.string()),
                                                           deps_cache, clang, verbose);
                objects.push_back(fs::path(random_obj));
                TML_LOG_DEBUG("build", "Including random runtime: " << random_obj);
            }

            // core/ - essential_ffi.c (FFI utility functions: tml_str_from_cstr, tml_free)
            fs::path ffi_c = runtime_dir / "core" / "essential_ffi.c";
            if (fs::exists(ffi_c)) {
                std::string ffi_obj = ensure_c_compiled(to_forward_slashes(ffi_c.string()),
                                                        deps_cache, clang, verbose);
                objects.push_back(fs::path(ffi_obj));
                TML_LOG_DEBUG("build", "Including ffi runtime: " << ffi_obj);
            }

            // memory/ - pool.c (memory pool allocator)
            fs::path pool_c = runtime_dir / "memory" / "pool.c";
            if (fs::exists(pool_c)) {
                std::string pool_obj = ensure_c_compiled(to_forward_slashes(pool_c.string()),
                                                         deps_cache, clang, verbose);
                objects.push_back(fs::path(pool_obj));
                TML_LOG_DEBUG("build", "Including pool runtime: " << pool_obj);
            }

            // memory/ - str_free.c (safe string deallocation with PE image range check)
            fs::path str_free_c = runtime_dir / "memory" / "str_free.c";
            if (fs::exists(str_free_c)) {
                std::string str_free_obj = ensure_c_compiled(
                    to_forward_slashes(str_free_c.string()), deps_cache, clang, verbose);
                objects.push_back(fs::path(str_free_obj));
                TML_LOG_DEBUG("build", "Including str_free runtime: " << str_free_obj);
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

            // net/ - poll.c (I/O event polling: epoll/WSAPoll)
            fs::path poll_c = runtime_dir / "net" / "poll.c";
            if (fs::exists(poll_c)) {
                std::string poll_obj = ensure_c_compiled(to_forward_slashes(poll_c.string()),
                                                         deps_cache, clang, verbose);
                objects.push_back(fs::path(poll_obj));
                TML_LOG_DEBUG("build", "Including poll runtime: " << poll_obj);
            }

            // net/ - iocp.c (IOCP async I/O: Windows only, stubs on other platforms)
            fs::path iocp_c = runtime_dir / "net" / "iocp.c";
            if (fs::exists(iocp_c)) {
                std::string iocp_obj = ensure_c_compiled(to_forward_slashes(iocp_c.string()),
                                                         deps_cache, clang, verbose);
                objects.push_back(fs::path(iocp_obj));
                TML_LOG_DEBUG("build", "Including iocp runtime: " << iocp_obj);
            }

            // collections/ - collections.c (buffer FFI for crypto/zlib)
            fs::path collections_c = runtime_dir / "collections" / "collections.c";
            if (fs::exists(collections_c)) {
                std::string collections_obj = ensure_c_compiled(
                    to_forward_slashes(collections_c.string()), deps_cache, clang, verbose);
                objects.push_back(fs::path(collections_obj));
                TML_LOG_DEBUG("build", "Including collections runtime: " << collections_obj);
            }

            // collections/ - buffer_simd.c (SIMD-optimized buffer ops: bswap, SSE2 search)
            fs::path buffer_simd_c = runtime_dir / "collections" / "buffer_simd.c";
            if (fs::exists(buffer_simd_c)) {
                std::string buffer_simd_obj = ensure_c_compiled(
                    to_forward_slashes(buffer_simd_c.string()), deps_cache, clang, verbose);
                objects.push_back(fs::path(buffer_simd_obj));
                TML_LOG_DEBUG("build", "Including buffer_simd runtime: " << buffer_simd_obj);
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

            // crypto/hash.c — pure hash functions (FNV1a, Murmur2, hex)
            // Always included: no external dependencies, needed by std::hash
            fs::path hash_c = runtime_dir / "crypto" / "hash.c";
            if (fs::exists(hash_c)) {
                std::string hash_obj = ensure_c_compiled(to_forward_slashes(hash_c.string()),
                                                         deps_cache, clang, verbose);
                objects.push_back(fs::path(hash_obj));
                TML_LOG_DEBUG("build", "Including hash runtime: " << hash_obj);
            }

            // crypto/*.c + tls.c — OpenSSL-dependent, only when crypto modules are used
            if (has_crypto_modules(registry)) {
                std::string crypto_extra_flags;
                {
                    auto openssl = find_openssl();
                    crypto_extra_flags = "-DTML_HAS_OPENSSL=1";
                    if (openssl.found) {
                        crypto_extra_flags +=
                            " -I\"" + to_forward_slashes(openssl.include_dir.string()) + "\"";
                    }
                }
                for (const auto& crypto_file :
                     {"crypto.c", "crypto_key.c", "crypto_x509.c", "crypto_dh.c", "crypto_ecdh.c",
                      "crypto_kdf.c", "crypto_rsa.c", "crypto_sign.c"}) {
                    fs::path crypto_c = runtime_dir / "crypto" / crypto_file;
                    if (fs::exists(crypto_c)) {
                        std::string crypto_obj =
                            ensure_c_compiled(to_forward_slashes(crypto_c.string()), deps_cache,
                                              clang, verbose, crypto_extra_flags);
                        objects.push_back(fs::path(crypto_obj));
                        TML_LOG_DEBUG("build",
                                      "Including " << crypto_file << " runtime: " << crypto_obj);
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

            // os/os_process.c (subprocess spawn/wait/read)
            fs::path os_process_c = runtime_dir / "os" / "os_process.c";
            if (fs::exists(os_process_c)) {
                std::string os_process_obj = ensure_c_compiled(
                    to_forward_slashes(os_process_c.string()), deps_cache, clang, verbose);
                objects.push_back(fs::path(os_process_obj));
                TML_LOG_DEBUG("build", "Including os_process runtime: " << os_process_obj);
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
    } else {
        // Even without test module, always link coverage runtime
        // (needed for test executables that use coverage instrumentation)
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

    // Link std::file runtime if imported (check submodules too)
    auto uses_file_module = [&registry]() -> bool {
        if (registry->has_module("std::file"))
            return true;
        for (const auto& [path, _] : registry->get_all_modules()) {
            if (path.find("std::file::") == 0)
                return true;
        }
        return false;
    };
    if (uses_file_module()) {
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
    // Also check for @derive(Deserialize) which generates direct FFI calls to tml_json_*
    auto uses_json_module = [&registry, &module]() -> bool {
        if (registry->has_module("std::json"))
            return true;
        for (const auto& [path, _] : registry->get_all_modules()) {
            if (path.find("std::json::") == 0 || path == "std::json") {
                return true;
            }
        }
        // Check if any struct/enum has @derive(Deserialize) — these generate
        // direct calls to tml_json_parse/tml_json_free/etc. without importing std::json
        auto has_derive_deserialize = [](const std::vector<parser::Decorator>& decorators) -> bool {
            for (const auto& deco : decorators) {
                if (deco.name == "derive") {
                    for (const auto& arg : deco.args) {
                        if (arg->is<parser::IdentExpr>() &&
                            arg->as<parser::IdentExpr>().name == "Deserialize") {
                            return true;
                        }
                    }
                }
            }
            return false;
        };
        for (const auto& decl : module.decls) {
            if (decl->is<parser::StructDecl>()) {
                if (has_derive_deserialize(decl->as<parser::StructDecl>().decorators))
                    return true;
            } else if (decl->is<parser::EnumDecl>()) {
                if (has_derive_deserialize(decl->as<parser::EnumDecl>().decorators))
                    return true;
            }
        }
        return false;
    };
    if (uses_json_module()) {
        // Find the JSON runtime library (built alongside tml.exe)
        auto find_json_runtime = []() -> std::optional<fs::path> {
            // Try both .lib (MSVC) and .a (Zig CC/Clang) naming conventions
            std::vector<std::string> lib_names;
#ifdef _WIN32
            lib_names = {"tml_json_runtime.lib", "libtml_json_runtime.a"};
#else
            lib_names = {"libtml_json_runtime.a", "tml_json_runtime.lib"};
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
                for (const auto& ln : lib_names) {
                    fs::path lib_path = fs::path(search_path) / ln;
                    if (fs::exists(lib_path)) {
                        return fs::absolute(lib_path);
                    }
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
            std::vector<std::string> json_core_names;
#ifdef _WIN32
            json_core_names = {"tml_json.lib", "libtml_json.a"};
#else
            json_core_names = {"libtml_json.a", "tml_json.lib"};
#endif
            bool json_core_found = false;
            for (const auto& jn : json_core_names) {
                auto tml_json_lib = json_lib_dir / jn;
                if (fs::exists(tml_json_lib)) {
                    objects.push_back(tml_json_lib);
                    TML_LOG_DEBUG("build",
                                  "Including JSON parser library: " << tml_json_lib.string());
                    json_core_found = true;
                    break;
                }
            }
            if (!json_core_found) {
                TML_LOG_WARN("build", "tml_json library not found in " << json_lib_dir.string());
            }
        } else {
            TML_LOG_WARN("build", "std::json imported but tml_json_runtime library not found");
        }
    }

    // Link std::zlib runtime if the user's source actually imports std::zlib (directly
    // or transitively). We check the source module's use declarations AND the registry's
    // tracked user imports, rather than the full registry which includes preloaded modules.
    auto uses_zlib_module = [&module, &registry]() -> bool {
        // Check 1: Direct user imports from source file
        for (const auto& decl : module.decls) {
            if (!decl->is<parser::UseDecl>())
                continue;
            const auto& use_decl = decl->as<parser::UseDecl>();
            std::string use_path;
            for (size_t i = 0; i < use_decl.path.segments.size(); ++i) {
                if (i > 0)
                    use_path += "::";
                use_path += use_decl.path.segments[i];
            }
            // Direct import of std::zlib or submodule
            if (use_path == "std::zlib" || use_path.find("std::zlib::") == 0)
                return true;
            // Modules that transitively require zlib
            if (use_path == "std::http::encoding" || use_path.find("std::http::encoding::") == 0)
                return true;
            if (use_path == "std::http::compression" ||
                use_path.find("std::http::compression::") == 0)
                return true;
        }
        // Check 2: Walk private_imports of user-imported modules to find transitive zlib deps.
        // This handles cases like: user imports module X, which imports std::zlib internally.
        if (registry) {
            // Collect user's direct module imports
            std::unordered_set<std::string> user_modules;
            for (const auto& decl : module.decls) {
                if (!decl->is<parser::UseDecl>())
                    continue;
                const auto& use_decl = decl->as<parser::UseDecl>();
                std::string use_path;
                for (size_t i = 0; i < use_decl.path.segments.size(); ++i) {
                    if (i > 0)
                        use_path += "::";
                    use_path += use_decl.path.segments[i];
                }
                user_modules.insert(use_path);
                // Also try parent path (for symbol imports like std::zlib::gzip)
                auto sep = use_path.rfind("::");
                if (sep != std::string::npos) {
                    user_modules.insert(use_path.substr(0, sep));
                }
            }
            // Check private_imports of directly imported modules
            for (const auto& mod_path : user_modules) {
                auto mod_opt = registry->get_module(mod_path);
                if (!mod_opt)
                    continue;
                for (const auto& dep : mod_opt->private_imports) {
                    if (dep == "std::zlib" || dep.find("std::zlib::") == 0)
                        return true;
                }
            }
        }
        return false;
    };
    if (uses_zlib_module()) {
        // Find the zlib runtime library (built alongside tml.exe)
        auto find_zlib_runtime = []() -> std::optional<fs::path> {
            // Try both .lib (MSVC) and .a (Zig CC/Clang) naming conventions
            std::vector<std::string> lib_names;
#ifdef _WIN32
            lib_names = {"tml_zlib_runtime.lib", "libtml_zlib_runtime.a"};
#else
            lib_names = {"libtml_zlib_runtime.a", "tml_zlib_runtime.lib"};
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
                for (const auto& ln : lib_names) {
                    fs::path lib_path = fs::path(search_path) / ln;
                    if (fs::exists(lib_path)) {
                        return fs::absolute(lib_path);
                    }
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
            // Try both .lib (MSVC) and .a (Zig CC/Clang) naming conventions
            std::vector<std::string> lib_names;
#ifdef _WIN32
            lib_names = {"tml_profiler.lib", "libtml_profiler.a"};
#else
            lib_names = {"libtml_profiler.a", "tml_profiler.lib"};
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
                for (const auto& ln : lib_names) {
                    fs::path lib_path = fs::path(search_path) / ln;
                    if (fs::exists(lib_path)) {
                        return fs::absolute(lib_path);
                    }
                }
            }
            return std::nullopt;
        };

        if (auto profiler_lib = find_profiler_runtime()) {
            objects.push_back(*profiler_lib);
            TML_LOG_DEBUG("build",
                          "Including profiler runtime library: " << profiler_lib->string());

            // Also link tml_log dependency (profiler uses TML_LOG_* macros)
            std::vector<std::string> log_lib_names;
#ifdef _WIN32
            log_lib_names = {"tml_log.lib", "libtml_log.a"};
#else
            log_lib_names = {"libtml_log.a", "tml_log.lib"};
#endif
            auto profiler_lib_dir = profiler_lib->parent_path();
            // Search: same dir as profiler, then lib/ subdir, then recursively in parent/cache
            bool log_found = false;
            // Check same directory first
            for (const auto& lln : log_lib_names) {
                auto log_same_dir = profiler_lib_dir / lln;
                if (fs::exists(log_same_dir)) {
                    objects.push_back(fs::absolute(log_same_dir));
                    log_found = true;
                    break;
                }
            }
            // Check lib/ subdirectory (CMake outputs to build/debug/lib/)
            if (!log_found) {
                for (const auto& lln : log_lib_names) {
                    auto log_lib_subdir = profiler_lib_dir / "lib" / lln;
                    if (fs::exists(log_lib_subdir)) {
                        objects.push_back(fs::absolute(log_lib_subdir));
                        log_found = true;
                        break;
                    }
                }
            }
            // Search build cache directory (CMake puts libs in cache/*/Debug/)
            if (!log_found) {
                auto cache_base = profiler_lib_dir.parent_path() / "cache";
                if (fs::exists(cache_base)) {
                    try {
                        for (auto& p : fs::recursive_directory_iterator(cache_base)) {
                            auto fname = p.path().filename().string();
                            for (const auto& lln : log_lib_names) {
                                if (fname == lln) {
                                    objects.push_back(fs::absolute(p.path()));
                                    log_found = true;
                                    break;
                                }
                            }
                            if (log_found)
                                break;
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
            // Try both .lib (MSVC) and .a (Zig CC/Clang) naming conventions
            std::vector<std::string> lib_names;
#ifdef _WIN32
            lib_names = {"tml_search_runtime.lib", "libtml_search_runtime.a"};
#else
            lib_names = {"libtml_search_runtime.a", "tml_search_runtime.lib"};
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
                for (const auto& ln : lib_names) {
                    fs::path lib_path = fs::path(search_path) / ln;
                    if (fs::exists(lib_path)) {
                        return fs::absolute(lib_path);
                    }
                }
            }
            return std::nullopt;
        };

        if (auto search_lib = find_search_runtime()) {
            objects.push_back(*search_lib);
            TML_LOG_DEBUG("build", "Including search runtime library: " << search_lib->string());

            // Also link tml_search.lib (search_engine.cpp depends on it)
            auto search_lib_dir = search_lib->parent_path();
            std::vector<std::string> search_core_names;
#ifdef _WIN32
            search_core_names = {"tml_search.lib", "libtml_search.a"};
#else
            search_core_names = {"libtml_search.a", "tml_search.lib"};
#endif
            // Search in cache directory (CMake puts libs there)
            bool core_found = false;
            auto cache_base = search_lib_dir.parent_path() / "cache";
            if (fs::exists(cache_base)) {
                try {
                    for (auto& p : fs::recursive_directory_iterator(cache_base)) {
                        auto fname = p.path().filename().string();
                        for (const auto& scn : search_core_names) {
                            if (fname == scn) {
                                objects.push_back(fs::absolute(p.path()));
                                core_found = true;
                                TML_LOG_DEBUG("build", "Including search core library: "
                                                           << p.path().string());
                                break;
                            }
                        }
                        if (core_found)
                            break;
                    }
                } catch (...) {}
            }
            if (!core_found) {
                // Check same directory
                for (const auto& scn : search_core_names) {
                    auto same_dir = search_lib_dir / scn;
                    if (fs::exists(same_dir)) {
                        objects.push_back(fs::absolute(same_dir));
                        core_found = true;
                        break;
                    }
                }
            }
            if (!core_found) {
                // Check lib/ subdirectory (CMake outputs to build/debug/lib/)
                for (const auto& scn : search_core_names) {
                    auto lib_dir = search_lib_dir / "lib" / scn;
                    if (fs::exists(lib_dir)) {
                        objects.push_back(fs::absolute(lib_dir));
                        core_found = true;
                        break;
                    }
                }
            }
            if (!core_found) {
                TML_LOG_WARN("build", "tml_search library not found (search runtime dependency)");
            }
        } else {
            TML_LOG_WARN("build", "std::search imported but tml_search_runtime library not found");
        }
    }

    // Link sqlite3 library if std::sqlite is imported (check submodules too)
    auto uses_sqlite_module = [&registry]() -> bool {
        if (!registry)
            return false;
        for (const auto& [path, _] : registry->get_all_modules()) {
            if (path == "std::sqlite" || path.find("std::sqlite::") == 0)
                return true;
        }
        return false;
    };
    if (uses_sqlite_module()) {
        auto sqlite = find_sqlite3();
        if (sqlite.found) {
            objects.push_back(sqlite.lib_path);
            TML_LOG_DEBUG("build", "Including sqlite3 library: " << sqlite.lib_path.string());
        } else {
            TML_LOG_WARN("build", "std::sqlite imported but sqlite3 library not found");
        }
    }

    return objects;
}

} // namespace tml::cli::build
