#!/bin/bash
# Fetch pre-built libpq binaries for all platforms.
# Run from the lib/postgresql/ directory.
#
# Usage:
#   bash scripts/fetch-libpq.sh           # all platforms
#   bash scripts/fetch-libpq.sh windows   # windows only
#   bash scripts/fetch-libpq.sh linux     # linux only
#   bash scripts/fetch-libpq.sh macos     # macos only

set -euo pipefail
cd "$(dirname "$0")/.."

PG_VERSION="17.4-1"
PG_MAJOR="17"

NATIVE_DIR="native"
mkdir -p "$NATIVE_DIR/win-x64" "$NATIVE_DIR/linux-x64" "$NATIVE_DIR/macos-arm64"

TMPDIR="${TMPDIR:-/tmp}/libpq-fetch-$$"
mkdir -p "$TMPDIR"
trap 'rm -rf "$TMPDIR"' EXIT

TARGET="${1:-all}"

# ── Windows x64 ──────────────────────────────────────────────────────────────
fetch_windows() {
    echo "=== Fetching libpq for Windows x64 ==="
    local URL="https://get.enterprisedb.com/postgresql/postgresql-${PG_VERSION}-windows-x64-binaries.zip"
    local ZIP="$TMPDIR/pg-win.zip"

    echo "  Downloading $URL ..."
    curl -L -o "$ZIP" "$URL" --progress-bar

    echo "  Extracting libpq files..."
    # Extract only the files we need from pgsql/bin/ and pgsql/lib/
    unzip -o -j "$ZIP" "pgsql/bin/libpq.dll" -d "$NATIVE_DIR/win-x64/" 2>/dev/null || true
    unzip -o -j "$ZIP" "pgsql/bin/libintl-9.dll" -d "$NATIVE_DIR/win-x64/" 2>/dev/null || true
    unzip -o -j "$ZIP" "pgsql/bin/libssl-3-x64.dll" -d "$NATIVE_DIR/win-x64/" 2>/dev/null || true
    unzip -o -j "$ZIP" "pgsql/bin/libcrypto-3-x64.dll" -d "$NATIVE_DIR/win-x64/" 2>/dev/null || true
    unzip -o -j "$ZIP" "pgsql/bin/libiconv-2.dll" -d "$NATIVE_DIR/win-x64/" 2>/dev/null || true
    unzip -o -j "$ZIP" "pgsql/lib/libpq.lib" -d "$NATIVE_DIR/win-x64/" 2>/dev/null || true

    # If libpq.lib wasn't in lib/, try to generate it from the DLL
    if [ ! -f "$NATIVE_DIR/win-x64/libpq.lib" ]; then
        echo "  WARNING: libpq.lib not found in zip, checking for .a..."
        unzip -o -j "$ZIP" "pgsql/lib/libpq.a" -d "$NATIVE_DIR/win-x64/" 2>/dev/null || true
    fi

    echo "  Windows files:"
    ls -la "$NATIVE_DIR/win-x64/" 2>/dev/null || echo "  (none)"
}

