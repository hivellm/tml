#!/bin/bash
# TML Test Runner Script
# Usage: ./scripts/test.sh [options]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# Detect host target triple (same as build.sh)
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

TARGET=$(detect_target)

# Find compiler (check debug first, then release)
TML_EXE="$ROOT_DIR/build/$TARGET/debug/tml"
if [ ! -f "$TML_EXE" ]; then
    TML_EXE="$ROOT_DIR/build/$TARGET/release/tml"
fi

# Add .exe extension on Windows/MSYS
if [ -f "$TML_EXE.exe" ]; then
    TML_EXE="$TML_EXE.exe"
fi

if [ ! -f "$TML_EXE" ]; then
    echo "Error: Compiler not found at build/$TARGET/"
    echo "Run ./scripts/build.sh first."
    exit 1
fi

cd "$ROOT_DIR"

echo "Running TML tests..."
echo ""

# Pass all arguments to tml test
"$TML_EXE" test "$@"
