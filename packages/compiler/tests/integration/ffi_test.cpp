// Integration test for C FFI functionality
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

// Helper to check if ar/llvm-ar is available (needed for static libraries)
static bool has_ar_tool() {
#ifdef _WIN32
    // Windows uses lib.exe or llvm-ar, both typically available with MSVC/LLVM
    return true;
#else
    // Check for llvm-ar or ar
    return std::system("which llvm-ar >/dev/null 2>&1") == 0 ||
           std::system("which ar >/dev/null 2>&1") == 0;
#endif
}

class FFIIntegrationTest : public ::testing::Test {
protected:
    fs::path test_dir;
    fs::path tml_exe;
    fs::path tml_lib_file;
    fs::path c_header_file;
    fs::path c_test_file;

    void SetUp() override {
        // Use a temporary directory for test files
        test_dir = fs::temp_directory_path() / "tml_ffi_test";
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

        tml_lib_file = test_dir / "test_ffi_lib.tml";
        c_header_file = test_dir / "test_ffi_lib.h";
        c_test_file = test_dir / "use_ffi_lib.c";

        create_tml_library();
        create_c_test_program();
    }

    void TearDown() override {
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    void create_tml_library() {
        std::ofstream out(tml_lib_file);
        out << R"(
pub func add_numbers(a: I32, b: I32) -> I32 {
    return a + b
}

pub func multiply_numbers(a: I32, b: I32) -> I32 {
    return a * b
}

pub func get_magic_number() -> I32 {
    return 42
}
)";
        out.close();
    }

    void create_c_test_program() {
        std::ofstream out(c_test_file);
        out << R"(
#include <stdio.h>
#include "test_ffi_lib.h"

int main() {
    int result1 = tml_add_numbers(10, 20);
    int result2 = tml_multiply_numbers(6, 7);
    int result3 = tml_get_magic_number();

    printf("add_numbers(10, 20) = %d\n", result1);
    printf("multiply_numbers(6, 7) = %d\n", result2);
    printf("get_magic_number() = %d\n", result3);

    // Verify results
    if (result1 != 30) {
        fprintf(stderr, "ERROR: Expected 30, got %d\n", result1);
        return 1;
    }
    if (result2 != 42) {
        fprintf(stderr, "ERROR: Expected 42, got %d\n", result2);
        return 1;
    }
    if (result3 != 42) {
        fprintf(stderr, "ERROR: Expected 42, got %d\n", result3);
        return 1;
    }

    printf("All FFI tests passed!\n");
    return 0;
}
)";
        out.close();
    }

    int run_command(const std::string& cmd) {
        return std::system(cmd.c_str());
    }
};

// Test: Build static library with header
TEST_F(FFIIntegrationTest, BuildStaticLibraryWithHeader) {
    if (!has_ar_tool()) {
        GTEST_SKIP() << "Skipping: llvm-ar/ar not available for static library creation";
    }

    std::string cmd = "\"" + tml_exe.string() + "\" build " + tml_lib_file.string() +
                      " --crate-type=lib --emit-header --out-dir=" + test_dir.string();

    int result = run_command(cmd);
    EXPECT_EQ(result, 0) << "Building static library should succeed";

    // Check that header was generated
    EXPECT_TRUE(fs::exists(c_header_file)) << "C header should be generated";

#ifdef _WIN32
    fs::path lib_file = test_dir / "test_ffi_lib.lib";
#else
    fs::path lib_file = test_dir / "libtest_ffi_lib.a";
#endif

    EXPECT_TRUE(fs::exists(lib_file)) << "Static library should be created";
    EXPECT_GT(fs::file_size(lib_file), 0) << "Library should not be empty";
}

