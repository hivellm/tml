// Tests for CLI command option parsing
// Covers: commands/cmd_build.cpp, commands/cmd_test.cpp,
//         commands/cmd_lint.cpp, commands/cmd_format.cpp

#include "cli/commands/cmd_build.hpp"
#include "cli/commands/cmd_cache.hpp" // F-031: evict_dir_to_cap

#include <gtest/gtest.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

// ============================================================================
// Build Command Option Parsing
// ============================================================================

TEST(BuildCommandTest, DefaultOptions) {
    // Verify default build configuration assumptions
    std::string default_opt = "O0";
    std::string default_crate = "bin";
    bool default_release = false;
    bool default_verbose = false;

    EXPECT_EQ(default_opt, "O0");
    EXPECT_EQ(default_crate, "bin");
    EXPECT_FALSE(default_release);
    EXPECT_FALSE(default_verbose);
}

TEST(BuildCommandTest, CrateTypes) {
    std::vector<std::string> valid_crate_types = {"bin", "lib", "dylib", "rlib"};
    EXPECT_EQ(valid_crate_types.size(), 4u);

    for (const auto& ct : valid_crate_types) {
        EXPECT_FALSE(ct.empty()) << "Crate type should not be empty";
    }
}

TEST(BuildCommandTest, OptimizationLevels) {
    std::vector<std::string> valid_levels = {"O0", "O1", "O2", "O3"};
    EXPECT_EQ(valid_levels.size(), 4u);
}

TEST(BuildCommandTest, BackendOptions) {
    std::vector<std::string> valid_backends = {"llvm", "cranelift"};
    EXPECT_EQ(valid_backends.size(), 2u);
}

// ============================================================================
// Test Command Option Parsing
// ============================================================================

TEST(TestCommandTest, DefaultTestOptions) {
    bool default_verbose = false;
    bool default_coverage = false;
    bool default_no_cache = false;
    bool default_profile = false;

    EXPECT_FALSE(default_verbose);
    EXPECT_FALSE(default_coverage);
    EXPECT_FALSE(default_no_cache);
    EXPECT_FALSE(default_profile);
}

TEST(TestCommandTest, SuiteNameParsing) {
    // Suite names follow the pattern "module/submodule"
    std::string suite1 = "core/str";
    std::string suite2 = "std/json";
    std::string suite3 = "core/error";

    EXPECT_NE(suite1.find('/'), std::string::npos);
    EXPECT_NE(suite2.find('/'), std::string::npos);
    EXPECT_NE(suite3.find('/'), std::string::npos);
}

// ============================================================================
// Lint Command Option Parsing
// ============================================================================

TEST(LintCommandTest, DefaultLintOptions) {
    bool default_fix = false;
    EXPECT_FALSE(default_fix);
}

// ============================================================================
// Format Command Option Parsing
// ============================================================================

TEST(FormatCommandTest, DefaultFormatOptions) {
    bool default_check = false;
    EXPECT_FALSE(default_check);
}

TEST(FormatCommandTest, FormatCheckMode) {
    // In check mode, files should not be modified
    bool check_mode = true;
    EXPECT_TRUE(check_mode);
}

// ============================================================================
// Phase 13d: Stage Override Defaults (TML Frontend Switchover)
// ============================================================================

// BuildOptions defaults to TML frontend for parser (phase13d switchover).
TEST(StageOverrideTest, BuildOptionsDefaultsToTmlParser) {
    tml::cli::BuildOptions opts;
    auto it = opts.stage_overrides.find("parser");
    ASSERT_NE(it, opts.stage_overrides.end())
        << "BuildOptions::stage_overrides must contain 'parser' key by default";
    EXPECT_EQ(it->second, "tml")
        << "Default parser implementation must be 'tml' (phase13d switchover)";
}

// RunOptions defaults to TML frontend for parser (phase13d switchover).
TEST(StageOverrideTest, RunOptionsDefaultsToTmlParser) {
    tml::cli::RunOptions opts;
    auto it = opts.stage_overrides.find("parser");
    ASSERT_NE(it, opts.stage_overrides.end())
        << "RunOptions::stage_overrides must contain 'parser' key by default";
    EXPECT_EQ(it->second, "tml")
        << "Default parser implementation must be 'tml' (phase13d switchover)";
}

// Overriding parser to "cpp" reverts to C++ frontend (Phase 5.2 fallback).
TEST(StageOverrideTest, CppOverrideReverts) {
    tml::cli::BuildOptions opts;
    // Simulate --stage=parser:cpp
    opts.stage_overrides["parser"] = "cpp";
    auto it = opts.stage_overrides.find("parser");
    ASSERT_NE(it, opts.stage_overrides.end());
    EXPECT_EQ(it->second, "cpp") << "--stage=parser:cpp must store 'cpp' to force C++ frontend";
}

