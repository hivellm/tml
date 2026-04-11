# 08 — Self-Contained Toolchain Architecture

**Date**: 2026-04-05
**Status**: Complete
**Companion**: [06-hybrid-strategy.md](06-hybrid-strategy.md),
[07-c-compilation-without-clang.md](07-c-compilation-without-clang.md),
[docs/analyses/linker/04-custom-linker-design.md](../linker/04-custom-linker-design.md)

---

## Vision

```
User downloads: tml-1.0-windows-x64.zip   (~22 MB)

Extracts and runs:
  tml build app.tml   →   app.exe

No MSVC installed. No Clang installed. No Zig installed.
No Visual Studio Build Tools. No Windows SDK. No Rust.
Just tml.exe and one ZIP.
```

This is the end state of the five-phase backend elimination strategy. This document
describes the architecture of that end state in concrete terms: what files are in the
distribution, how each component is built, what interfaces connect them, and what the
bootstrap sequence looks like.

---

## 1. Distribution Layout by Phase

### Phase 1 Distribution (Current + Cranelift, ~25 MB)

Available at the end of Phase 1 of the hybrid strategy. LLVM is still present but no
longer required for basic builds.

```
tml-0.3-windows-x64.zip
├── tml.exe                         (15 MB) — launcher, query system, CLI
├── tml_compiler.dll                (31 MB) — compiler pipeline (no LLVM glue in default path)
├── tml_codegen_cranelift.dll        (5 MB) — default backend: MIR → COFF via Cranelift
├── lib/
│   ├── core/src/                          — TML core library source (.tml files)
│   ├── std/src/                           — TML std library source (.tml files)
│   └── runtime/
│       ├── windows-x64/                   — pre-compiled C runtime .obj files (~800 KB)
│       └── linux-x64/                     — pre-compiled C runtime .obj files (~650 KB)
└── LICENSE.txt

Optional separate download:
├── tml_codegen_x86.dll             (78 MB) — LLVM O3 backend for --release builds
```

User experience on Phase 1 distribution:
```
tml build hello.tml          → uses tml_codegen_cranelift.dll, fast (~100ms)
tml build hello.tml --release → error: "LLVM backend not installed, run: tml install --llvm"
tml install --llvm            → downloads tml_codegen_x86.dll (~78 MB)
tml build hello.tml --release → uses tml_codegen_x86.dll, optimal code
```

### Phase 3 Distribution (Custom Backend, ~18 MB)

Available after Phase 3 (custom x86_64 backend in TML). Cranelift removed. No Rust
runtime dependency. tml-link replaces LLD.

```
tml-0.5-windows-x64.zip
├── tml.exe                         (12 MB) — compiler + custom backend + tml-link (monolithic)
├── lib/
│   ├── core/src/                          — TML core library source (.tml files)
│   ├── std/src/                           — TML std library source (.tml files)
│   └── runtime/
│       ├── windows-x64/                   — pre-compiled C runtime .obj files (~800 KB)
│       ├── linux-x64/                     — pre-compiled C runtime .obj files (~650 KB)
│       └── macos-arm64/                   — pre-compiled C runtime .obj files (~700 KB)
└── LICENSE.txt

Optional:
├── tml_codegen_x86.dll             (78 MB) — LLVM O3 for --release (still valuable)
```

### Phase 5 Distribution (Self-Contained, ~22 MB)

Final end state. Custom linker, custom backend, optional LLVM, minimal C residue as
pre-compiled objects.

```
tml-1.0-windows-x64.zip
├── tml.exe                         (12 MB) — monolithic: compiler + backend + linker
├── lib/
│   ├── core/src/                          — TML core library source (.tml files)
│   ├── std/src/                           — TML std library source (.tml files)
│   ├── runtime/
│   │   ├── windows-x64/                   — pre-compiled C runtime .obj (~800 KB)
│   │   ├── linux-x64/                     — pre-compiled C runtime .obj (~650 KB)
│   │   └── macos-arm64/                   — pre-compiled C runtime .obj (~700 KB)
│   └── win32/
│       └── import-stubs/                  — Win32 .lib import stubs (~200 KB, replaces MSVC SDK)
└── LICENSE.txt
```

Cross-compilation targets are available as additional downloads (~700 KB each for the
runtime objects + any target-specific linker tables).

---

## 2. Component Architecture

### 2.1 tml.exe — The Monolithic Launcher

In Phase 5, `tml.exe` is a monolithic binary containing:
- The entire compiler pipeline (Lexer → Parser → TypeChecker → HIR → MIR)
- The query and incremental compilation system
- The custom x86_64 backend (MIR → COFF)
- tml-link (COFF linker, PE/COFF/ELF writer)
- The CLI and build system interface

