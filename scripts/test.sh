#!/bin/bash
# TML Test Runner Script
# Usage: ./scripts/test.sh [options]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# Check if compiler is built
TML_EXE="$ROOT_DIR/build/debug/tml"
if [ ! -f "$TML_EXE" ]; then
    TML_EXE="$ROOT_DIR/build/debug/Debug/tml"  # MSVC structure
fi
if [ ! -f "$TML_EXE" ] && [ ! -f "$TML_EXE.exe" ]; then
    echo "Error: Compiler not found. Run ./scripts/build.sh first."
    exit 1
fi

# Add .exe extension on Windows
if [ -f "$TML_EXE.exe" ]; then
    TML_EXE="$TML_EXE.exe"
fi

cd "$ROOT_DIR"

echo "Running TML tests..."
echo ""

# Pass all arguments to tml test
"$TML_EXE" test "$@"
