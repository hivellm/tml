# Distribution Artifacts — Per Platform

## Windows x64

### Core (Required)

```
tml/
├── bin/
│   ├── tml.exe                    # Thin launcher (~750 KB)
│   ├── tml_mcp.exe                # MCP server (~3 MB)
│   ├── tml_daemon.exe             # Daemon supervisor (~700 KB)
│   ├── plugins/
│   │   ├── tml_compiler.dll       # Compiler core (~78 MB, ~20 MB packed)
│   │   ├── tml_codegen_x86.dll    # x86 backend + LLD (~61 MB, ~15 MB packed)
│   │   ├── tml_mcp.dll            # MCP plugin (~12 KB)
│   │   ├── tml_test.dll           # Test plugin (~42 KB)
│   │   ├── tml_tools.dll          # Tools plugin (~12 KB)
│   │   └── manifest.json          # SHA256 hashes for integrity
│   ├── libcrypto-3-x64.dll        # OpenSSL crypto (4.5 MB)
│   ├── libssl-3-x64.dll           # OpenSSL TLS (804 KB)
│   ├── sqlite3.dll                # SQLite (1.1 MB)
│   ├── zlib1.dll                  # zlib (88 KB)
│   ├── zstd.dll                   # Zstandard (637 KB)
│   ├── brotlicommon.dll           # Brotli (135 KB)
│   ├── brotlidec.dll              # Brotli decoder (50 KB)
│   └── brotlienc.dll              # Brotli encoder (3.2 MB)
├── lib/
│   ├── core/                      # Core library (~5 MB, ~2200 .tml files)
│   │   └── src/...
│   ├── std/                       # Standard library (~15 MB, ~1200 .tml files)
│   │   └── src/...
│   └── test/                      # Test framework (~2 MB)
│       └── src/...
├── docs/
│   ├── examples/                  # Example programs (15 files)
│   └── readme.md                  # Language quick reference
├── LICENSE                        # Apache 2.0
└── CHANGELOG.md                   # Version history
```

### Optional Components

```
tml/
├── debug/
│   ├── tml.pdb                    # Debug symbols (~7 MB)
│   ├── tml_mcp.pdb                # MCP debug symbols (~30 MB)
│   └── plugins/
│       ├── tml_compiler.pdb       # Compiler debug symbols (~165 MB)
│       └── tml_codegen_x86.pdb    # Codegen debug symbols (~120 MB)
├── lib/
│   ├── chart/                     # Chart library (requires gnuplot)
│   │   ├── src/...
│   │   └── native/...
│   └── postgresql/                # PostgreSQL driver (requires libpq)
│       ├── src/...
│       └── native/...
└── tools/
    ├── tree-sitter-tml/           # Editor grammar
    └── vscode-tml/                # VS Code extension (.vsix)
```

### Size Estimates (Windows)

| Component | Raw Size | Packed (zstd) |
|-----------|----------|---------------|
| Binaries (exe + dll) | ~150 MB | ~45 MB |
| vcpkg DLLs | ~10 MB | ~4 MB |
| Standard library | ~22 MB | ~5 MB |
| Docs + examples | ~1 MB | ~0.5 MB |
| **Total installer** | **~183 MB** | **~55 MB** |
| Debug symbols (optional) | ~330 MB | ~80 MB |

## Linux x64

```
tml/
├── bin/
│   ├── tml                        # Main binary (stripped)
│   ├── tml-mcp                    # MCP server
│   ├── tml-daemon                 # Daemon
│   └── plugins/
│       ├── libtml_compiler.so     # Compiler
│       ├── libtml_codegen_x86.so  # x86 backend
│       ├── libtml_mcp.so          # MCP
│       ├── libtml_test.so         # Test
│       └── libtml_tools.so        # Tools
├── lib/tml/
│   ├── core/...
│   ├── std/...
│   └── test/...
├── share/
│   ├── doc/tml/
│   │   ├── examples/
│   │   └── README.md
│   ├── licenses/tml/LICENSE
│   └── man/man1/tml.1
└── etc/
    └── profile.d/tml.sh           # PATH setup script
```

**Linux dependencies** (system packages, not bundled):
- `libssl3` / `libssl-dev` — OpenSSL
- `libsqlite3-0` — SQLite
- `zlib1g` — zlib
- `libzstd1` — Zstandard
- `libbrotli1` — Brotli

Alternatively, bundle `.so` files like Windows bundles DLLs (avoids version conflicts).

## macOS (arm64 + x86_64)

```
TML.pkg/
├── usr/local/bin/
│   ├── tml
│   ├── tml-mcp
│   └── tml-daemon
├── usr/local/lib/tml/
│   ├── plugins/
│   │   ├── libtml_compiler.dylib
│   │   ├── libtml_codegen_x86.dylib   # x86_64 only
│   │   ├── libtml_codegen_arm64.dylib  # arm64 only
│   │   ├── libtml_mcp.dylib
│   │   ├── libtml_test.dylib
│   │   └── libtml_tools.dylib
│   ├── core/...
│   ├── std/...
│   └── test/...
├── usr/local/share/doc/tml/
│   └── examples/
└── etc/paths.d/tml                 # PATH registration
```

**macOS considerations:**
- Universal binaries (fat Mach-O) for arm64 + x86_64 OR separate packages
- `libcrypto.3.dylib`, `libssl.3.dylib` — bundle with `@rpath`
- `sqlite3` — available in macOS system (/usr/lib/libsqlite3.dylib)
- Code signing + notarization required for Gatekeeper
- Homebrew formula as alternative distribution

## File Classification

| Category | Ship? | Notes |
|----------|-------|-------|
| Executables (tml, tml_mcp, tml_daemon) | YES | Core binaries |
| Compiler plugins (.dll/.so/.dylib) | YES | Required for compilation |
| vcpkg DLLs (openssl, zlib, etc.) | YES (Windows) | Linux/macOS can use system libs |
| Standard library (.tml source) | YES | Compiled on first use |
| Debug symbols (.pdb) | OPTIONAL | Separate download |
| Chart/PostgreSQL libraries | OPTIONAL | Separate packages |
| Editor integrations | OPTIONAL | VS Code extension, tree-sitter |
| C++ unit tests (tml_tests.exe) | NO | Development only |
| Build cache (.incr-cache/) | NO | Generated per-user |
| .tml.meta files | NO | Generated from source |
