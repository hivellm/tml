# macOS Distribution

## Strategy: .pkg Installer + Homebrew Formula

| Format | Target | Priority |
|--------|--------|----------|
| `.pkg` | Direct download (official) | P0 |
| Homebrew formula | `brew install tml` | P1 |
| `.dmg` with drag-install | GUI users | P3 (low value for CLI tool) |

## 1. .pkg Installer (P0)

### Tool: pkgbuild + productbuild (built into Xcode)

```bash
# Build the component package
pkgbuild --root staging/ \
         --identifier com.hivellm.tml \
         --version 0.2.9 \
         --install-location /usr/local \
         --scripts scripts/macos/ \
         tml-component.pkg

# Build the product (with UI, license, readme)
productbuild --distribution Distribution.xml \
             --package-path . \
             --resources resources/ \
             TML-0.2.9-macos.pkg
```

### Layout

```
/usr/local/
├── bin/
│   ├── tml                          # Main binary
│   ├── tml-mcp                      # MCP server
│   └── tml-daemon                   # Daemon
├── lib/tml/
│   ├── plugins/
│   │   ├── libtml_compiler.dylib
│   │   ├── libtml_codegen_x86.dylib  # Intel Macs
│   │   ├── libtml_codegen_arm64.dylib # Apple Silicon
│   │   ├── libtml_mcp.dylib
│   │   ├── libtml_test.dylib
│   │   └── libtml_tools.dylib
│   ├── deps/                         # Bundled dylibs
│   │   ├── libcrypto.3.dylib
│   │   ├── libssl.3.dylib
│   │   ├── libzstd.1.dylib
│   │   ├── libbrotlicommon.1.dylib
│   │   ├── libbrotlidec.1.dylib
│   │   └── libbrotlienc.1.dylib
│   ├── core/...
│   ├── std/...
│   └── test/...
└── share/
    ├── doc/tml/examples/
    └── man/man1/tml.1
```

**Note:** `/usr/local/bin` is already in the default macOS PATH. No PATH modification needed.

**sqlite3:** macOS ships `/usr/lib/libsqlite3.dylib` — no need to bundle.
**zlib:** macOS ships `/usr/lib/libz.1.dylib` — no need to bundle.

### Dynamic Library Handling

macOS requires dylibs to have correct install names. Fix with `install_name_tool`:

```bash
# Fix the rpath for bundled dylibs
for dylib in staging/lib/tml/deps/*.dylib; do
    install_name_tool -id "@rpath/$(basename $dylib)" "$dylib"
done

# Set rpath on main binary
install_name_tool -add_rpath "@executable_path/../lib/tml/deps" staging/bin/tml
install_name_tool -add_rpath "@executable_path/../lib/tml/plugins" staging/bin/tml
```

### postinstall Script

```bash
#!/bin/bash
# Verify installation
if /usr/local/bin/tml --version > /dev/null 2>&1; then
    echo "TML installed successfully. Run 'tml --version' to verify."
fi
```

### Universal Binary (arm64 + x86_64)

For the main binaries, create universal (fat) binaries:

```bash
# Build for both architectures
cmake -B build-x86 -DCMAKE_OSX_ARCHITECTURES=x86_64 ...
cmake -B build-arm -DCMAKE_OSX_ARCHITECTURES=arm64 ...
cmake --build build-x86 --config Release
cmake --build build-arm --config Release

# Create universal binary
lipo -create build-x86/bin/tml build-arm/bin/tml -output staging/bin/tml
lipo -create build-x86/bin/tml-mcp build-arm/bin/tml-mcp -output staging/bin/tml-mcp
```

**Exception:** Plugin DLLs are architecture-specific (they contain LLVM backend code for the target). Ship both:
- `plugins/libtml_codegen_x86.dylib` — for Intel targets (cross-compile on ARM too)
- `plugins/libtml_codegen_arm64.dylib` — for ARM targets

## 2. Homebrew Formula (P1)

### Formula: `tml.rb`

```ruby
class Tml < Formula
  desc "TML compiler — systems programming language for LLM code generation"
  homepage "https://github.com/hivellm/tml"
  url "https://github.com/hivellm/tml/releases/download/v0.2.9/tml-0.2.9-source.tar.gz"
  sha256 "..."
  license "Apache-2.0"

  depends_on "cmake" => :build
  depends_on "openssl@3"
  depends_on "sqlite"
  depends_on "zstd"
  depends_on "brotli"

  # Or use pre-built bottles (faster):
  bottle do
    sha256 cellar: :any, arm64_sonoma: "..."
    sha256 cellar: :any, sonoma: "..."
    sha256 cellar: :any, ventura: "..."
  end

  def install
    system "scripts/build.sh", "release"
    bin.install "build/release/bin/tml"
    bin.install "build/release/bin/tml-mcp" => "tml-mcp"
    bin.install "build/release/bin/tml-daemon" => "tml-daemon"
    (lib/"tml/plugins").install Dir["build/release/bin/plugins/*.dylib"]
    (lib/"tml").install "lib/core", "lib/std", "lib/test"
    doc.install "docs/examples"
    man1.install "docs/man/tml.1" if File.exist?("docs/man/tml.1")
  end

  test do
    (testpath/"hello.tml").write <<~EOS
      func main() {
          println("Hello from TML!")
      }
    EOS
    assert_match "Hello from TML!", shell_output("#{bin}/tml run hello.tml")
  end
end
```

### Homebrew Tap (Initial)

Before acceptance into homebrew-core, host in a custom tap:

```bash
# Create tap repository: hivellm/homebrew-tml
brew tap hivellm/tml
brew install hivellm/tml/tml
```

### Homebrew Cask (Alternative — for .pkg)

If we distribute pre-built .pkg files:

```ruby
cask "tml" do
  version "0.2.9"
  sha256 "..."

  url "https://github.com/hivellm/tml/releases/download/v#{version}/TML-#{version}-macos.pkg"
  name "TML"
  desc "Systems programming language compiler"
  homepage "https://github.com/hivellm/tml"

  pkg "TML-#{version}-macos.pkg"

  uninstall pkgutil: "com.hivellm.tml"
end
```

## 3. Gatekeeper & Notarization

macOS requires signed and notarized software to run without security warnings.

See [05-code-signing.md](05-code-signing.md) for full details.

Quick summary:
1. Sign all binaries and dylibs with Developer ID certificate
2. Notarize the .pkg with `notarytool`
3. Staple the notarization ticket to the .pkg
4. Users can install without "unidentified developer" warnings

**Without signing:** Users must right-click → Open → confirm security dialog. Unacceptable for a professional tool.
