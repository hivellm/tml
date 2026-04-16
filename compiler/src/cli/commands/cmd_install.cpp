TML_MODULE("compiler")

//! # Install Command
//!
//! `tml install` — installs native dependencies for all packages in the project.
//! Reads each package's `package.toml` for `[native-deps]` sections and copies
//! platform-appropriate native files to the user cache (`~/.tml/native-cache/`).

#include "cli/commands/cmd_install.hpp"

#include "cli/builder/build_config.hpp"
#include "cli/builder/dependency_resolver.hpp"
#include "cli/builder/native_lib_resolver.hpp"
#include "log/log.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

/// Write a manifest.toml to a native library cache directory.
/// Records the library name, version, platform, and installation timestamp.
/// Does not overwrite existing manifests (preserves original install info).
static void write_cache_manifest(const fs::path& cache_dir, const std::string& lib_name,
                                 const std::string& version, const std::string& platform) {
    auto manifest = cache_dir / "manifest.toml";
    if (fs::exists(manifest))
        return; // Don't overwrite existing manifest

    std::ofstream ofs(manifest);
    if (!ofs.is_open())
        return;

    ofs << "[native-lib]\n";
    ofs << "name = \"" << lib_name << "\"\n";
    ofs << "version = \"" << version << "\"\n";
    ofs << "platform = \"" << platform << "\"\n";

    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    ofs << "installed = \"" << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S") << "\"\n";
}

/// Resolve TML dependencies declared in tml.toml and clone/build them into
/// the user cache (~/.tml/cache). Writes tml.lock on success so subsequent
/// builds can reproduce the exact dependency graph.
///
/// Follows the same flow the build system uses internally, but exposes it
/// as an explicit `tml install` step so CI / package scripts can prepare
/// the dependency graph before any compilation happens.
static int install_tml_deps(bool verbose) {
    fs::path manifest_path = fs::current_path() / "tml.toml";
    if (!fs::exists(manifest_path)) {
        // No tml.toml is perfectly fine for a lib/ scan scenario — just
        // report no TML deps and let the native-deps pass continue.
        if (verbose) {
            std::cout << "No tml.toml in current directory — skipping TML dependency resolution.\n";
        }
        return 0;
    }

    auto manifest = Manifest::load(manifest_path);
    if (!manifest) {
        TML_LOG_ERROR("install", "Could not parse tml.toml");
        return 1;
    }
    if (manifest->dependencies.empty()) {
        if (verbose) {
            std::cout << "No [dependencies] in tml.toml — skipping.\n";
        }
        return 0;
    }

    DependencyResolverOptions opts;
    opts.verbose = verbose;
    opts.cache_dir = get_default_cache_dir();
    DependencyResolver resolver(opts);

    std::cout << "Resolving " << manifest->dependencies.size() << " TML dependencies...\n";
    auto result = resolver.resolve(*manifest, fs::current_path());
    if (!result.success) {
        TML_LOG_ERROR("install", result.error_message);
        return 1;
    }

    // Write tml.lock so reproducible builds can replay the exact graph.
    Lockfile lock;
    lock.version = "1";
    for (const auto& r : result.resolved) {
        LockfileEntry entry;
        entry.name = r.name;
        entry.version = r.version;
        if (r.is_path_dependency) {
            entry.source = "path";
            entry.source_detail = r.source_path.string();
        } else {
            // Cached git/registry deps — record the cache path so downstream
            // tooling can locate the sources without re-resolving.
            entry.source = "cache";
            entry.source_detail = r.source_path.string();
        }
        entry.dependencies = r.dependencies;
        lock.packages.push_back(entry);
    }
    fs::path lock_path = fs::current_path() / "tml.lock";
    if (!lock.save(lock_path)) {
        TML_LOG_WARN("install", "Could not write tml.lock (continuing anyway)");
    } else if (verbose) {
        std::cout << "Wrote tml.lock with " << lock.packages.size() << " entries.\n";
    }

    for (const auto& r : result.resolved) {
        std::cout << "  " << r.name << " v" << r.version
                  << (r.is_path_dependency ? " (path)" : "") << "\n";
    }
    std::cout << "Installed " << result.resolved.size() << " TML dependencies.\n";
    return 0;
}

