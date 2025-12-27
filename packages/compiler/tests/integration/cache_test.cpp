// Integration test for build cache system
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class CacheIntegrationTest : public ::testing::Test {
protected:
    fs::path test_dir;
    fs::path tml_exe;
    fs::path test_file;

    void SetUp() override {
        // Use a temporary directory for test files
        test_dir = fs::temp_directory_path() / "tml_cache_test";
        fs::create_directories(test_dir);

        // Find tml executable (platform-specific name)
#ifdef _WIN32
        const char* exe_name = "tml.exe";
#else
        const char* exe_name = "tml";
#endif
        tml_exe = fs::current_path() / "build" / "debug" / exe_name;
        if (!fs::exists(tml_exe)) {
            // Try without build/debug prefix (if run from build/debug/)
            tml_exe = fs::current_path() / exe_name;
        }
        ASSERT_TRUE(fs::exists(tml_exe)) << "tml executable not found at: " << tml_exe;

        // Create a simple TML test file
        test_file = test_dir / "test_cache.tml";
        create_test_file();
    }

    void TearDown() override {
        // Clean up test directory
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    void create_test_file() {
        std::ofstream out(test_file);
        out << R"(
func main() -> I32 {
    println("Cache test")
    let x: I32 = 42
    println("Result: {x}")
    return 0
}
)";
        out.close();
    }

    void modify_test_file() {
        std::ofstream out(test_file);
        out << R"(
func main() -> I32 {
    println("Cache test - MODIFIED")
    let x: I32 = 100
    println("Result: {x}")
    return 0
}
)";
        out.close();
    }

    std::pair<bool, double> run_tml_command(const std::string& cmd) {
        auto start = std::chrono::high_resolution_clock::now();

        std::string full_cmd = "\"" + tml_exe.string() + "\" " + cmd + " 2>&1";
        int result = std::system(full_cmd.c_str());

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        return {result == 0, elapsed.count()};
    }
};

// Test: First build creates cache
TEST_F(CacheIntegrationTest, FirstBuildCreatesCache) {
    auto [success, time1] = run_tml_command("run " + test_file.string());

    EXPECT_TRUE(success) << "First build should succeed";
    EXPECT_GT(time1, 0.0) << "Build should take measurable time";
}

// Test: Second build uses cache (should be faster)
TEST_F(CacheIntegrationTest, SecondBuildUsesCache) {
    // First build
    auto [success1, time1] = run_tml_command("run " + test_file.string());
    ASSERT_TRUE(success1) << "First build should succeed";

    // Second build (should hit cache)
    auto [success2, time2] = run_tml_command("run " + test_file.string());
    EXPECT_TRUE(success2) << "Second build should succeed";

    // Cache hit should be fast
    // Note: When first build is already very fast, percentage comparisons are unreliable
    // So we just verify the cached build is also fast
    EXPECT_LT(time2, 0.2) << "Cached build should be very fast (<200ms)";
}

// Test: Modified file causes cache miss
TEST_F(CacheIntegrationTest, ModifiedFileCausesCacheMiss) {
    // First build
    auto [success1, time1] = run_tml_command("run " + test_file.string());
    ASSERT_TRUE(success1) << "First build should succeed";

    // Modify the file
    modify_test_file();

    // Build again (should miss cache)
    auto [success2, time2] = run_tml_command("run " + test_file.string());
    EXPECT_TRUE(success2) << "Build after modification should succeed";

    // Cache miss should take similar time to first build
    // Allow some variance (within 2x)
    EXPECT_GT(time2, time1 * 0.5) << "Cache miss should take reasonable compilation time";
}

// Test: Multiple runs of unchanged file all hit cache
TEST_F(CacheIntegrationTest, MultipleRunsHitCache) {
    // First build
    auto [success1, time1] = run_tml_command("run " + test_file.string());
    ASSERT_TRUE(success1) << "First build should succeed";

    // Run 5 more times - all should hit cache
    for (int i = 0; i < 5; i++) {
        auto [success, time] = run_tml_command("run " + test_file.string());
        EXPECT_TRUE(success) << "Run " << (i + 2) << " should succeed";
        EXPECT_LT(time, 0.2) << "Run " << (i + 2) << " should be fast (cache hit)";
    }
}
