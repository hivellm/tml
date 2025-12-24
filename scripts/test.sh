#!/bin/bash
# TML Test Runner Script
# Usage: ./scripts/test.sh [options]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# Find compiler (check debug first, then release)
# Structure: build/debug/tml or build/release/tml
TML_EXE="$ROOT_DIR/build/debug/tml"
if [ ! -f "$TML_EXE" ]; then
    TML_EXE="$ROOT_DIR/build/release/tml"
fi

# Add .exe extension on Windows/MSYS
if [ -f "$TML_EXE.exe" ]; then
    TML_EXE="$TML_EXE.exe"
fi

if [ ! -f "$TML_EXE" ]; then
    echo "Error: Compiler not found at build/debug/ or build/release/"
    echo "Run ./scripts/build.sh first."
    exit 1
fi

cd "$ROOT_DIR"

echo "Running TML tests..."
echo ""

# Pass all arguments to tml test
"$TML_EXE" test "$@"