int run_install(int argc, char* argv[]) {
    bool verbose = false;
    bool native_only = false;
    bool deps_only = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--native-only") {
            native_only = true;
        } else if (arg == "--deps-only") {
            deps_only = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: tml install [options]\n"
                      << "\nInstall TML + native dependencies for the current project.\n"
                      << "\nOptions:\n"
                      << "  --deps-only    Only resolve TML deps (skip native libs)\n"
                      << "  --native-only  Only install native libs (skip TML deps)\n"
                      << "  --verbose, -v  Show detailed output\n";
            return 0;
        }
    }

    // TML dependency resolution (path/git/version). Runs first because the
    // native-deps pass scans lib/ for local packages, which may themselves
    // be dependencies just cloned into the cache.
    if (!native_only) {
        int rc = install_tml_deps(verbose);
        if (rc != 0) {
            return rc;
        }
        if (deps_only) {
            return 0;
        }
    }

    auto platform = Platform::detect();
    auto platform_name = platform.native_dir_name();
    auto cache_dir = NativeLibResolver::get_native_cache_dir();

    std::cout << "Installing native dependencies for platform: " << platform_name << "\n";
    if (verbose) {
        std::cout << "Cache directory: " << cache_dir.string() << "\n";
    }

    // Scan lib/ for packages with package.toml
    fs::path lib_dir = fs::current_path() / "lib";
    if (!fs::exists(lib_dir)) {
        std::cout << "No lib/ directory found — nothing to install.\n";
        return 0;
    }

    int packages_found = 0;
    int libs_installed = 0;

    std::error_code ec;
    for (const auto& pkg_entry : fs::directory_iterator(lib_dir, ec)) {
        if (!pkg_entry.is_directory()) {
            continue;
        }

        fs::path pkg_toml = pkg_entry.path() / "package.toml";
        if (!fs::exists(pkg_toml)) {
            continue;
        }

        std::string pkg_name = pkg_entry.path().filename().string();

        // Load package.toml
        auto manifest = Manifest::load(pkg_toml);
        if (!manifest) {
            if (verbose) {
                // Try to read raw content for debugging
                std::ifstream dbg(pkg_toml);
                std::string content((std::istreambuf_iterator<char>(dbg)),
                                    std::istreambuf_iterator<char>());
                std::cout << "  " << pkg_name << ": failed to parse package.toml ("
                          << content.size() << " bytes)\n";
            }
            continue;
        }
        if (manifest->native_deps.empty()) {
            if (verbose) {
                std::cout << "  " << pkg_name << ": no native-deps (parsed OK, "
                          << manifest->package.name << " v" << manifest->package.version << ")\n";
            }
            continue;
        }

        packages_found++;

        for (const auto& [dep_name, dep] : manifest->native_deps) {
            // Find platform-specific config
            auto it = dep.platforms.find(platform_name);
            if (it == dep.platforms.end()) {
                if (verbose) {
                    std::cout << "  " << pkg_name << "/" << dep_name << ": no config for "
                              << platform_name << "\n";
                }
                continue;
            }

            const auto& plat = it->second;
            if (plat.source.empty()) {
                if (verbose) {
                    std::cout << "  " << pkg_name << "/" << dep_name << ": no source path\n";
                }
                continue;
            }

            // Source directory
            fs::path source_dir = pkg_entry.path() / plat.source;
            if (!fs::exists(source_dir)) {
                std::cerr << "  WARNING: " << pkg_name << "/" << dep_name
                          << ": source dir not found: " << source_dir.string() << "\n";
                continue;
            }

            // Destination: ~/.tml/native-cache/<dep_name>/<version>/<platform>/
            std::string version = dep.version.empty() ? "0.0.0" : dep.version;
            fs::path dest_dir = cache_dir / dep_name / version / platform_name;
            fs::create_directories(dest_dir, ec);
            if (ec) {
                std::cerr << "  ERROR: cannot create " << dest_dir.string() << ": " << ec.message()
                          << "\n";
                continue;
            }

            // Copy runtime files
            int copied = 0;
            for (const auto& file : plat.runtime) {
                fs::path src = source_dir / file;
                fs::path dst = dest_dir / file;
                if (!fs::exists(src)) {
                    std::cerr << "  WARNING: " << file << " not found in " << source_dir.string()
                              << "\n";
                    continue;
                }
                // Skip if already exists with same size
                if (fs::exists(dst)) {
                    auto src_sz = fs::file_size(src, ec);
                    auto dst_sz = fs::file_size(dst, ec);
                    if (src_sz == dst_sz) {
                        if (verbose) {
                            std::cout << "  " << file << " (up to date)\n";
                        }
                        continue;
                    }
                }
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
                if (!ec) {
                    copied++;
                    if (verbose) {
                        std::cout << "  " << file << " -> " << dest_dir.string() << "\n";
                    }
                }
            }

            // Copy link files
            for (const auto& file : plat.link) {
                fs::path src = source_dir / file;
                fs::path dst = dest_dir / file;
                if (!fs::exists(src)) {
                    continue;
                }
                if (fs::exists(dst)) {
                    auto src_sz = fs::file_size(src, ec);
                    auto dst_sz = fs::file_size(dst, ec);
                    if (src_sz == dst_sz) {
                        continue;
                    }
                }
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
                if (!ec) {
                    copied++;
                    if (verbose) {
                        std::cout << "  " << file << " -> " << dest_dir.string() << "\n";
                    }
                }
            }

            // Write manifest.toml to cache dir for tracking
            write_cache_manifest(dest_dir, dep_name, version, platform_name);

            libs_installed++;
            std::cout << "  " << pkg_name << "/" << dep_name << " v" << version << " ("
                      << platform_name << ")"
                      << " — " << copied << " files installed\n";
        }
    }

    if (packages_found == 0) {
        std::cout << "No packages with native-deps found.\n";
    } else {
        std::cout << "\nInstalled " << libs_installed << " native lib(s) from " << packages_found
                  << " package(s) to " << cache_dir.string() << "\n";
    }

    return 0;
}

} // namespace tml::cli