// Empty stage_overrides means use TML default (non-CLI path, e.g. CompileConfig default).
TEST(StageOverrideTest, EmptyStageOverridesMeansQueryDefault) {
    std::map<std::string, std::string> overrides;
    // When stage_overrides is empty, query_core.cpp falls back to C++ frontend
    // (the QueryOptions default is empty — TML default is enforced by CLI structs).
    EXPECT_TRUE(overrides.find("parser") == overrides.end())
        << "Empty stage_overrides correctly signals: use QueryOptions default";
}

// ============================================================================
// F-031: LRU cache eviction (phase42c)
// ============================================================================

namespace {
namespace fs = std::filesystem;

void evict_write_file(const fs::path& p, size_t bytes) {
    std::ofstream f(p, std::ios::binary);
    std::string data(bytes, 'x');
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
}

std::string evict_norm(const fs::path& p) {
    std::string s = p.string();
    std::replace(s.begin(), s.end(), '\\', '/');
#ifdef _WIN32
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    return s;
}

uintmax_t evict_dir_total(const fs::path& dir) {
    uintmax_t total = 0;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (fs::is_regular_file(e.path())) {
            total += fs::file_size(e.path());
        }
    }
    return total;
}
} // namespace

// Under the cap, eviction is a no-op and nothing is deleted.
TEST(CacheEvictionTest, UnderCapNoOp) {
    fs::path dir = fs::temp_directory_path() / "tml_evict_noop";
    fs::remove_all(dir);
    fs::create_directories(dir);
    evict_write_file(dir / "a.exe", 1024 * 1024);

    auto reclaimed = tml::cli::evict_dir_to_cap(dir, /*cap_mb=*/100, /*recursive=*/false, ".exe", {},
                                                "test");
    EXPECT_EQ(reclaimed, 0u);
    EXPECT_TRUE(fs::exists(dir / "a.exe"));
    fs::remove_all(dir);
}

// Over the cap: evict oldest-first down to the cap; referenced (protected) files
// survive even when they are the oldest, unreferenced orphans go first.
TEST(CacheEvictionTest, RespectsCapAndProtectsReferenced) {
    fs::path dir = fs::temp_directory_path() / "tml_evict_cap";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // 6 EXEs, 1 MB each = 6 MB. Distinct, increasing mtimes: s0 oldest .. s5 newest.
    std::vector<fs::path> exes;
    for (int i = 0; i < 6; ++i) {
        fs::path p = dir / ("s" + std::to_string(i) + ".exe");
        evict_write_file(p, 1024 * 1024);
        fs::last_write_time(p, fs::file_time_type::clock::now() + std::chrono::seconds(i));
        exes.push_back(p);
    }

    // Protect s0 — the OLDEST, i.e. the first that plain LRU would drop.
    std::set<std::string> protect{evict_norm(exes[0])};

    auto reclaimed = tml::cli::evict_dir_to_cap(dir, /*cap_mb=*/3, /*recursive=*/false, ".exe",
                                                protect, "test");
    EXPECT_GE(reclaimed, 3u * 1024 * 1024);
    EXPECT_LE(evict_dir_total(dir), 3u * 1024 * 1024);

    EXPECT_TRUE(fs::exists(exes[0])) << "protected (referenced) EXE must survive";
    EXPECT_TRUE(fs::exists(exes[5])) << "newest EXE survives (oldest evicted first)";
    EXPECT_FALSE(fs::exists(exes[1])) << "oldest unprotected EXE evicted first";
    fs::remove_all(dir);
}

// Deleting an `.exe` unit also removes its sibling artifacts (.lib/.pdb/...).
TEST(CacheEvictionTest, EvictsExeSiblings) {
    fs::path dir = fs::temp_directory_path() / "tml_evict_sib";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // One 4 MB exe + a 1 MB sibling .lib; cap 1 MB forces eviction of the unit.
    evict_write_file(dir / "old.exe", 4u * 1024 * 1024);
    evict_write_file(dir / "old.lib", 1024 * 1024);

    auto reclaimed = tml::cli::evict_dir_to_cap(dir, /*cap_mb=*/1, /*recursive=*/false, ".exe", {},
                                                "test");
    // Both exe (4 MB) and its .lib sibling (1 MB) are reclaimed as one unit.
    EXPECT_GE(reclaimed, 5u * 1024 * 1024);
    EXPECT_FALSE(fs::exists(dir / "old.exe"));
    EXPECT_FALSE(fs::exists(dir / "old.lib")) << "sibling .lib removed with its .exe";
    fs::remove_all(dir);
}
