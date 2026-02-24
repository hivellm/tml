cmake_minimum_required(VERSION 3.16)
project(tml_profile_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release optimizations
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include paths
set(COMPILER_DIR "${CMAKE_SOURCE_DIR}/../../compiler")
include_directories(${COMPILER_DIR}/include)

# JSON library sources (needed for json_bench)
set(JSON_SOURCES
    ${COMPILER_DIR}/src/json/json.cpp
    ${COMPILER_DIR}/src/json/json_fast_parser.cpp
)

# Math benchmark
add_executable(math_bench math_bench.cpp)

# String benchmark
add_executable(string_bench string_bench.cpp)

# JSON benchmark (needs JSON library)
add_executable(json_bench json_bench.cpp ${JSON_SOURCES})

# Collections benchmark
add_executable(collections_bench collections_bench.cpp)

# Memory benchmark
add_executable(memory_bench memory_bench.cpp)

# Text benchmark
add_executable(text_bench text_bench.cpp)

# Output to results/bin directory
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/../results/bin")
set_target_properties(math_bench string_bench json_bench collections_bench memory_bench text_bench
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR}
)

# Install target
install(TARGETS math_bench string_bench json_bench collections_bench memory_bench text_bench
    RUNTIME DESTINATION ${OUTPUT_DIR}
)
