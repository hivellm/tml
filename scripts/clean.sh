#!/bin/bash
# TML Build Cleanup Script
# Usage: ./scripts/clean.sh [--all] [--target <triple>]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build"

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

CLEAN_ALL=false
TARGET=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --all)
            CLEAN_ALL=true
            shift
            ;;
        --target)
            TARGET="$2"
            shift 2
            ;;
        --help|-h)
            echo "TML Build Cleanup Script"
            echo ""
            echo "Usage: ./scripts/clean.sh [options]"
            echo ""
            echo "Options:"
            echo "  --all            Remove entire build directory (all targets)"
            echo "  --target <triple> Only clean specific target"
            echo "  --help           Show this help message"
            echo ""
            echo "Without options: cleans current host target ($(detect_target))"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            exit 1
            ;;
    esac
done

if [ "$CLEAN_ALL" = true ]; then
    echo "Removing entire build directory..."
    rm -rf "$BUILD_DIR"
    echo "Done."
elif [ -n "$TARGET" ]; then
    echo "Cleaning target: $TARGET..."
    rm -rf "$BUILD_DIR/$TARGET"
    echo "Done."
else
    TARGET=$(detect_target)
    echo "Cleaning host target: $TARGET..."
    rm -rf "$BUILD_DIR/$TARGET"
    echo "Done."
fi