// Test: C program can use TML static library
TEST_F(FFIIntegrationTest, CProgramUsesStaticLibrary) {
    if (!has_ar_tool()) {
        GTEST_SKIP() << "Skipping: llvm-ar/ar not available for static library creation";
    }

    // Build TML library
    std::string build_cmd = "\"" + tml_exe.string() + "\" build " + tml_lib_file.string() +
                            " --crate-type=lib --emit-header --out-dir=" + test_dir.string();
    int build_result = run_command(build_cmd);
    ASSERT_EQ(build_result, 0) << "Building TML library should succeed";

    // Compile C program with TML library
#ifdef _WIN32
    fs::path lib_file = test_dir / "test_ffi_lib.lib";
    fs::path exe_file = test_dir / "use_ffi_lib.exe";
    std::string compile_cmd = "clang -I" + test_dir.string() + " \"" + c_test_file.string() +
                              "\" \"" + lib_file.string() + "\" -o \"" + exe_file.string() +
                              "\" 2>&1";
#else
    fs::path lib_file = test_dir / "libtest_ffi_lib.a";
    fs::path exe_file = test_dir / "use_ffi_lib";
    std::string compile_cmd = "clang -I" + test_dir.string() + " \"" + c_test_file.string() +
                              "\" \"" + lib_file.string() + "\" -o \"" + exe_file.string() + "\"";
#endif

    int compile_result = run_command(compile_cmd);
    ASSERT_EQ(compile_result, 0) << "Compiling C program should succeed";
    ASSERT_TRUE(fs::exists(exe_file)) << "C executable should be created";

    // Run C program
    std::string run_cmd = "\"" + exe_file.string() + "\"";
    int run_result = run_command(run_cmd);
    EXPECT_EQ(run_result, 0) << "C program should execute successfully";
}

// Test: Build dynamic library with header
TEST_F(FFIIntegrationTest, BuildDynamicLibraryWithHeader) {
    std::string cmd = "\"" + tml_exe.string() + "\" build " + tml_lib_file.string() +
                      " --crate-type=dylib --emit-header --out-dir=" + test_dir.string();

    int result = run_command(cmd);
    EXPECT_EQ(result, 0) << "Building dynamic library should succeed";

    // Check that header was generated
    EXPECT_TRUE(fs::exists(c_header_file)) << "C header should be generated";

#ifdef _WIN32
    fs::path dll_file = test_dir / "test_ffi_lib.dll";
    fs::path lib_file = test_dir / "test_ffi_lib.lib"; // Import library
    EXPECT_TRUE(fs::exists(dll_file)) << "DLL should be created";
    EXPECT_TRUE(fs::exists(lib_file)) << "Import library should be created";
#else
    fs::path so_file = test_dir / "libtest_ffi_lib.so";
    EXPECT_TRUE(fs::exists(so_file)) << "Shared library should be created";
#endif
}

// Test: Header contains correct function declarations
TEST_F(FFIIntegrationTest, HeaderContainsCorrectDeclarations) {
    if (!has_ar_tool()) {
        GTEST_SKIP() << "Skipping: llvm-ar/ar not available for static library creation";
    }

    // Build library to generate header
    std::string cmd = "\"" + tml_exe.string() + "\" build " + tml_lib_file.string() +
                      " --crate-type=lib --emit-header --out-dir=" + test_dir.string();
    int result = run_command(cmd);
    ASSERT_EQ(result, 0) << "Building library should succeed";
    ASSERT_TRUE(fs::exists(c_header_file)) << "Header should exist";

    // Read header contents
    std::ifstream header(c_header_file);
    std::string content((std::istreambuf_iterator<char>(header)), std::istreambuf_iterator<char>());

    // Verify function declarations are present
    EXPECT_NE(content.find("tml_add_numbers"), std::string::npos)
        << "Header should contain add_numbers declaration";
    EXPECT_NE(content.find("tml_multiply_numbers"), std::string::npos)
        << "Header should contain multiply_numbers declaration";
    EXPECT_NE(content.find("tml_get_magic_number"), std::string::npos)
        << "Header should contain get_magic_number declaration";

    // Verify it has include guards
    EXPECT_NE(content.find("#ifndef"), std::string::npos) << "Header should have include guards";
    EXPECT_NE(content.find("#define"), std::string::npos) << "Header should define include guard";
}