This contrasts with the current architecture where `tml.exe` is a thin launcher that
loads `tml_compiler.dll` and `tml_codegen_x86.dll`. The monolithic design is used for
the self-contained distribution because it eliminates DLL loading and path resolution.

The plugin architecture (DLLs) is retained for the developer workflow, where separate
DLLs allow incremental rebuilds without relinking the entire compiler binary.

### 2.2 Custom Backend Interface

The custom backend implements the existing `CodegenBackend` interface:

```cpp
// compiler/include/codegen/backend.hpp  (existing interface, unchanged)
class CodegenBackend {
public:
    virtual ~CodegenBackend() = default;

    // Compile a MIR module to a native object file
    // Returns the object bytes in COFF (Windows) or ELF (Linux) format
    virtual auto compile_module(
        const mir::Module& module,
        const CodegenOptions& opts
    ) -> std::expected<std::vector<uint8_t>, CompileError> = 0;

    // Query backend capabilities
    virtual auto capabilities() const -> BackendCapabilities = 0;
};
```

The custom backend (written in TML, called via FFI) implements this interface:

```
CustomBackend::compile_module(mir::Module)
  │
  ├── InstructionSelector::lower(mir::BasicBlock[])
  │     Maps each MIR instruction to one or more x86_64 instructions
  │     Output: virtual-register instruction stream
  │
  ├── RegisterAllocator::allocate(instruction_stream)
  │     Maps virtual registers to physical registers (rax, rbx, ..., xmm0-15)
  │     Uses linear scan algorithm (simple, ~85% quality vs optimal)
  │     Output: physical-register instruction stream
  │
  ├── InstructionScheduler::schedule(instruction_stream)
  │     (Phase 3g and later) Reorders instructions to avoid pipeline stalls
  │     Output: scheduled instruction stream
  │
  ├── CodeEmitter::emit(instruction_stream)
  │     Encodes each x86_64 instruction to bytes
  │     Manages relocations (calls to external functions, global data references)
  │     Output: raw machine code bytes + relocation table
  │
  └── ObjectWriter::write_coff(code, relocations, symbols)
        Produces a valid COFF .obj file from the raw machine code
        Handles: section headers, symbol table, string table, relocation records
        Output: Vec<U8> (COFF .obj bytes)
```

### 2.3 tml-link — The Custom PE/COFF Linker

The linker design is documented in `docs/analyses/linker/04-custom-linker-design.md`.
From the perspective of the self-contained toolchain, tml-link is invoked with:

**Input:**
- `Vec<ObjectBytes>` from the compiler backend (already in memory, no disk I/O)
- `Vec<RuntimeObjectBytes>` from the pre-compiled runtime bundle (loaded at startup)
- `Vec<ImportLib>` from the Win32 import stubs directory

**Output:**
- `Vec<u8>` PE bytes (written to disk as the final `.exe` or `.dll`)

