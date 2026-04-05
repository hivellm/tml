# Linux Distribution Packages

## Strategy: Multiple Distribution Formats

| Format | Target | Tool | Priority |
|--------|--------|------|----------|
| `.tar.gz` | Universal (manual install) | tar + install script | P0 |
| `.deb` | Debian, Ubuntu, Mint | dpkg-deb / nfpm | P1 |
| `.rpm` | Fedora, RHEL, CentOS, openSUSE | rpmbuild / nfpm | P1 |
| Snap | Ubuntu Software Center | snapcraft | P2 |
| Flatpak | GNOME Software | flatpak-builder | P3 |
| AppImage | Universal (single file) | appimagetool | P3 |

## 1. Tarball + Install Script (P0 — Universal)

### Layout

```
tml-0.2.9-linux-x64.tar.gz
├── install.sh                     # Interactive installer
├── bin/
│   ├── tml
│   ├── tml-mcp
│   ├── tml-daemon
│   └── plugins/
│       ├── libtml_compiler.so
│       ├── libtml_codegen_x86.so
│       ├── libtml_mcp.so
│       ├── libtml_test.so
│       ├── libtml_tools.so
│       └── manifest.json
├── lib/
│   ├── core/...
│   ├── std/...
│   └── test/...
├── share/
│   ├── man/man1/tml.1
│   └── doc/tml/
│       ├── examples/
│       └── README.md
└── LICENSE
```

### install.sh

```bash
#!/bin/bash
set -euo pipefail

PREFIX="${1:-/usr/local}"
BINDIR="$PREFIX/bin"
LIBDIR="$PREFIX/lib/tml"
SHAREDIR="$PREFIX/share"

echo "Installing TML 0.2.9 to $PREFIX..."

# Check for root if installing to /usr/local
if [[ "$PREFIX" == /usr/* ]] && [[ $EUID -ne 0 ]]; then
    echo "Error: Installing to $PREFIX requires root. Use: sudo ./install.sh"
    echo "Or install to user directory: ./install.sh ~/.local"
    exit 1
fi

# Create directories
mkdir -p "$BINDIR" "$LIBDIR/plugins"

# Copy binaries
install -m 755 bin/tml bin/tml-mcp bin/tml-daemon "$BINDIR/"
install -m 644 bin/plugins/*.so "$LIBDIR/plugins/"
install -m 644 bin/plugins/manifest.json "$LIBDIR/plugins/"

# Copy standard library
cp -r lib/core lib/std lib/test "$LIBDIR/"

# Copy docs
mkdir -p "$SHAREDIR/doc/tml" "$SHAREDIR/man/man1"
cp -r share/doc/tml/* "$SHAREDIR/doc/tml/"
install -m 644 share/man/man1/tml.1 "$SHAREDIR/man/man1/"

# Set TML_HOME for plugin/library discovery
echo "export TML_HOME=$LIBDIR" > /etc/profile.d/tml.sh 2>/dev/null || \
    echo "Add to your shell profile: export TML_HOME=$LIBDIR"

# Verify PATH
if command -v tml &>/dev/null; then
    echo "TML $(tml --version) installed successfully!"
else
    echo "TML installed to $BINDIR/tml"
    echo "Add to PATH if not already: export PATH=\"$BINDIR:\$PATH\""
fi
```

### Uninstall

```bash
#!/bin/bash
PREFIX="${1:-/usr/local}"
rm -f "$PREFIX/bin/tml" "$PREFIX/bin/tml-mcp" "$PREFIX/bin/tml-daemon"
rm -rf "$PREFIX/lib/tml"
rm -rf "$PREFIX/share/doc/tml"
rm -f "$PREFIX/share/man/man1/tml.1"
rm -f /etc/profile.d/tml.sh
echo "TML uninstalled."
```

## 2. Debian Package (.deb) — P1

### Tool: nfpm (cross-platform package builder)

**Why nfpm:** Single Go binary, builds .deb + .rpm from YAML config, no distro-specific tooling needed. Used by Grafana, Hugo, GoReleaser.

### nfpm.yaml

