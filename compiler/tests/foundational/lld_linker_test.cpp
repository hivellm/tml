// LLD Linker tests
//
// Tests for the embedded LLD linker wrapper.

#include "backend/lld_linker.hpp"
#include "backend/llvm_backend.hpp"

#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;
using namespace tml::backend;

class LLDLinkerTest : public ::testing::Test {
protected:
    fs::path temp_dir_;

    void SetUp() override {
        temp_dir_ = fs::temp_directory_path() / "tml_lld_linker_test";
        fs::create_directories(temp_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(temp_dir_, ec);
    }

    // Compile minimal IR to an object file for linking tests
    fs::path compile_test_obj() {
        LLVMBackend backend;
        if (!backend.initialize())
            return {};

        std::string ir = R"(
target triple = "x86_64-pc-windows-msvc"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"

define i32 @main() {
entry:
    ret i32 0
}
)";

        auto obj_path = temp_dir_ / "test.obj";
        LLVMCompileOptions opts;
        auto result = backend.compile_ir_to_object(ir, obj_path, opts);
        if (!result.success)
            return {};
        return result.object_file;
    }
};

// ============================================================================
// Availability
// ============================================================================

TEST_F(LLDLinkerTest, IsAvailable) {
    EXPECT_TRUE(is_lld_available());
}

TEST_F(LLDLinkerTest, VersionNonEmpty) {
    auto version = get_lld_version();
    EXPECT_FALSE(version.empty());
}

// ============================================================================
// Initialization
// ============================================================================

TEST_F(LLDLinkerTest, InitializeSucceeds) {
    LLDLinker linker;
    EXPECT_FALSE(linker.is_initialized());
    EXPECT_TRUE(linker.initialize());
    EXPECT_TRUE(linker.is_initialized());
}

// ============================================================================
// Link valid object
// ============================================================================

TEST_F(LLDLinkerTest, LinkValidObject) {
    auto obj = compile_test_obj();
    if (obj.empty()) {
        GTEST_SKIP() << "Could not compile test object";
    }

    LLDLinker linker;
    ASSERT_TRUE(linker.initialize());

    auto exe_path = temp_dir_ / "test.exe";
    LLDLinkOptions opts;
    opts.output_type = LLDOutputType::Executable;
    opts.subsystem = "console";

    auto result = linker.link({obj}, exe_path, opts);
    EXPECT_TRUE(result.success) << result.error_message;
    EXPECT_TRUE(fs::exists(result.output_file));
}

// ============================================================================
// Link missing object
// ============================================================================

TEST_F(LLDLinkerTest, LinkMissingObjectFails) {
    LLDLinker linker;
    ASSERT_TRUE(linker.initialize());

    auto exe_path = temp_dir_ / "bad.exe";
    LLDLinkOptions opts;

    auto result = linker.link({temp_dir_ / "nonexistent.obj"}, exe_path, opts);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}
