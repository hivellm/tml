#!/bin/bash
# TML Compiler - Format Script
# Usage: ./scripts/format.sh [--check]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

CHECK_MODE=false
if [[ "$1" == "--check" ]]; then
    CHECK_MODE=true
fi

echo -e "${YELLOW}Running clang-format...${NC}"

# Find all C/C++ source files (excluding build directories and dependencies)
FILES=$(find "$ROOT_DIR/packages/compiler/src" \
    "$ROOT_DIR/packages/compiler/runtime" \
    \( -name "*.cpp" -o -name "*.c" -o -name "*.h" -o -name "*.hpp" \) \
    2>/dev/null)

if [ -z "$FILES" ]; then
    echo -e "${YELLOW}No files to format${NC}"
    exit 0
fi

# Run clang-format if available
if command -v clang-format &> /dev/null; then
    if [ "$CHECK_MODE" = true ]; then
        echo -e "${YELLOW}Checking format...${NC}"
        NEEDS_FORMAT=false
        while IFS= read -r file; do
            if [ -f "$file" ]; then
                DIFF=$(clang-format --dry-run --Werror "$file" 2>&1) || {
                    echo -e "  ${RED}Needs formatting: ${file#$ROOT_DIR/}${NC}"
                    NEEDS_FORMAT=true
                }
            fi
        done <<< "$FILES"

        if [ "$NEEDS_FORMAT" = true ]; then
            echo -e "${RED}Some files need formatting. Run ./scripts/format.sh to fix.${NC}"
            exit 1
        fi
        echo -e "${GREEN}All files are properly formatted${NC}"
    else
        echo -e "${YELLOW}Formatting files...${NC}"
        echo "$FILES" | while read -r file; do
            if [ -f "$file" ]; then
                echo -e "  Formatting: ${file#$ROOT_DIR/}"
                clang-format -i "$file"
            fi
        done
        echo -e "${GREEN}Format complete${NC}"
    fi
else
    echo -e "${YELLOW}clang-format not found, skipping format${NC}"
fi
