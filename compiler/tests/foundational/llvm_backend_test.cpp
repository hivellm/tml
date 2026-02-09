// LLVM Backend tests
//
// Tests for the embedded LLVM compilation pipeline.

#include "backend/llvm_backend.hpp"

#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;
using namespace tml::backend;

class LLVMBackendTest : public ::testing::Test {
protected:
    fs::path temp_dir_;

    void SetUp() override {
        temp_dir_ = fs::temp_directory_path() / "tml_llvm_backend_test";
        fs::create_directories(temp_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(temp_dir_, ec);
    }

    static std::string minimal_ir() {
        return R"(
target triple = "x86_64-pc-windows-msvc"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"

define i32 @main() {
entry:
    ret i32 0
}
)";
    }
};

// ============================================================================
// Availability
// ============================================================================

TEST_F(LLVMBackendTest, IsAvailable) {
    EXPECT_TRUE(is_llvm_backend_available());
}

TEST_F(LLVMBackendTest, VersionNonEmpty) {
    auto version = get_llvm_version();
    EXPECT_FALSE(version.empty());
}

// ============================================================================
// Initialization
// ============================================================================

TEST_F(LLVMBackendTest, InitializeSucceeds) {
    LLVMBackend backend;
    EXPECT_FALSE(backend.is_initialized());
    EXPECT_TRUE(backend.initialize());
    EXPECT_TRUE(backend.is_initialized());
}

// ============================================================================
// Target triple
// ============================================================================

TEST_F(LLVMBackendTest, DefaultTargetTripleNonEmpty) {
    LLVMBackend backend;
    ASSERT_TRUE(backend.initialize());
    auto triple = backend.get_default_target_triple();
    EXPECT_FALSE(triple.empty());
}

// ============================================================================
// Compile IR to object
// ============================================================================

TEST_F(LLVMBackendTest, CompileValidIr) {
    LLVMBackend backend;
    ASSERT_TRUE(backend.initialize());

    auto output = temp_dir_ / "test.obj";
    LLVMCompileOptions opts;

    auto result = backend.compile_ir_to_object(minimal_ir(), output, opts);
    EXPECT_TRUE(result.success) << result.error_message;
    EXPECT_TRUE(fs::exists(result.object_file));
    EXPECT_GT(fs::file_size(result.object_file), 0u);
}

TEST_F(LLVMBackendTest, CompileInvalidIr) {
    LLVMBackend backend;
    ASSERT_TRUE(backend.initialize());

    auto output = temp_dir_ / "bad.obj";
    LLVMCompileOptions opts;

    auto result = backend.compile_ir_to_object("this is not valid LLVM IR", output, opts);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

TEST_F(LLVMBackendTest, CompileWithOptimization) {
    LLVMBackend backend;
    ASSERT_TRUE(backend.initialize());

    auto output = temp_dir_ / "opt.obj";
    LLVMCompileOptions opts;
    opts.optimization_level = 2;

    auto result = backend.compile_ir_to_object(minimal_ir(), output, opts);
    EXPECT_TRUE(result.success) << result.error_message;
    EXPECT_TRUE(fs::exists(result.object_file));
}
