# CMake Toolchain File for Zig CC
#
# Uses zig cc/c++ for compilation and linking with MSVC ABI.
# The wrapper scripts filter unsupported linker args and add -nostdlib++.
# Requires vcvars64 environment for Windows SDK headers.

get_filename_component(_toolchain_dir "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
get_filename_component(_root_dir "${_toolchain_dir}" DIRECTORY)
set(_scripts_dir "${_root_dir}/scripts/toolchain")

set(CMAKE_C_COMPILER "${_scripts_dir}/zig-cc.bat" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_scripts_dir}/zig-cxx.bat" CACHE FILEPATH "" FORCE)

# Skip compiler tests — Zig CC linking quirks fail CMake's test program
set(CMAKE_C_COMPILER_WORKS TRUE CACHE BOOL "" FORCE)
set(CMAKE_CXX_COMPILER_WORKS TRUE CACHE BOOL "" FORCE)

# Identify as Clang with required metadata
set(CMAKE_C_COMPILER_ID "Clang" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_ID "Clang" CACHE STRING "" FORCE)
set(CMAKE_C_COMPILER_VERSION "20.1.2" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_VERSION "20.1.2" CACHE STRING "" FORCE)

# C/C++ standard defaults (required since we skip ABI detection)
set(CMAKE_C_STANDARD_COMPUTED_DEFAULT "17" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_COMPUTED_DEFAULT "17" CACHE STRING "" FORCE)
set(CMAKE_C_EXTENSIONS_COMPUTED_DEFAULT "ON" CACHE STRING "" FORCE)
set(CMAKE_CXX_EXTENSIONS_COMPUTED_DEFAULT "ON" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILE_FEATURES "cxx_std_17;cxx_std_20;cxx_std_23" CACHE STRING "" FORCE)
set(CMAKE_C_COMPILE_FEATURES "c_std_11;c_std_17" CACHE STRING "" FORCE)

# Use MSVC resource compiler (rc.exe) — Zig doesn't include windres
set(CMAKE_RC_COMPILER "rc" CACHE FILEPATH "" FORCE)

# Archiver and ranlib — use zig ar (LLVM-ar compatible, avoids CMAKE_AR-NOTFOUND on clean builds)
set(CMAKE_AR "${_scripts_dir}/zig-ar.bat" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB "${_scripts_dir}/zig-ar.bat" CACHE FILEPATH "" FORCE)
# Silence CMake's "ranlib needs no args" warning for LLVM-style archives
set(CMAKE_C_CREATE_STATIC_LIBRARY "<CMAKE_AR> qc <TARGET> <OBJECTS>" CACHE STRING "" FORCE)
set(CMAKE_CXX_CREATE_STATIC_LIBRARY "<CMAKE_AR> qc <TARGET> <OBJECTS>" CACHE STRING "" FORCE)

# Force static CRT (/MT) to match F:/LLVM static libs built with /MT
# Zig CC default is static CRT; --dependent-lib=libcmt links the static CRT
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_INIT "-Xclang --dependent-lib=libcmt -fno-sanitize=undefined" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_INIT "-Xclang --dependent-lib=libcmt -fno-sanitize=undefined" CACHE STRING "" FORCE)

set(TML_USE_ZIG_CC ON CACHE BOOL "Using Zig CC toolchain" FORCE)
