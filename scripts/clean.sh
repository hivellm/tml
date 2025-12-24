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
CLEAN_CACHE=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --all)
            CLEAN_ALL=true
            shift
            ;;
        --cache)
            CLEAN_CACHE=true
            shift
            ;;
        --help|-h)
            echo "TML Build Cleanup Script"
            echo ""
            echo "Usage: ./scripts/clean.sh [options]"
            echo ""
            echo "Options:"
            echo "  --all    Remove entire build directory"
            echo "  --cache  Only clean build cache for current target"
            echo "  --help   Show this help message"
            echo ""
            echo "Without options: cleans outputs and cache for current target"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            exit 1
            ;;
    esac
done

TARGET=$(detect_target)

if [ "$CLEAN_ALL" = true ]; then
    echo "Removing entire build directory..."
    rm -rf "$BUILD_DIR"
    echo "Done."
elif [ "$CLEAN_CACHE" = true ]; then
    echo "Cleaning build cache for $TARGET..."
    rm -rf "$BUILD_DIR/cache/$TARGET"
    echo "Done."
else
    echo "Cleaning build outputs and cache..."
    rm -rf "$BUILD_DIR/debug"
    rm -rf "$BUILD_DIR/release"
    rm -rf "$BUILD_DIR/cache/$TARGET"
    echo "Done."
fi
