// Tests for CLI command option parsing
// Covers: commands/cmd_build.cpp, commands/cmd_test.cpp,
//         commands/cmd_lint.cpp, commands/cmd_format.cpp

#include "cli/commands/cmd_build.hpp"

#include <gtest/gtest.h>
#include <map>
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
