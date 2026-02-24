cmake_minimum_required(VERSION 3.16)
project(tml_profile_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release optimizations (MSVC vs GCC/Clang)
set(CMAKE_BUILD_TYPE Release)
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG /EHsc")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# Include paths
set(COMPILER_DIR "${CMAKE_SOURCE_DIR}/../../compiler")
include_directories(${COMPILER_DIR}/include)

# All benchmarks
add_executable(math_bench math_bench.cpp)
add_executable(string_bench string_bench.cpp)
add_executable(control_flow_bench control_flow_bench.cpp)
add_executable(closure_bench closure_bench.cpp)
add_executable(function_bench function_bench.cpp)
add_executable(collections_bench collections_bench.cpp)
add_executable(memory_bench memory_bench.cpp)
add_executable(text_bench text_bench.cpp)
add_executable(oop_bench oop_bench.cpp)
add_executable(encoding_bench encoding_bench.cpp)
add_executable(type_bench type_bench.cpp)

# JSON benchmark (requires json source files - check if they exist)
set(JSON_SRC_1 "${COMPILER_DIR}/src/json/json_value.cpp")
set(JSON_SRC_2 "${COMPILER_DIR}/src/json/json_fast_parser.cpp")
if(EXISTS "${JSON_SRC_1}" AND EXISTS "${JSON_SRC_2}")
    add_executable(json_bench json_bench.cpp ${JSON_SRC_1} ${JSON_SRC_2})
    set(JSON_BENCH_TARGET json_bench)
else()
    message(STATUS "json_bench: SKIPPED (json source files not found)")
    set(JSON_BENCH_TARGET "")
endif()

# All targets list (without crypto/json - added conditionally)
set(ALL_BENCHMARKS
    math_bench string_bench control_flow_bench closure_bench function_bench
    collections_bench memory_bench text_bench oop_bench encoding_bench type_bench
    ${JSON_BENCH_TARGET}
)

# Crypto benchmark (requires OpenSSL)
set(VCPKG_OPENSSL_DIR "${CMAKE_SOURCE_DIR}/../../vcpkg_installed/x64-windows")
set(SYSTEM_OPENSSL_DIR "C:/Program Files/OpenSSL-Win64")

if(EXISTS "${VCPKG_OPENSSL_DIR}/include/openssl/evp.h")
    add_executable(crypto_bench crypto_bench.cpp)
    target_include_directories(crypto_bench PRIVATE "${VCPKG_OPENSSL_DIR}/include")
    if(WIN32)
        target_link_libraries(crypto_bench PRIVATE
            "${VCPKG_OPENSSL_DIR}/lib/libcrypto.lib"
            "${VCPKG_OPENSSL_DIR}/lib/libssl.lib"
            crypt32 ws2_32)
    else()
        target_link_libraries(crypto_bench PRIVATE
            "${VCPKG_OPENSSL_DIR}/lib/libcrypto.a"
            "${VCPKG_OPENSSL_DIR}/lib/libssl.a")
    endif()
    list(APPEND ALL_BENCHMARKS crypto_bench)
    message(STATUS "crypto_bench: OpenSSL found via vcpkg")
elseif(WIN32 AND EXISTS "${SYSTEM_OPENSSL_DIR}/include/openssl/evp.h")
    add_executable(crypto_bench crypto_bench.cpp)
    target_include_directories(crypto_bench PRIVATE "${SYSTEM_OPENSSL_DIR}/include")
    target_link_libraries(crypto_bench PRIVATE
        "${SYSTEM_OPENSSL_DIR}/lib/libcrypto_static.lib"
        "${SYSTEM_OPENSSL_DIR}/lib/libssl_static.lib"
        crypt32 ws2_32)
    list(APPEND ALL_BENCHMARKS crypto_bench)
    message(STATUS "crypto_bench: OpenSSL found at system path")
else()
    message(STATUS "crypto_bench: SKIPPED (OpenSSL not found)")
endif()

# Output to results/bin directory
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/../results/bin")
set_target_properties(${ALL_BENCHMARKS}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR}
)

# Install target
install(TARGETS ${ALL_BENCHMARKS}
    RUNTIME DESTINATION ${OUTPUT_DIR}
)