**Interface (from the compiler's perspective):**

```cpp
// compiler/include/backend/linker_interface.hpp

struct LinkInput {
    std::vector<std::span<const uint8_t>> compilation_units; // from backend
    std::vector<std::span<const uint8_t>> runtime_objects;   // from pre-compiled bundle
    std::vector<std::filesystem::path>    import_libraries;  // Win32 .lib stubs
    std::string                           output_path;
    LinkOutputType                        output_type;        // Exe, Dll, StaticLib
    std::string                           entry_point;
};

struct LinkResult {
    std::vector<uint8_t> bytes;   // non-empty on success
    std::string          error;   // non-empty on failure
};

auto tml_link(const LinkInput& input) -> LinkResult;
```

### 2.4 Win32 Import Stubs

MSVC-style `.lib` import libraries are required to link programs that call Win32 APIs
(`kernel32.dll`, `ntdll.dll`, `ws2_32.dll`, etc.). These `.lib` files are normally
shipped with the Windows SDK (~4 GB). The self-contained toolchain uses a minimal
alternative.

The `windows-rs` project (Microsoft, MIT license) ships a `windows-bindgen` tool that
generates bindings from the official Win32 metadata (`.winmd` files). The same metadata
can be used to generate minimal import libraries:

```
windows.win32.winmd  →  gen-import-libs  →  kernel32.lib (~50 KB)
                                         →  ntdll.lib    (~30 KB)
                                         →  ws2_32.lib   (~20 KB)
                                         →  ... (40 libs total, ~200 KB)
```

The `gen-import-libs` tool is a ~200-line C++ program that:
1. Reads the `.winmd` binary format (documented in ECMA-335)
2. Extracts function names and DLL names for each exported symbol
3. Writes COFF import library archives (one `.lib` per DLL)

These generated `.lib` files are checked into `lib/win32/import-stubs/` and never need
to be regenerated unless the Windows API surface changes (which is rare for stable APIs
like `CreateFile`, `WSAStartup`, etc.).

---

## 3. Bootstrap Problem and Solution

### The Problem

To build `tml.exe` (the self-hosting compiler), you need a compiler. The TML compiler
cannot compile itself until the custom backend is complete (Phase 3). Before that, the
C++ compiler (built with Zig CC) is used.

### The Solution: Three-Stage Bootstrap

This is the same approach used by Go (which bootstrapped from C to Go in Go 1.5) and
Rust (which bootstrapped from OCaml to Rust).

```
Stage 0: C++ tml.exe (current — always available as a binary)
  Purpose: builds everything else
  Built by: Zig CC + CMake (developer workflow only)
  Ships as: tml-bootstrap.exe in the repository

Stage 1: C++ tml.exe compiles TML source → native objects → links → new tml.exe
  Input: compiler written in TML (lib/compiler/)
  Backend: C++ custom backend (Phase 3a-3e)
  Output: tml_v2.exe (TML-compiled, C++ backend)
  Verification: tml_v2.exe produces identical output to C++ tml.exe for a test corpus

Stage 2: tml_v2.exe compiles TML source → native objects → links → tml_v3.exe
  Input: same compiler written in TML
  Backend: TML-written custom backend (Phase 3 complete)
  Output: tml_v3.exe (fully self-hosting)
  Verification: tml_v3.exe compiles itself to produce tml_v4.exe
                tml_v3.exe and tml_v4.exe are byte-for-byte identical → self-hosting confirmed

Stage 3: tml_v3.exe IS the shipped binary
  Shipped as: tml.exe in the release distribution
  Bootstrap binary: tml_v3.exe is checked into the repository as the new bootstrap
```

### What Needs to Be True for Stage 2

For the compiler to be fully self-hosting, ALL of the following must be written in TML:
- Lexer, Parser, Type Checker, HIR, MIR builders — already being migrated
- Query system and incremental cache — already being migrated
- Custom x86_64 backend — Phase 3 of this analysis
- tml-link — Phase 3 of the linker analysis
- CLI and build system interface — already being migrated

The only C that remains is the ~9,881 LOC OS-interface residue from
`07-c-compilation-without-clang.md`, which is pre-compiled and linked in at the end.
The TML compiler does not need to compile this C — it is already in object form.

### Bootstrap Binary Policy

```
Every TML release includes:
  tml-bootstrap-X.Y.Z-windows-x64.exe    (C++ built, for Stage 0)
  tml-X.Y.Z-windows-x64.exe              (self-hosted, for users)

Developers who want to build from source run:
  scripts/bootstrap.bat      # uses tml-bootstrap.exe to build tml.exe
  scripts/build.bat          # uses the self-hosted tml.exe for incremental rebuilds
```

---

## 4. Cross-Compilation

After Phase 5, `tml build` supports cross-compilation to any target for which runtime
objects and a linker backend are available.

### Target Triple Support

| Target | Backend | Linker Output | Runtime Objects | Status |
|--------|---------|--------------|-----------------|--------|
| `windows-x64` | Custom (x86_64) | PE/COFF (.exe, .dll) | Bundled | Phase 5 |
| `linux-x64` | Custom (x86_64) | ELF (.out, .so) | Bundled | Phase 5 |
| `macos-arm64` | Cranelift (ARM64) | Mach-O (.dylib) | Bundled | Phase 5+ |
| `windows-arm64` | Cranelift (ARM64) | PE/COFF (.exe) | Downloadable | Phase 5+ |
| `linux-arm64` | Cranelift (ARM64) | ELF | Downloadable | Phase 5+ |
| `wasi-wasm32` | LLVM (required) | WASM module | N/A | No timeline |

### Cross-Compilation Workflow

```
# On Windows, build for Linux x64:
tml build app.tml --target=linux-x64 --output=app

# tml.exe does:
# 1. Compile TML source → MIR (same for all targets)
# 2. Run custom backend (x86_64) with linux-x64 ABI (ELF calling convention)
# 3. Link against lib/runtime/linux-x64/*.o and linux import stubs
# 4. Write ELF output → app (no extension, ELF convention)
```

The same MIR intermediate representation is used for all targets. Only the backend
(instruction selection + ABI) and linker (COFF vs ELF vs Mach-O) are target-specific.

### Per-Target Runtime Objects

Each target ships a small bundle of pre-compiled runtime objects:

```
lib/runtime/
├── windows-x64/
│   ├── manifest.json             (SHA-256 hashes of each .obj)
│   ├── essential.obj             (console I/O, panic)
│   ├── mem.obj                   (malloc/free)
│   ├── net_win.obj               (IOCP, Winsock2)
│   ├── crypto_win.obj            (BCrypt, OpenSSL)
│   └── concurrency_win.obj       (CRITICAL_SECTION, CreateThread)
├── linux-x64/
│   ├── manifest.json
│   ├── essential.o               (write(2), _exit(2))
│   ├── mem.o                     (malloc/free)
│   ├── net_linux.o               (epoll, socket(2))
│   ├── crypto_linux.o            (OpenSSL)
│   └── concurrency_linux.o       (pthread_mutex, sem_post)
└── macos-arm64/
    └── ...
```

The `manifest.json` lists the expected SHA-256 hash of each object. `tml.exe` verifies
hashes at startup to detect corrupted or replaced runtime objects.

---

## 5. Dependency Elimination Checklist

The following table tracks which external tools are eliminated at each phase.

| Dependency | Role | After Phase 1 | After Phase 2 | After Phase 3 | After Phase 5 |
|-----------|------|--------------|--------------|--------------|--------------|
| LLVM (78 MB) | Code generation | Optional (--release) | Optional (--release) | Optional (--release) | Optional |
| LLD linker | Linking | Default | Default | Replaced by tml-link | Eliminated |
| Zig CC | C runtime compilation | Dev only | Dev only | Dev only | Dev only |
| MSVC / Clang | (not used) | Not needed | Not needed | Not needed | Not needed |
| Windows SDK | Import libraries | Via LLD | Via LLD | Via Win32 stubs | Eliminated |
| Rust toolchain | Cranelift build dep | Dev only | Dev only | Eliminated | Eliminated |
| CMake | Build system | Dev only | Dev only | Dev only | Dev only |

"Dev only" means the dependency is needed to build the compiler from source, but not to
use the compiler. End users never encounter it.

After Phase 5, the only remaining external dependencies for **end users** are:
- The operating system itself (Windows, Linux, macOS)
- An internet connection to download the `tml-1.0-windows-x64.zip` file (one time)

---

## 6. Security Considerations

### Runtime Object Integrity

Pre-compiled runtime objects are high-value targets: replacing them with malicious objects
would allow arbitrary code execution in any program compiled with TML. Mitigations:

1. **SHA-256 manifest**: Each runtime bundle includes `manifest.json` with hashes of every
   object file. `tml.exe` verifies all hashes at startup. A mismatch halts compilation with
   a clear error.
2. **Code signing**: Release builds of `tml.exe` and all DLLs are Authenticode-signed.
   The runtime objects are bundled inside the signed zip, covered by the zip hash.
3. **No network access for runtime**: `tml.exe` never fetches runtime objects from the
   network during compilation. Network access is only used by `tml install` (explicit
   opt-in by the user).

### Supply Chain

All components of the self-contained toolchain have auditable sources:
- TML compiler: source available in this repository
- Cranelift: source available at github.com/bytecodealliance/wasmtime (Apache 2.0)
- Custom backend: source in TML (in this repository)
- Win32 import stubs: generated from windows.win32.winmd (Microsoft, MIT)
- TCC (optional): source at github.com/TinyCC/tinycc (LGPL 2.1)
- Pre-compiled runtime: source in `compiler/runtime/` (this repository)

---

## 7. Size Budget

### Phase 5 Target: Under 25 MB Uncompressed, Under 8 MB Compressed

The target is motivated by practical distribution: 25 MB uncompressed is fast to extract,
and 8 MB compressed is fast to download even on slow connections. Comparison:

| Tool | Download Size | Notes |
|------|-------------|-------|
| TML 1.0 (target) | ~8 MB compressed | ~22 MB uncompressed |
| Go 1.22 toolchain | ~65 MB | Includes std library source |
| Zig 0.13 toolchain | ~50 MB | Includes LLVM backend |
| Rust minimal | ~10 MB | Core only, no std |
| Deno 1.x | ~90 MB | Includes V8 |
| Node.js 20 LTS | ~25 MB | Runtime only, no compiler |

### Component Size Budget

| Component | Target Size | Notes |
|-----------|------------|-------|
| Compiler pipeline (Lexer → MIR) | 3-4 MB | Type checker, HIR, MIR builders |
| Custom x86_64 backend | 1-2 MB | Instruction selection, regalloc, emitter |
| tml-link (PE/COFF/ELF writer) | 0.5-1 MB | Custom linker |
| Query and CLI | 1-2 MB | Incremental cache, build system |
| Runtime objects (windows-x64) | ~800 KB | Pre-compiled C runtime |
| TML standard library source | ~2-3 MB | .tml files, compressed |
| Win32 import stubs | ~200 KB | Generated import libraries |
| **Total** | **~10-13 MB** | Before compression |

Aggressive compression (zstd level 15) achieves 2.5-3x ratio on mixed binary+text
archives of this type. Target: ~5-6 MB compressed for the minimal distribution.

The LLVM optional download (~78 MB) is excluded from this budget — it is for users who
need maximum optimization and have already committed to a larger toolchain.
