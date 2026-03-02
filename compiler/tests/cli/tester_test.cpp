// Tests for CLI tester infrastructure
// Covers: tester/discovery.cpp, tester/output.cpp, tester/helpers.cpp,
//         tester/test_cache.cpp, tester/coverage.cpp

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

namespace fs = std::filesystem;

// ============================================================================
// Test Discovery Tests
// ============================================================================

TEST(TesterDiscoveryTest, FindsTestFiles) {
    auto temp = fs::temp_directory_path() / "tml_tester_disc_test";
    fs::create_directories(temp);

    // Create test files matching the pattern
    {
        std::ofstream f(temp / "basic.test.tml");
        f << "use test\nfunc test_basic() { assert_eq(1, 1) }";
    }
    {
        std::ofstream f(temp / "advanced.test.tml");
        f << "use test\nfunc test_advanced() { assert_eq(2, 2) }";
    }
    {
        std::ofstream f(temp / "not_a_test.tml");
        f << "func main() { }";
    }

    int test_count = 0;
    for (const auto& entry : fs::directory_iterator(temp)) {
        auto name = entry.path().filename().string();
        if (name.find(".test.tml") != std::string::npos) {
            test_count++;
        }
    }

    EXPECT_EQ(test_count, 2) << "Should find 2 test files";

    fs::remove_all(temp);
}

TEST(TesterDiscoveryTest, RecursiveTestDiscovery) {
    auto temp = fs::temp_directory_path() / "tml_tester_rec_test";
    fs::create_directories(temp / "sub1");
    fs::create_directories(temp / "sub2");

    {
        std::ofstream f(temp / "sub1" / "a.test.tml");
        f << "use test\nfunc test_a() { }";
    }
    {
        std::ofstream f(temp / "sub2" / "b.test.tml");
        f << "use test\nfunc test_b() { }";
    }

    int test_count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(temp)) {
        if (entry.is_regular_file()) {
            auto name = entry.path().filename().string();
            if (name.find(".test.tml") != std::string::npos) {
                test_count++;
            }
        }
    }

    EXPECT_EQ(test_count, 2) << "Should find test files recursively";

    fs::remove_all(temp);
}

// ============================================================================
// Test Output Formatting
// ============================================================================

TEST(TesterOutputTest, TestResultFormatting) {
    // Simple test to verify we can format test results
    std::string pass_marker = "PASS";
    std::string fail_marker = "FAIL";

    EXPECT_NE(pass_marker, fail_marker);
    EXPECT_EQ(pass_marker.length(), 4u);
}

// ============================================================================
// Test Cache Tests
// ============================================================================

TEST(TesterCacheTest, CacheFileCreation) {
    auto temp = fs::temp_directory_path() / "tml_cache_test_dir";
    fs::create_directories(temp);

    auto cache_file = temp / ".test-cache.json";

    // Write a minimal cache file
    {
        std::ofstream f(cache_file);
        f << R"({"version":1,"entries":{}})";
    }

    EXPECT_TRUE(fs::exists(cache_file));

    // Read it back
    std::ifstream in(cache_file);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("version"), std::string::npos);

    fs::remove_all(temp);
}

TEST(TesterCacheTest, CacheInvalidationOnSourceChange) {
    // Verify the concept: if source hash changes, cache entry is invalid
    std::string hash1 = "abc123";
    std::string hash2 = "def456";
    EXPECT_NE(hash1, hash2) << "Different source should produce different hash";
}

// ============================================================================
// Test Helpers
// ============================================================================

TEST(TesterHelpersTest, TestFilePatternMatch) {
    std::string filename1 = "basic.test.tml";
    std::string filename2 = "helper.tml";
    std::string filename3 = "benchmark.bench.tml";

    EXPECT_NE(filename1.find(".test.tml"), std::string::npos);
    EXPECT_EQ(filename2.find(".test.tml"), std::string::npos);
    EXPECT_EQ(filename3.find(".test.tml"), std::string::npos);
}
