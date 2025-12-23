#!/bin/bash
# TML Build Cleanup Script
# Usage: ./scripts/clean.sh [--all]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build"

if [ "$1" = "--all" ]; then
    echo "Removing entire build directory..."
    rm -rf "$BUILD_DIR"
    echo "Done."
else
    echo "Cleaning build artifacts..."
    rm -rf "$BUILD_DIR/debug"
    rm -rf "$BUILD_DIR/release"
    echo "Done. (Cache preserved. Use --all to remove everything)"
fi
