// Object compiler unit tests
// Uses relative path to src/cli/ headers since they're not in include/
#include "../../src/cli/object_compiler.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace tml::cli;
namespace fs = std::filesystem;

class ObjectCompilerTest : public ::testing::Test {
protected:
    fs::path test_dir;
    fs::path ll_file;
    fs::path obj_file;

    void SetUp() override {
        // Create temporary test directory
        test_dir = fs::temp_directory_path() / "tml_test_object_compiler";
        fs::create_directories(test_dir);

        // Create a minimal LLVM IR file
        ll_file = test_dir / "test.ll";
        std::ofstream ll(ll_file);
        ll << R"(; ModuleID = 'test'
target triple = "x86_64-pc-windows-msvc"

define i32 @test_add(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}

define i32 @main() {
entry:
  %result = call i32 @test_add(i32 5, i32 3)
  ret i32 %result
}
)";
        ll.close();

#ifdef _WIN32
        obj_file = test_dir / "test.obj";
#else
        obj_file = test_dir / "test.o";
#endif
    }

    void TearDown() override {
        // Clean up test files
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }
};

// Test: Object file compilation succeeds
TEST_F(ObjectCompilerTest, CompileSuccess) {
    ObjectCompileOptions opts;
    opts.verbose = false;
    opts.optimization_level = 0; // -O0

    auto result = compile_ll_to_object(ll_file, obj_file, "clang", opts);

    EXPECT_TRUE(result.success) << "Compilation should succeed";
    EXPECT_TRUE(fs::exists(obj_file)) << "Object file should be created";
    EXPECT_GT(fs::file_size(obj_file), 0) << "Object file should not be empty";
}

// Test: Optimization levels
TEST_F(ObjectCompilerTest, OptimizationLevels) {
    ObjectCompileOptions opts;
    opts.verbose = false;

    std::vector<int> opt_levels = {0, 1, 2, 3};

    for (int opt_level : opt_levels) {
        fs::path opt_obj = test_dir / ("test_O" + std::to_string(opt_level) + ".obj");
        opts.optimization_level = opt_level;

        auto result = compile_ll_to_object(ll_file, opt_obj, "clang", opts);

        EXPECT_TRUE(result.success) << "Compilation with -O" << opt_level << " should succeed";
        EXPECT_TRUE(fs::exists(opt_obj)) << "Object file with -O" << opt_level << " should exist";
    }
}

// Test: Invalid LLVM IR file
TEST_F(ObjectCompilerTest, InvalidLLVMIR) {
    fs::path invalid_ll = test_dir / "invalid.ll";
    std::ofstream ll(invalid_ll);
    ll << "This is not valid LLVM IR";
    ll.close();

    ObjectCompileOptions opts;
    auto result = compile_ll_to_object(invalid_ll, obj_file, "clang", opts);

    EXPECT_FALSE(result.success) << "Compilation of invalid IR should fail";
    EXPECT_FALSE(result.error_message.empty()) << "Error message should be present";
}

// Test: Non-existent input file
TEST_F(ObjectCompilerTest, NonExistentInput) {
    fs::path nonexistent = test_dir / "nonexistent.ll";

    ObjectCompileOptions opts;
    auto result = compile_ll_to_object(nonexistent, obj_file, "clang", opts);

    EXPECT_FALSE(result.success) << "Compilation should fail for non-existent file";
}

// Test: Batch compilation with auto thread detection
TEST_F(ObjectCompilerTest, BatchCompilation) {
    // Create multiple LLVM IR files
    std::vector<fs::path> ll_files;
    for (int i = 0; i < 3; ++i) {
        fs::path ll_path = test_dir / ("test_" + std::to_string(i) + ".ll");
        std::ofstream ll(ll_path);
        ll << R"(; ModuleID = 'test)" << i << R"('
target triple = "x86_64-pc-windows-msvc"

define i32 @test_func)"
           << i << R"((i32 %x) {
entry:
  %result = add i32 %x, )"
           << i << R"(
  ret i32 %result
}
)";
        ll.close();
        ll_files.push_back(ll_path);
    }

    ObjectCompileOptions opts;
    opts.verbose = false;

    // Test parallel compilation with auto thread detection (0 = auto)
    auto result = compile_ll_batch(ll_files, "clang", opts, 0);

    EXPECT_TRUE(result.success) << "Batch compilation should succeed";
    EXPECT_EQ(result.object_files.size(), ll_files.size())
        << "Should produce object files for all inputs";

    // Verify all object files exist
    for (const auto& obj : result.object_files) {
        EXPECT_TRUE(fs::exists(obj)) << "Object file " << obj << " should exist";
        EXPECT_GT(fs::file_size(obj), 0) << "Object file should not be empty";
    }
}

// Test: Batch compilation with specified thread count
TEST_F(ObjectCompilerTest, BatchCompilationWithThreads) {
    std::vector<fs::path> ll_files;
    for (int i = 0; i < 2; ++i) {
        fs::path ll_path = test_dir / ("batch_" + std::to_string(i) + ".ll");
        std::ofstream ll(ll_path);
        ll << R"(define i32 @func)" << i << R"((i32 %x) { ret i32 %x })";
        ll.close();
        ll_files.push_back(ll_path);
    }

    ObjectCompileOptions opts;
    auto result = compile_ll_batch(ll_files, "clang", opts, 2);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.object_files.size(), 2);
}
