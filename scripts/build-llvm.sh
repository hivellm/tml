#!/usr/bin/env bash
# Build the vendored LLVM submodule (src/llvm-project) into build/llvm/.
# This is a ONE-TIME build (~30-90 minutes). Subsequent TML compiler
# builds pick up build/llvm/ automatically via CMake's find_package.
#
# Usage:
#   scripts/build-llvm.sh            Release build (default)
#   scripts/build-llvm.sh debug      Debug build
#   scripts/build-llvm.sh --clean    Delete build/llvm/ and rebuild

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LLVM_SRC="$ROOT_DIR/src/llvm-project/llvm"
LLVM_BUILD="$ROOT_DIR/build/llvm"

BUILD_TYPE="Release"
CLEAN=0
for arg in "$@"; do
  case "$arg" in
    debug)   BUILD_TYPE="Debug" ;;
    release) BUILD_TYPE="Release" ;;
    --clean) CLEAN=1 ;;
    --help|-h)
      echo "Build the vendored LLVM submodule into build/llvm/."
      echo
      echo "Usage: scripts/build-llvm.sh [release|debug] [--clean]"
      exit 0
      ;;
    *) echo "Unknown argument: $arg" >&2; exit 1 ;;
  esac
done

if [ ! -f "$LLVM_SRC/CMakeLists.txt" ]; then
  echo "ERROR: src/llvm-project/llvm/CMakeLists.txt not found." >&2
  echo "Initialize the submodule with:" >&2
  echo "  git submodule update --init --recursive src/llvm-project" >&2
  exit 1
fi

if [ "$CLEAN" = "1" ] && [ -d "$LLVM_BUILD" ]; then
  echo "Removing existing build/llvm/ ..."
  rm -rf "$LLVM_BUILD"
fi

mkdir -p "$LLVM_BUILD"

echo "================================================================================"
echo "  Building vendored LLVM   (this takes 30-90 minutes on first run)"
echo "================================================================================"
echo "  Source:   $LLVM_SRC"
echo "  Build:    $LLVM_BUILD"
echo "  Type:     $BUILD_TYPE"
echo "================================================================================"
echo

cd "$LLVM_BUILD"

if [ ! -f "CMakeCache.txt" ]; then
  echo "Configuring LLVM (first run only) ..."
  cmake "$LLVM_SRC" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DLLVM_TARGETS_TO_BUILD=X86 \
    -DLLVM_ENABLE_PROJECTS=lld \
    -DLLVM_ENABLE_RUNTIMES= \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_BENCHMARKS=OFF \
    -DLLVM_INCLUDE_DOCS=OFF \
    -DLLVM_BUILD_TOOLS=OFF \
    -DLLVM_INCLUDE_UTILS=OFF \
    -DLLVM_ENABLE_TERMINFO=OFF \
    -DLLVM_ENABLE_LIBXML2=OFF \
    -DLLVM_ENABLE_ZLIB=OFF \
    -DLLVM_ENABLE_ZSTD=OFF \
    -DLLVM_ENABLE_DIA_SDK=OFF \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=OFF
else
  echo "Re-using existing CMake cache at $LLVM_BUILD/CMakeCache.txt"
fi

echo
echo "Compiling LLVM (incremental; Ctrl+C safe to resume) ..."
cmake --build . --config "$BUILD_TYPE" --parallel

echo
echo "================================================================================"
echo "  LLVM build complete"
echo "================================================================================"
echo "  Libraries at: $LLVM_BUILD/$BUILD_TYPE/lib   (or $LLVM_BUILD/lib on multi-config)"
echo "  Headers at:   $LLVM_BUILD/include"
echo
echo "Next step: run scripts/build.sh to build the TML compiler."
echo "================================================================================"
