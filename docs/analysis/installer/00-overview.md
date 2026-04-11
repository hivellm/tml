# TML Installer & Distribution System — Analysis

**Date**: 2026-04-05
**Version**: 0.2.9
**Status**: Planning

## Goal

Create a professional cross-platform installer that allows a new user to:

1. Download a single installer for their platform
2. Run it, get `tml` available in their terminal
3. Start writing and running TML code immediately

No manual PATH editing, no dependency hunting, no build-from-source.

## Document Index

| Document | Contents |
|----------|----------|
| [01-distribution-artifacts.md](01-distribution-artifacts.md) | What files to ship per platform |
| [02-windows-msi.md](02-windows-msi.md) | Windows MSI installer (WiX) |
| [03-linux-packages.md](03-linux-packages.md) | Linux .deb, .rpm, .tar.gz, install script |
| [04-macos-package.md](04-macos-package.md) | macOS .pkg, Homebrew formula |
| [05-code-signing.md](05-code-signing.md) | Executable signing for Windows, macOS, Linux |
| [06-ci-cd-pipeline.md](06-ci-cd-pipeline.md) | GitHub Actions release pipeline |
| [07-implementation-plan.md](07-implementation-plan.md) | Phased task breakdown |

## Current State

### Build Outputs (Release)

| File | Size (est.) | Purpose |
|------|------------|---------|
| `tml.exe` | ~750 KB | Thin launcher |
| `tml_mcp.exe` | ~3 MB | MCP server |
| `tml_daemon.exe` | ~700 KB | Daemon supervisor |
| `plugins/tml_compiler.dll` | ~78 MB | Compiler core |
| `plugins/tml_codegen_x86.dll` | ~61 MB | x86 codegen + LLD |
| `plugins/tml_mcp.dll` | ~12 KB | MCP plugin |
| `plugins/tml_test.dll` | ~42 KB | Test plugin |
| `plugins/tml_tools.dll` | ~12 KB | Tools plugin |
| `libcrypto-3-x64.dll` | 4.5 MB | OpenSSL crypto |
| `libssl-3-x64.dll` | 804 KB | OpenSSL TLS |
| `sqlite3.dll` | 1.1 MB | SQLite |
| `zlib1.dll` | 88 KB | zlib |
| `zstd.dll` | 637 KB | Zstandard |
| `brotlicommon.dll` | 135 KB | Brotli |
| `brotlidec.dll` | 50 KB | Brotli decoder |
| `brotlienc.dll` | 3.2 MB | Brotli encoder |
| `lib/core/` | ~5 MB | Core library (.tml) |
| `lib/std/` | ~15 MB | Standard library (.tml) |
| `lib/test/` | ~2 MB | Test framework (.tml) |
| **Total (compressed)** | **~50-60 MB** | With zstd plugin packing |

### Key Constraints

1. **LLVM is statically linked** — no LLVM DLLs to ship (they're inside tml_compiler.dll and tml_codegen_x86.dll)
2. **Plugin packing** — `--pack` compresses DLLs with zstd to ~25% size, with SHA256 manifest
3. **vcpkg dependencies** — OpenSSL, zlib, brotli, zstd, sqlite3 ship as DLLs
4. **Standard library is source** — .tml files ship as-is, compiled on first use (cached in .incr-cache/)
5. **No .NET/Java/Python runtime** — TML compiles to native code, zero runtime dependencies beyond OS + shipped DLLs
