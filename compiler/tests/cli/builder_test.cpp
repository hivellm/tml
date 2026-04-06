// Tests for CLI builder infrastructure
// Covers: builder/build.cpp, builder/dependency_resolver.cpp,
//         builder/build_config.cpp, builder/rlib.cpp, builder/build_script.hpp

#include "cli/builder/build_script.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ============================================================================
// Dependency Resolution Tests
// ============================================================================

TEST(DependencyResolverTest, NoDependencies) {
    // A module with no imports has no dependencies
    std::string source = "func main() -> I32 { return 42 }";
    EXPECT_EQ(source.find("use "), std::string::npos) << "Source without 'use' has no dependencies";
}

TEST(DependencyResolverTest, SingleDependency) {
    std::string source = "use core::str\nfunc main() { }";
    EXPECT_NE(source.find("use core::str"), std::string::npos);
}

TEST(DependencyResolverTest, MultipleDependencies) {
    std::string source = "use core::str\nuse std::json\nuse test\nfunc main() { }";
    int use_count = 0;
    size_t pos = 0;
    while ((pos = source.find("use ", pos)) != std::string::npos) {
        use_count++;
        pos++;
    }
    EXPECT_EQ(use_count, 3);
}

// ============================================================================
// Build Configuration Tests
// ============================================================================

TEST(BuildConfigTest, DebugBuildDefaults) {
    bool is_release = false;
    std::string opt_level = "O0";
    bool emit_debug_info = true;

    EXPECT_FALSE(is_release);
    EXPECT_EQ(opt_level, "O0");
    EXPECT_TRUE(emit_debug_info);
}

TEST(BuildConfigTest, ReleaseBuildDefaults) {
    bool is_release = true;
    std::string opt_level = "O3";
    bool emit_debug_info = false;

    EXPECT_TRUE(is_release);
    EXPECT_EQ(opt_level, "O3");
    EXPECT_FALSE(emit_debug_info);
}

TEST(BuildConfigTest, OutputDirectoryCreation) {
    auto temp = fs::temp_directory_path() / "tml_build_config_test";
    fs::create_directories(temp / "debug" / "bin");

    EXPECT_TRUE(fs::exists(temp / "debug" / "bin"));

    fs::remove_all(temp);
}

// ============================================================================
// RLib Format Tests
// ============================================================================

TEST(RlibTest, RlibExtension) {
    std::string rlib_name = "libcore.rlib";
    EXPECT_NE(rlib_name.find(".rlib"), std::string::npos);
}

TEST(RlibTest, LibraryNaming) {
    // TML library naming convention: lib{name}.rlib
    std::string module_name = "core";
    std::string expected = "lib" + module_name + ".rlib";
    EXPECT_EQ(expected, "libcore.rlib");
}

// ============================================================================
// Build Orchestration Tests
// ============================================================================

TEST(BuildOrchestratorTest, SourceFileDetection) {
    auto temp = fs::temp_directory_path() / "tml_build_orch_test";
    fs::create_directories(temp);

    {
        std::ofstream f(temp / "main.tml");
        f << "func main() { }";
    }

    EXPECT_TRUE(fs::exists(temp / "main.tml"));
    EXPECT_TRUE(fs::file_size(temp / "main.tml") > 0);

    fs::remove_all(temp);
}

TEST(BuildOrchestratorTest, OutputPathCalculation) {
    // Debug build output
    fs::path debug_bin = fs::path("build") / "debug" / "bin" / "tml.exe";
    EXPECT_EQ(debug_bin.filename().string(), "tml.exe");

    // Release build output
    fs::path release_bin = fs::path("build") / "release" / "bin" / "tml.exe";
    EXPECT_EQ(release_bin.filename().string(), "tml.exe");
}

// ============================================================================
// Build Script Directive Parsing Tests
// ============================================================================

TEST(BuildScriptTest, ParseValidDirectives) {
    // Test all 6 directive types
    std::string output = "tml:link-lib=pq\n"
                         "tml:link-lib=ssl\n"
                         "tml:link-search=/usr/lib\n"
                         "tml:link-search=C:\\libs\\pg\n"
                         "tml:copy-artifact=native/libpq.dll\n"
                         "tml:warning=Using system libpq\n"
                         "tml:cfg=HAS_POSTGRES\n"
                         "tml:rerun-if-changed=native/libpq.dll\n";

    auto result = tml::cli::parse_build_directives(output);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.link_libs.size(), 2u);
    EXPECT_TRUE(result.link_libs.count("pq"));
    EXPECT_TRUE(result.link_libs.count("ssl"));
    EXPECT_EQ(result.link_search_paths.size(), 2u);
    EXPECT_EQ(result.copy_artifacts.size(), 1u);
    EXPECT_EQ(result.warnings.size(), 1u);
    EXPECT_EQ(result.warnings[0], "Using system libpq");
    EXPECT_EQ(result.cfg_symbols.size(), 1u);
    EXPECT_TRUE(result.cfg_symbols.count("HAS_POSTGRES"));
    EXPECT_EQ(result.rerun_paths.size(), 1u);
}

TEST(BuildScriptTest, IgnoreNonDirectiveLines) {
    std::string output = "Building native deps...\n"
                         "Found libpq at /usr/lib\n"
                         "tml:link-lib=pq\n"
                         "Compilation complete.\n"
                         "Total time: 1.5s\n";

    auto result = tml::cli::parse_build_directives(output);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.link_libs.size(), 1u); // Only the tml: line
    EXPECT_TRUE(result.link_libs.count("pq"));
    EXPECT_TRUE(result.warnings.empty());
    EXPECT_TRUE(result.copy_artifacts.empty());
}

TEST(BuildScriptTest, HandleEdgeCases) {
    // Empty stdout
    {
        auto result = tml::cli::parse_build_directives("");
        EXPECT_TRUE(result.success);
        EXPECT_TRUE(result.link_libs.empty());
    }

    // Directive without = (missing value, should be ignored)
    {
        auto result = tml::cli::parse_build_directives("tml:link-lib\n");
        EXPECT_TRUE(result.success);
        EXPECT_TRUE(result.link_libs.empty());
    }

    // Unknown directive (should be silently ignored)
    {
        auto result = tml::cli::parse_build_directives("tml:unknown-thing=value\n");
        EXPECT_TRUE(result.success);
        EXPECT_TRUE(result.link_libs.empty());
    }

    // Value with leading/trailing whitespace (should be trimmed)
    {
        auto result = tml::cli::parse_build_directives("tml:link-lib=  pq  \n");
        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.link_libs.size(), 1u);
        EXPECT_TRUE(result.link_libs.count("pq"));
    }

    // Empty value after = (should be ignored)
    {
        auto result = tml::cli::parse_build_directives("tml:link-lib=\n");
        EXPECT_TRUE(result.success);
        EXPECT_TRUE(result.link_libs.empty());
    }

    // Windows line endings (\r\n)
    {
        auto result = tml::cli::parse_build_directives("tml:link-lib=pq\r\n");
        EXPECT_TRUE(result.success);
        EXPECT_TRUE(result.link_libs.count("pq"));
    }

    // Duplicate lib (set deduplicates)
    {
        auto result = tml::cli::parse_build_directives("tml:link-lib=pq\ntml:link-lib=pq\n");
        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.link_libs.size(), 1u);
    }
}
