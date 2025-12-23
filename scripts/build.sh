#!/bin/bash
# TML Compiler Build Script
# Usage: ./scripts/build.sh [debug|release] [--clean] [--tests] [--target <triple>]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
DIM='\033[2m'
NC='\033[0m' # No Color

# Get the root directory (parent of scripts/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# Detect host target triple
detect_target() {
    local arch=$(uname -m)
    local os=$(uname -s | tr '[:upper:]' '[:lower:]')

    case "$arch" in
        x86_64|amd64) arch="x86_64" ;;
        aarch64|arm64) arch="aarch64" ;;
        i686|i386) arch="i686" ;;
    esac

    case "$os" in
        linux) echo "${arch}-unknown-linux-gnu" ;;
        darwin) echo "${arch}-apple-darwin" ;;
        mingw*|msys*|cygwin*) echo "${arch}-pc-windows-msvc" ;;
        *) echo "${arch}-unknown-${os}" ;;
    esac
}

# Default values
BUILD_TYPE="debug"
CLEAN_BUILD=false
BUILD_TESTS=true
TARGET=$(detect_target)

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        debug|Debug|DEBUG)
            BUILD_TYPE="debug"
            shift
            ;;
        release|Release|RELEASE)
            BUILD_TYPE="release"
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --no-tests)
            BUILD_TESTS=false
            shift
            ;;
        --tests)
            BUILD_TESTS=true
            shift
            ;;
        --target)
            TARGET="$2"
            shift 2
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
            echo "  --clean          Clean build directory before building"
            echo "  --tests          Build tests (default)"
            echo "  --no-tests       Don't build tests"
            echo "  --target <triple> Target triple (default: host)"
            echo "  --help           Show this help message"
            echo ""
            echo "Host target: $(detect_target)"
            echo ""
            echo "Output structure (like Rust's target/):"
            echo "  build/<target>/debug/"
            echo "  build/<target>/release/"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown argument: $1${NC}"
            exit 1
            ;;
    esac
done

# Set build directory (target-specific, like Rust's target/<triple>/)
BUILD_DIR="$ROOT_DIR/build/$TARGET/$BUILD_TYPE"

echo -e "${CYAN}╔════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║       TML Compiler Build System        ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════╝${NC}"
echo ""
echo -e "Target:      ${YELLOW}$TARGET${NC}"
echo -e "Build type:  ${YELLOW}$BUILD_TYPE${NC}"
echo -e "Build dir:   ${DIM}$BUILD_DIR${NC}"
echo -e "Tests:       ${YELLOW}$BUILD_TESTS${NC}"
echo ""

# Clean if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create directories
mkdir -p "$BUILD_DIR"

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
