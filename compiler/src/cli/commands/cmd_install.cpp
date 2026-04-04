TML_MODULE("compiler")

//! # Install Command
//!
//! `tml install` — installs native dependencies for all packages in the project.
//! Reads each package's `package.toml` for `[native-deps]` sections and copies
//! platform-appropriate native files to the user cache (`~/.tml/native-cache/`).

#include "cli/commands/cmd_install.hpp"

#include "cli/builder/build_config.hpp"
#include "cli/builder/native_lib_resolver.hpp"
#include "log/log.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

int run_install(int argc, char* argv[]) {
    bool verbose = false;
    bool native_only = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--native-only") {
            native_only = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: tml install [options]\n"
                      << "\nInstall native dependencies for all packages.\n"
                      << "\nOptions:\n"
                      << "  --native-only  Only install native libs (skip TML deps)\n"
                      << "  --verbose, -v  Show detailed output\n";
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