# ── Linux x64 ────────────────────────────────────────────────────────────────
fetch_linux() {
    echo "=== Fetching libpq for Linux x64 ==="

    # Use the PostgreSQL apt repository's libpq package
    # Download the .deb and extract just libpq.so
    local URL="https://apt.postgresql.org/pub/repos/apt/pool/main/p/postgresql-${PG_MAJOR}/libpq5_${PG_VERSION}.pgdg22.04+2_amd64.deb"
    local DEB="$TMPDIR/libpq5.deb"
    local DEV_URL="https://apt.postgresql.org/pub/repos/apt/pool/main/p/postgresql-${PG_MAJOR}/libpq-dev_${PG_VERSION}.pgdg22.04+2_amd64.deb"
    local DEV_DEB="$TMPDIR/libpq-dev.deb"

    echo "  Downloading libpq5 .deb ..."
    curl -L -o "$DEB" "$URL" --progress-bar 2>/dev/null || {
        echo "  Direct .deb download failed. Trying alternative..."
        # Fallback: just note what's needed
        echo "  For Linux, install: apt install libpq-dev"
        echo "  Then copy: cp /usr/lib/x86_64-linux-gnu/libpq.so* native/linux-x64/"
        echo "  And:       cp /usr/lib/x86_64-linux-gnu/libpq.a native/linux-x64/"
        return 0
    }

    echo "  Extracting..."
    cd "$TMPDIR"
    ar x "$DEB" 2>/dev/null || true
    tar xf data.tar.* 2>/dev/null || true
    find . -name "libpq.so*" -exec cp {} "$OLDPWD/$NATIVE_DIR/linux-x64/" \; 2>/dev/null || true
    cd "$OLDPWD"

    # Try dev package for static lib
    echo "  Downloading libpq-dev .deb ..."
    curl -L -o "$DEV_DEB" "$DEV_URL" --progress-bar 2>/dev/null || true
    if [ -f "$DEV_DEB" ]; then
        cd "$TMPDIR"
        rm -f data.tar.* control.tar.* debian-binary
        ar x "$DEV_DEB" 2>/dev/null || true
        tar xf data.tar.* 2>/dev/null || true
        find . -name "libpq.a" -exec cp {} "$OLDPWD/$NATIVE_DIR/linux-x64/" \; 2>/dev/null || true
        find . -name "libpq-fe.h" -exec cp {} "$OLDPWD/$NATIVE_DIR/linux-x64/" \; 2>/dev/null || true
        cd "$OLDPWD"
    fi

    echo "  Linux files:"
    ls -la "$NATIVE_DIR/linux-x64/" 2>/dev/null || echo "  (none)"
}

# ── macOS arm64 ──────────────────────────────────────────────────────────────
fetch_macos() {
    echo "=== Fetching libpq for macOS arm64 ==="

    # PostgreSQL official provides .dmg but extracting on non-macOS is hard.
    # For cross-compilation, note the instructions:
    echo "  macOS libpq must be copied from a macOS machine."
    echo ""
    echo "  On macOS with Homebrew:"
    echo "    brew install libpq"
    echo "    cp /opt/homebrew/opt/libpq/lib/libpq.dylib native/macos-arm64/"
    echo "    cp /opt/homebrew/opt/libpq/lib/libpq.a native/macos-arm64/"
    echo ""
    echo "  Or with Postgres.app:"
    echo "    cp /Applications/Postgres.app/Contents/Versions/latest/lib/libpq.dylib native/macos-arm64/"
    echo "    cp /Applications/Postgres.app/Contents/Versions/latest/lib/libpq.a native/macos-arm64/"

    # Create a README in the directory
    cat > "$NATIVE_DIR/macos-arm64/README.md" << 'HEREDOC'
# macOS arm64 libpq

Copy these files from a macOS machine:

```bash
# Homebrew
brew install libpq
cp /opt/homebrew/opt/libpq/lib/libpq.dylib .
cp /opt/homebrew/opt/libpq/lib/libpq.a .

# Or Postgres.app
cp /Applications/Postgres.app/Contents/Versions/latest/lib/libpq.dylib .
cp /Applications/Postgres.app/Contents/Versions/latest/lib/libpq.a .
```
HEREDOC
}

# ── Main ─────────────────────────────────────────────────────────────────────
case "$TARGET" in
    windows) fetch_windows ;;
    linux)   fetch_linux ;;
    macos)   fetch_macos ;;
    all)
        fetch_windows
        echo ""
        fetch_linux
        echo ""
        fetch_macos
        ;;
    *)
        echo "Usage: $0 [windows|linux|macos|all]"
        exit 1
        ;;
esac

echo ""
echo "=== Done ==="
echo "Files in native/:"
find native/ -type f -not -name "README.md" | sort
