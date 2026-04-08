//! PackageRegistry implementation.
//!
//! Walks up from CWD looking for a `tml.toml` with a `[workspace]` section,
//! then for each member loads `<member>/tml.toml` via the existing
//! `SimpleTomlParser` and records `{name → {root, src, deps}}`.
//!
//! On first call `ensure_loaded()` runs once via `std::call_once`. Failure
//! to find a workspace (no `[workspace]` anywhere up to the filesystem root)
//! is non-fatal — the registry stays empty and callers fall back to whatever
//! default behavior they had. This keeps `tml check file.tml` working from
//! arbitrary directories.

#include "package/package_registry.hpp"

#include "cli/builder/build_config.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace tml::pkg {

PackageRegistry& PackageRegistry::instance() {
    static PackageRegistry reg;
    return reg;
}

void PackageRegistry::reload() {
    std::lock_guard<std::mutex> lock(load_mutex_);
    packages_.clear();
    workspace_root_.clear();
    loaded_ = false;
    load_impl();
    loaded_ = true;
}

void PackageRegistry::ensure_loaded() const {
    std::lock_guard<std::mutex> lock(load_mutex_);
    if (loaded_) {
        return;
    }
    const_cast<PackageRegistry*>(this)->load_impl();
    loaded_ = true;
}

// Find the nearest ancestor of `start` that contains a `tml.toml` with a
// `[workspace]` section. Returns empty path on failure.
static fs::path find_workspace_root(const fs::path& start) {
    std::error_code ec;
    fs::path cur = fs::absolute(start, ec);
    if (ec) {
        return {};
    }

    for (int depth = 0; depth < 16; ++depth) {
        fs::path candidate = cur / "tml.toml";
        if (fs::exists(candidate, ec) && !ec) {
            std::ifstream in(candidate);
            if (in) {
                std::stringstream ss;
                ss << in.rdbuf();
                std::string content = ss.str();
                // Cheap textual check before parsing: must contain [workspace]
                if (content.find("[workspace]") != std::string::npos) {
                    return cur;
                }
            }
        }

        fs::path parent = cur.parent_path();
        if (parent == cur || parent.empty()) {
            break;
        }
        cur = parent;
    }
    return {};
}

void PackageRegistry::load_impl() {
    fs::path ws_root = find_workspace_root(fs::current_path());
    if (ws_root.empty()) {
        return; // no workspace — registry stays empty
    }
    workspace_root_ = ws_root;

    auto root_manifest = cli::Manifest::load(ws_root / "tml.toml");
    if (!root_manifest || !root_manifest->workspace.is_workspace()) {
        return;
    }

    for (const auto& member_rel : root_manifest->workspace.members) {
        fs::path member_dir = ws_root / member_rel;
        std::error_code ec;
        if (!fs::exists(member_dir, ec) || ec) {
            std::cerr << "[package] warning: workspace member directory missing: "
                      << member_dir.string() << "\n";
            continue;
        }

        fs::path member_manifest = member_dir / "tml.toml";
        if (!fs::exists(member_manifest, ec) || ec) {
            std::cerr << "[package] warning: workspace member has no tml.toml: "
                      << member_dir.string() << "\n";
            continue;
        }

        auto mem = cli::Manifest::load(member_manifest);
        if (!mem) {
            std::cerr << "[package] warning: failed to parse " << member_manifest.string() << "\n";
            continue;
        }

        if (mem->package.name.empty()) {
            std::cerr << "[package] warning: " << member_manifest.string()
                      << " has no [package].name\n";
            continue;
        }

        PackageInfo info;
        info.name = mem->package.name;
        info.root_dir = fs::canonical(member_dir, ec);
        if (ec) {
            info.root_dir = member_dir;
        }
        info.src_dir = info.root_dir / "src";

        for (const auto& [dep_name, _dep] : mem->dependencies) {
            info.dependencies.push_back(dep_name);
        }

        packages_.emplace(info.name, std::move(info));
    }
}

std::optional<PackageLookup>
PackageRegistry::lookup_for_module(const std::string& module_path) const {
    ensure_loaded();
    if (packages_.empty()) {
        return std::nullopt;
    }

    // Find `::` separator. If absent, the whole path is the package name.
    auto sep = module_path.find("::");
    std::string head = (sep == std::string::npos) ? module_path : module_path.substr(0, sep);

    auto it = packages_.find(head);
    if (it == packages_.end()) {
        return std::nullopt;
    }

    PackageLookup result;
    result.package = &it->second;
    result.rest = (sep == std::string::npos) ? std::string{} : module_path.substr(sep + 2);
    return result;
}

bool PackageRegistry::is_package_module(const std::string& module_path) const {
    return lookup_for_module(module_path).has_value();
}

const PackageInfo* PackageRegistry::get(const std::string& name) const {
    ensure_loaded();
    auto it = packages_.find(name);
    return (it == packages_.end()) ? nullptr : &it->second;
}

} // namespace tml::pkg
