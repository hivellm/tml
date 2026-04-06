# Proposal: Cross-Compilation Support

## Status
- **Created**: 2025-01-08
- **Status**: Draft
- **Priority**: High
- **Dependencies**: `conditional-compilation` task

## Why

TML needs Rust-style cross-compilation support, allowing compilation for different target platforms from any host OS. This enables:

1. **Build Once, Deploy Anywhere** - Compile Linux binaries on Windows, macOS binaries on Linux, etc.
2. **CI/CD Pipelines** - Build releases for all platforms from a single build server
3. **Embedded Development** - Cross-compile for ARM, RISC-V from x86 development machines
4. **WebAssembly** - Compile to WASM from any host platform
5. **Mobile Targets** - Build Android/iOS libraries from desktop systems

### Inspiration

- **Rust**: `cargo build --target x86_64-unknown-linux-gnu`
- **Go**: `GOOS=linux GOARCH=amd64 go build`
- **Clang/LLVM**: `clang --target=aarch64-linux-gnu`

## Proposed Syntax

### CLI Usage

```bash
# Basic cross-compilation
tml build file.tml --target x86_64-unknown-linux-gnu
tml build file.tml --target aarch64-apple-darwin
tml build file.tml --target wasm32-unknown-unknown

# List available targets
tml targets list

# Show target details
tml targets info x86_64-unknown-linux-gnu

# Show host triple
tml targets host

# Override linker
tml build file.tml --target x86_64-unknown-linux-gnu --linker ld.lld

# Override sysroot
tml build file.tml --target x86_64-unknown-linux-gnu --sysroot /path/to/sysroot

# Add library search paths
tml build file.tml --target x86_64-unknown-linux-gnu -L /path/to/libs
```

### Target Triple Format

Target triples follow the format: `<arch>-<vendor>-<os>-<env>`

```
x86_64-pc-windows-msvc     # Windows 64-bit (MSVC)
x86_64-unknown-linux-gnu   # Linux 64-bit (glibc)
x86_64-unknown-linux-musl  # Linux 64-bit (static musl)
aarch64-apple-darwin       # macOS Apple Silicon
aarch64-unknown-linux-gnu  # Linux ARM64
wasm32-unknown-unknown     # WebAssembly
wasm32-wasi                # WebAssembly + WASI
```

### Output Organization

Cross-compiled binaries go to target-specific directories:

```
build/
├── debug/                           # Host target (default)
│   └── myapp.exe
├── x86_64-unknown-linux-gnu/
│   ├── debug/
│   │   └── myapp
│   └── release/
│       └── myapp
├── aarch64-apple-darwin/
│   └── debug/
│       └── myapp
└── wasm32-unknown-unknown/
    └── debug/
        └── myapp.wasm
```

### Configuration File (tml.toml)

```toml
[package]
name = "myapp"
version = "0.1.0"

# Target-specific configuration
[target.x86_64-unknown-linux-gnu]
linker = "x86_64-linux-gnu-gcc"
rustflags = ["-C", "target-feature=+crt-static"]

[target.aarch64-unknown-linux-gnu]
linker = "aarch64-linux-gnu-gcc"

[target.wasm32-unknown-unknown]
# No linker needed for wasm
```

### Environment Variables

```bash
# Default target
export TML_TARGET=x86_64-unknown-linux-gnu

# Per-target linker override
export TML_LINKER_X86_64_UNKNOWN_LINUX_GNU=/usr/bin/x86_64-linux-gnu-gcc

# Per-target sysroot
export TML_SYSROOT_AARCH64_UNKNOWN_LINUX_GNU=/usr/aarch64-linux-gnu
```

## Target Tiers

### Tier 1 (Must Have)
Full support, CI-tested, guaranteed to work:
- `x86_64-pc-windows-msvc` - Windows 64-bit
- `x86_64-unknown-linux-gnu` - Linux 64-bit
- `x86_64-apple-darwin` - macOS Intel
- `aarch64-apple-darwin` - macOS Apple Silicon
- `wasm32-unknown-unknown` - WebAssembly

### Tier 2 (Should Have)
Supported, tested occasionally:
- `x86_64-unknown-linux-musl` - Static Linux
- `aarch64-unknown-linux-gnu` - Linux ARM64
- `x86_64-pc-windows-gnu` - Windows MinGW
- `wasm32-wasi` - WebAssembly + WASI

### Tier 3 (Nice to Have)
Best-effort support:
- `aarch64-linux-android` - Android ARM64
- `x86_64-unknown-freebsd` - FreeBSD
- `riscv64gc-unknown-linux-gnu` - RISC-V 64-bit

## Implementation

### Target System Enhancement

Extend existing `codegen/target.hpp`:

```cpp
struct Target {
    Arch arch;
    Vendor vendor;
    OS os;
    Env env;

    // New methods
    auto default_linker() const -> std::string;
    auto default_ar() const -> std::string;
    auto object_extension() const -> std::string;    // .o, .obj
    auto exe_extension() const -> std::string;       // .exe, empty, .wasm
    auto static_lib_extension() const -> std::string; // .a, .lib
    auto dynamic_lib_extension() const -> std::string; // .so, .dll, .dylib
    auto llvm_triple() const -> std::string;
    auto data_layout() const -> std::string;
};
```

### Toolchain Discovery

```cpp
struct Toolchain {
    std::string compiler;      // clang, gcc, etc.
    std::string linker;        // ld, lld, link.exe
    std::string archiver;      // ar, llvm-ar, lib.exe
    std::string sysroot;       // Target system root
    std::vector<std::string> lib_paths;
};

// Discover toolchain for target
auto find_toolchain(const Target& target) -> std::optional<Toolchain>;
```

### LLVM Target Configuration

```cpp
// Set in LLVM IR header
target triple = "x86_64-unknown-linux-gnu"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
```

## What Changes

### New Files

- `compiler/src/codegen/toolchain.hpp` - Toolchain discovery
- `compiler/src/codegen/toolchain.cpp` - Toolchain implementation
- `compiler/src/cli/cmd_targets.hpp` - Targets subcommand
- `compiler/src/cli/cmd_targets.cpp` - Targets implementation
- `compiler/src/cli/cmd_doctor.cpp` - Doctor command for diagnostics

### Modified Files

- `compiler/include/codegen/target.hpp` - Extend Target struct
- `compiler/src/codegen/target.cpp` - Implement new methods
- `compiler/src/cli/dispatcher.cpp` - Add --target flag, targets subcommand
- `compiler/src/cli/cmd_build.cpp` - Handle cross-compilation
- `compiler/src/codegen/llvm_ir_gen.cpp` - Use target triple/data layout

## Impact

- **Breaking change**: NO (new feature, backward compatible)
- **User benefit**: Build for any platform from any platform

## Success Criteria

1. `tml build --target x86_64-unknown-linux-gnu` compiles successfully on Windows/macOS
2. `tml build --target x86_64-pc-windows-msvc` compiles successfully on Linux/macOS
3. `tml build --target wasm32-unknown-unknown` produces valid WASM from any platform
4. `tml targets list` shows all available targets
5. `tml targets host` shows correct host triple
6. Generated binaries run correctly on target platforms
7. Output directories organized by target
8. Clear error messages when toolchain not found
9. `tml doctor` diagnoses cross-compilation issues

## References

- Rust Platform Support: https://doc.rust-lang.org/nightly/rustc/platform-support.html
- LLVM Target Triples: https://llvm.org/docs/LangRef.html#target-triple
- Clang Cross-Compilation: https://clang.llvm.org/docs/CrossCompilation.html
