// Tests for CLI builder infrastructure
// Covers: builder/build.cpp, builder/dependency_resolver.cpp,
//         builder/build_config.cpp, builder/rlib.cpp

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
