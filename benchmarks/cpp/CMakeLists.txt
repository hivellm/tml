cmake_minimum_required(VERSION 3.16)
project(tml_benchmarks CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Google Benchmark (optional)
find_package(benchmark QUIET)

add_executable(algorithms algorithms.cpp)

if(benchmark_FOUND)
    add_executable(algorithms_bench algorithms_bench.cpp)
    target_link_libraries(algorithms_bench benchmark::benchmark)
endif()

# Optimization flags
if(MSVC)
    target_compile_options(algorithms PRIVATE /O2)
else()
    target_compile_options(algorithms PRIVATE -O3)
endif()
