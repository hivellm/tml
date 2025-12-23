#!/bin/bash
# TML Compiler Build Script
# Usage: ./scripts/build.sh [debug|release] [--clean] [--tests]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Get the root directory (parent of scripts/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# Default values
BUILD_TYPE="debug"
CLEAN_BUILD=false
BUILD_TESTS=true

# Parse arguments
for arg in "$@"; do
    case $arg in
        debug|Debug|DEBUG)
            BUILD_TYPE="debug"
            ;;
        release|Release|RELEASE)
            BUILD_TYPE="release"
            ;;
        --clean)
            CLEAN_BUILD=true
            ;;
        --no-tests)
            BUILD_TESTS=false
            ;;
        --tests)
            BUILD_TESTS=true
            ;;
        --help|-h)
            echo "TML Compiler Build Script"
            echo ""
            echo "Usage: ./scripts/build.sh [debug|release] [options]"
            echo ""
            echo "Build types:"
            echo "  debug     Build with debug symbols (default)"
            echo "  release   Build with optimizations"
            echo ""
            echo "Options:"
            echo "  --clean     Clean build directory before building"
            echo "  --tests     Build tests (default)"
            echo "  --no-tests  Don't build tests"
            echo "  --help      Show this help message"
            echo ""
            echo "Output:"
            echo "  /build/debug/    Debug build output"
            echo "  /build/release/  Release build output"
            echo "  /build/cache/    Compilation cache"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown argument: $arg${NC}"
            exit 1
            ;;
    esac
done

# Set build directory
BUILD_DIR="$ROOT_DIR/build/$BUILD_TYPE"
CACHE_DIR="$ROOT_DIR/build/cache"

echo -e "${CYAN}╔════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║       TML Compiler Build System        ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════╝${NC}"
echo ""
echo -e "Build type:  ${YELLOW}$BUILD_TYPE${NC}"
echo -e "Build dir:   ${YELLOW}$BUILD_DIR${NC}"
echo -e "Tests:       ${YELLOW}$BUILD_TESTS${NC}"
echo ""

# Clean if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create directories
mkdir -p "$BUILD_DIR"
mkdir -p "$CACHE_DIR"

# Configure CMake
echo -e "${GREEN}Configuring CMake...${NC}"
cd "$BUILD_DIR"

CMAKE_BUILD_TYPE="Debug"
if [ "$BUILD_TYPE" = "release" ]; then
    CMAKE_BUILD_TYPE="Release"
fi

cmake "$ROOT_DIR/packages/compiler" \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DTML_BUILD_TESTS="$BUILD_TESTS" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
echo ""
echo -e "${GREEN}Building TML compiler...${NC}"

# Detect number of cores
if command -v nproc &> /dev/null; then
    JOBS=$(nproc)
elif command -v sysctl &> /dev/null; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=4
fi

cmake --build . --config "$CMAKE_BUILD_TYPE" -j "$JOBS"

# Copy compile_commands.json to root for IDE support
if [ -f "compile_commands.json" ]; then
    cp compile_commands.json "$ROOT_DIR/"
fi

# Print result
echo ""
echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║          Build Complete!               ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
echo ""
echo -e "Compiler:    ${CYAN}$BUILD_DIR/tml${NC}"
if [ "$BUILD_TESTS" = true ]; then
    echo -e "Tests:       ${CYAN}$BUILD_DIR/tml_tests${NC}"
fi
echo ""
echo -e "To use the compiler:"
echo -e "  ${YELLOW}$BUILD_DIR/tml --help${NC}"
echo ""
