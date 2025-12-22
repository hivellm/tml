cmake_minimum_required(VERSION 3.20)
project(tml VERSION 0.1.0 LANGUAGES CXX)

# C++20 required for concepts, ranges, etc.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(TML_BUILD_TESTS "Build tests" ON)
option(TML_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(TML_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    if(TML_ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
    if(TML_ENABLE_UBSAN)
        add_compile_options(-fsanitize=undefined)
        add_link_options(-fsanitize=undefined)
    endif()
elseif(MSVC)
    add_compile_options(/W4 /WX /utf-8)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Lexer library
add_library(tml_lexer STATIC
    src/lexer/token.cpp
    src/lexer/lexer.cpp
    src/lexer/source.cpp
)
target_include_directories(tml_lexer PUBLIC include src)

# Parser library
add_library(tml_parser STATIC
    src/parser/ast.cpp
    src/parser/parser.cpp
)
target_link_libraries(tml_parser PUBLIC tml_lexer)
target_include_directories(tml_parser PUBLIC include src)

# Type checker library
add_library(tml_types STATIC
    src/types/type.cpp
    src/types/checker.cpp
    src/types/env.cpp
)
target_link_libraries(tml_types PUBLIC tml_parser)
target_include_directories(tml_types PUBLIC include src)

# Borrow checker library
add_library(tml_borrow STATIC
    src/borrow/ownership.cpp
    src/borrow/tracker.cpp
    src/borrow/checker.cpp
)
target_link_libraries(tml_borrow PUBLIC tml_types)
target_include_directories(tml_borrow PUBLIC include src)

# IR library
add_library(tml_ir STATIC
    src/ir/ir.cpp
    src/ir/builder.cpp
    src/ir/generator.cpp
)
target_link_libraries(tml_ir PUBLIC tml_borrow)
target_include_directories(tml_ir PUBLIC include src)

# Codegen library (LLVM IR text generator + clang backend)
add_library(tml_codegen STATIC
    src/codegen/llvm_ir_gen.cpp
    src/codegen/c_gen.cpp
    src/codegen/stub.cpp
)
target_link_libraries(tml_codegen PUBLIC tml_ir)
target_include_directories(tml_codegen PUBLIC include src)

# Format library
add_library(tml_format STATIC
    src/format/formatter.cpp
)
target_link_libraries(tml_format PUBLIC tml_parser)
target_include_directories(tml_format PUBLIC include src)

# CLI library
add_library(tml_cli STATIC
    src/cli/args.cpp
    src/cli/driver.cpp
    src/cli/reporter.cpp
)
target_link_libraries(tml_cli PUBLIC tml_codegen tml_format)
target_include_directories(tml_cli PUBLIC include src)

# Main executable
add_executable(tml src/main.cpp)
target_link_libraries(tml PRIVATE tml_cli)

# Tests
if(TML_BUILD_TESTS)
    enable_testing()

    # Fetch GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)

    # Test executable
    add_executable(tml_tests
        tests/lexer_test.cpp
        tests/parser_test.cpp
        tests/types_test.cpp
        tests/borrow_test.cpp
        tests/ir_test.cpp
        tests/format_test.cpp
    )
    target_link_libraries(tml_tests PRIVATE
        tml_cli
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(tml_tests)
endif()

# Install
install(TARGETS tml RUNTIME DESTINATION bin)
install(DIRECTORY include/tml DESTINATION include)