```yaml
name: tml
arch: amd64
platform: linux
version: "0.2.9"
maintainer: "HiveLLM <team@hivellm.com>"
description: "TML compiler — systems programming language for LLM code generation"
homepage: "https://github.com/hivellm/tml"
license: "Apache-2.0"

depends:
  - libc6 (>= 2.31)
  - libssl3 | libssl1.1        # OpenSSL (either version)
  - libsqlite3-0               # SQLite
  - zlib1g                     # zlib
  - libzstd1                   # Zstandard

recommends:
  - gnuplot                    # For chart library

contents:
  # Binaries
  - src: build/release/bin/tml
    dst: /usr/bin/tml
    file_info:
      mode: 0755

  - src: build/release/bin/tml_mcp
    dst: /usr/bin/tml-mcp
    file_info:
      mode: 0755

  - src: build/release/bin/tml_daemon
    dst: /usr/bin/tml-daemon
    file_info:
      mode: 0755

  # Plugins
  - src: build/release/bin/plugins/
    dst: /usr/lib/tml/plugins/

  # Standard library
  - src: lib/core/
    dst: /usr/lib/tml/core/
    type: tree

  - src: lib/std/
    dst: /usr/lib/tml/std/
    type: tree

  - src: lib/test/
    dst: /usr/lib/tml/test/
    type: tree

  # Docs
  - src: docs/examples/
    dst: /usr/share/doc/tml/examples/
    type: tree

  - src: LICENSE
    dst: /usr/share/licenses/tml/LICENSE

  # PATH and environment
  - src: installer/linux/tml.sh
    dst: /etc/profile.d/tml.sh
    file_info:
      mode: 0644

scripts:
  postinstall: installer/linux/postinstall.sh
  preremove: installer/linux/preremove.sh
```

### FHS Layout

```
/usr/bin/tml                           # Symlink or binary
/usr/bin/tml-mcp
/usr/bin/tml-daemon
/usr/lib/tml/plugins/libtml_compiler.so
/usr/lib/tml/plugins/libtml_codegen_x86.so
/usr/lib/tml/plugins/libtml_mcp.so
/usr/lib/tml/plugins/libtml_test.so
/usr/lib/tml/plugins/libtml_tools.so
/usr/lib/tml/core/...                  # Core library
/usr/lib/tml/std/...                   # Standard library
/usr/lib/tml/test/...                  # Test framework
/usr/share/doc/tml/examples/           # Examples
/usr/share/licenses/tml/LICENSE        # License
/usr/share/man/man1/tml.1             # Man page
/etc/profile.d/tml.sh                 # TML_HOME env var
```

### Build Command

```bash
nfpm package --packager deb --target tml-0.2.9-amd64.deb
nfpm package --packager rpm --target tml-0.2.9-x86_64.rpm
```

## 3. RPM Package (.rpm) — P1

Same nfpm.yaml produces RPM. Key differences:

```yaml
# RPM-specific overrides
rpm:
  group: "Development/Languages"
  compression: xz
  signature:
    key_file: keys/rpm-signing.gpg
```

**Dependencies mapping:**

| Debian | RPM |
|--------|-----|
| libc6 | glibc |
| libssl3 | openssl-libs |
| libsqlite3-0 | sqlite-libs |
| zlib1g | zlib |
| libzstd1 | libzstd |

## 4. Bundle vs. System Dependencies

### Strategy: Bundle all on Linux too

Shipping `.so` files alongside binaries avoids:
- Version conflicts (Ubuntu 22.04 has OpenSSL 3.0, 24.04 has 3.2)
- Missing packages on minimal installations
- Different package names across distros

**Implementation:** Set `RPATH` at link time:

```cmake
set_target_properties(tml PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib/tml/deps"
    BUILD_WITH_INSTALL_RPATH ON)
```

Then ship:
```
/usr/lib/tml/deps/
├── libcrypto.so.3
├── libssl.so.3
├── libsqlite3.so.0
├── libz.so.1
├── libzstd.so.1
├── libbrotlicommon.so.1
├── libbrotlidec.so.1
└── libbrotlienc.so.1
```

This mirrors Rust's approach (rustup bundles everything) and eliminates "works on Ubuntu but not Fedora" issues.

## 5. APT/DNF Repository (Future)

For seamless updates:

```bash
# User adds repo
curl -fsSL https://packages.hivellm.com/gpg | sudo gpg --dearmor -o /usr/share/keyrings/tml.gpg
echo "deb [signed-by=/usr/share/keyrings/tml.gpg] https://packages.hivellm.com/deb stable main" | \
    sudo tee /etc/apt/sources.list.d/tml.list

# Then install/update
sudo apt update && sudo apt install tml
```

This requires hosting infrastructure (S3 + CloudFront or similar). Defer until user base grows.
