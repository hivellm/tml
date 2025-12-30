#!/bin/bash
# TML Compiler - Lint Script
# Usage: ./scripts/lint.sh [--fix]
#
# Runs:
#   1. tml lint - for TML files (if compiler available)
#   2. clang-tidy - for C/C++ files (if available)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

FIX_MODE=""
if [[ "$1" == "--fix" ]]; then
    FIX_MODE="--fix"
fi

cd "$ROOT_DIR"

# ============================================
# TML Files Lint (using tml lint command)
# ============================================
echo -e "${YELLOW}Linting TML files...${NC}"

# Find TML compiler
TML_EXE=""
if [ -f "./build/debug/tml" ]; then
    TML_EXE="./build/debug/tml"
elif [ -f "./build/release/tml" ]; then
    TML_EXE="./build/release/tml"
fi

if [ -n "$TML_EXE" ]; then
    "$TML_EXE" lint $FIX_MODE lib examples
else
    echo -e "  ${YELLOW}TML compiler not found. Build first: ./scripts/build.sh${NC}"
    echo -e "  ${YELLOW}Falling back to basic checks...${NC}"

    # Basic fallback check
    ERRORS=0
    for file in $(find lib examples -name "*.tml" 2>/dev/null); do
        if [ -f "$file" ]; then
            if grep -q $'\t' "$file" 2>/dev/null; then
                echo -e "  ${RED}[TAB]${NC} $file"
                ERRORS=$((ERRORS + 1))
            fi
            if grep -q '[[:space:]]$' "$file" 2>/dev/null; then
                echo -e "  ${RED}[TRAIL]${NC} $file"
                ERRORS=$((ERRORS + 1))
            fi
        fi
    done

    if [ $ERRORS -gt 0 ]; then
        echo -e "${RED}Found $ERRORS errors${NC}"
        exit 1
    fi
fi

# ============================================
# C/C++ Files Lint (using clang-tidy)
# ============================================
echo -e "${YELLOW}Linting C/C++ files...${NC}"

if command -v clang-tidy &> /dev/null; then
    # Find compile_commands.json
    COMPILE_COMMANDS="$ROOT_DIR/compile_commands.json"
    if [ ! -f "$COMPILE_COMMANDS" ]; then
        COMPILE_COMMANDS=$(find "$ROOT_DIR/build" -name "compile_commands.json" 2>/dev/null | head -1)
    fi

    CLANG_ARGS=""
    if [ -n "$COMPILE_COMMANDS" ] && [ -f "$COMPILE_COMMANDS" ]; then
        CLANG_ARGS="-p $(dirname "$COMPILE_COMMANDS")"
    fi

    if [ -n "$FIX_MODE" ]; then
        CLANG_ARGS="$CLANG_ARGS --fix"
    fi

    # Run on key C++ files
    for file in compiler/src/main.cpp; do
        if [ -f "$file" ]; then
            echo -e "  Checking: $file"
            clang-tidy $CLANG_ARGS "$file" -- -std=c++17 2>/dev/null || true
        fi
    done
else
    echo -e "  ${YELLOW}clang-tidy not found, skipping C++ lint${NC}"
fi

echo -e "${GREEN}Lint complete${NC}"
