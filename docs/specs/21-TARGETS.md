# TML v1.0 — Cross-Compilation Targets

## 1. Overview

TML supports cross-compilation to multiple platforms. The compiler uses LLVM as the backend, enabling support for all LLVM targets.

## 2. Target Triple Format

```
<arch>-<vendor>-<os>-<env>

Examples:
x86_64-unknown-linux-gnu
x86_64-pc-windows-msvc
aarch64-apple-darwin
wasm32-unknown-unknown
```

## 3. Tier 1 Targets (Primary Support)

These targets are fully supported with complete testing.

### 3.1 x86_64-unknown-linux-gnu

```toml
[target.x86_64-unknown-linux-gnu]
arch = "x86_64"
os = "linux"
env = "gnu"
pointer_width = 64
endian = "little"
features = ["sse2", "sse4.2", "avx"]
linker = "gcc"
ar = "ar"

[target.x86_64-unknown-linux-gnu.libs]
dynamic = [".so"]
static = [".a"]
system = ["c", "m", "pthread", "dl", "rt"]
```

### 3.2 x86_64-pc-windows-msvc

```toml
[target.x86_64-pc-windows-msvc]
arch = "x86_64"
os = "windows"
env = "msvc"
pointer_width = 64
endian = "little"
features = ["sse2"]
linker = "link.exe"
ar = "lib.exe"

[target.x86_64-pc-windows-msvc.libs]
dynamic = [".dll"]
static = [".lib"]
system = ["kernel32", "user32", "advapi32", "ws2_32", "bcrypt"]
```

### 3.3 x86_64-apple-darwin

```toml
[target.x86_64-apple-darwin]
arch = "x86_64"
os = "macos"
env = "none"
pointer_width = 64
endian = "little"
features = ["sse2", "sse4.2"]
linker = "clang"
ar = "ar"
min_os_version = "10.12"

[target.x86_64-apple-darwin.libs]
dynamic = [".dylib"]
static = [".a"]
system = ["System"]
frameworks = ["CoreFoundation", "Security"]
```

### 3.4 aarch64-apple-darwin (Apple Silicon)

```toml
[target.aarch64-apple-darwin]
arch = "aarch64"
os = "macos"
env = "none"
pointer_width = 64
endian = "little"
features = ["neon", "fp-armv8"]
linker = "clang"
ar = "ar"
min_os_version = "11.0"

[target.aarch64-apple-darwin.libs]
dynamic = [".dylib"]
static = [".a"]
system = ["System"]
frameworks = ["CoreFoundation", "Security"]
```

## 4. Tier 2 Targets (Full Support)

These targets are supported with regular testing.

### 4.1 aarch64-unknown-linux-gnu

```toml
[target.aarch64-unknown-linux-gnu]
arch = "aarch64"
os = "linux"
env = "gnu"
pointer_width = 64
endian = "little"
features = ["neon"]
linker = "aarch64-linux-gnu-gcc"
ar = "aarch64-linux-gnu-ar"
```

### 4.2 x86_64-unknown-linux-musl

```toml
[target.x86_64-unknown-linux-musl]
arch = "x86_64"
os = "linux"
env = "musl"
pointer_width = 64
endian = "little"
linker = "musl-gcc"
static_only = true  # Produces fully static binaries
```

### 4.3 i686-pc-windows-msvc

```toml
[target.i686-pc-windows-msvc]
arch = "i686"
os = "windows"
env = "msvc"
pointer_width = 32
endian = "little"
linker = "link.exe"
```

### 4.4 i686-unknown-linux-gnu

```toml
[target.i686-unknown-linux-gnu]
arch = "i686"
os = "linux"
env = "gnu"
pointer_width = 32
endian = "little"
linker = "gcc"
cflags = ["-m32"]
```

## 5. Tier 3 Targets (Experimental)

These targets are experimental with limited testing.

### 5.1 wasm32-unknown-unknown

```toml
[target.wasm32-unknown-unknown]
arch = "wasm32"
os = "none"
env = "none"
pointer_width = 32
endian = "little"
features = ["bulk-memory", "mutable-globals", "sign-ext"]
linker = "wasm-ld"
no_std = true

[target.wasm32-unknown-unknown.output]
extension = ".wasm"
format = "wasm"
```

### 5.2 wasm32-wasi

```toml
[target.wasm32-wasi]
arch = "wasm32"
os = "wasi"
env = "none"
pointer_width = 32
endian = "little"
features = ["bulk-memory"]
linker = "wasm-ld"
sysroot = "${WASI_SDK}/share/wasi-sysroot"

[target.wasm32-wasi.libs]
system = ["wasi-emulated-mman", "wasi-emulated-signal"]
```

### 5.3 aarch64-unknown-linux-musl

```toml
[target.aarch64-unknown-linux-musl]
arch = "aarch64"
os = "linux"
env = "musl"
pointer_width = 64
endian = "little"
linker = "aarch64-linux-musl-gcc"
static_only = true
```

### 5.4 riscv64gc-unknown-linux-gnu

```toml
[target.riscv64gc-unknown-linux-gnu]
arch = "riscv64"
os = "linux"
env = "gnu"
pointer_width = 64
endian = "little"
features = ["m", "a", "f", "d", "c"]
linker = "riscv64-linux-gnu-gcc"
```

## 6. Cross-Compilation Setup

### 6.1 Configuration File

```toml
# ~/.tml/config.toml or project tml.toml

[build]
target = "x86_64-unknown-linux-gnu"  # Default target

[target.aarch64-unknown-linux-gnu]
linker = "aarch64-linux-gnu-gcc"
ar = "aarch64-linux-gnu-ar"
sysroot = "/usr/aarch64-linux-gnu"

[target.x86_64-pc-windows-gnu]
linker = "x86_64-w64-mingw32-gcc"
ar = "x86_64-w64-mingw32-ar"
```

### 6.2 Building for Different Targets

```bash
# Build for current platform
tml build

# Build for specific target
tml build --target x86_64-pc-windows-msvc

# Build for multiple targets
tml build --target x86_64-unknown-linux-gnu --target aarch64-apple-darwin

# List available targets
tml target list

# Add target support
tml target add aarch64-unknown-linux-gnu
```

### 6.3 Target-Specific Code

```tml
module platform

@when(target_os = "linux")
public func get_home_dir() -> Maybe[PathBuf] {
    when env.var("HOME") {
        Ok(val) -> Just(PathBuf.from(val)),
        Err(_) -> Nothing,
    }
}

@when(target_os = "windows")
public func get_home_dir() -> Maybe[PathBuf] {
    when env.var("USERPROFILE") {
        Ok(val) -> Just(PathBuf.from(val)),
        Err(_) -> Nothing,
    }
}

@when(target_os = "macos")
public func get_home_dir() -> Maybe[PathBuf] {
    when env.var("HOME") {
        Ok(val) -> Just(PathBuf.from(val)),
        Err(_) -> Nothing,
    }
}

@when(target_arch = "x86_64")
public func fast_memcpy(dst: *mut U8, src: *const U8, len: U64) {
    // Use AVX if available
    if cpu_has_avx() {
        avx_memcpy(dst, src, len)
    } else {
        default_memcpy(dst, src, len)
    }
}

@when(target_arch = "aarch64")
public func fast_memcpy(dst: *mut U8, src: *const U8, len: U64) {
    // Use NEON
    neon_memcpy(dst, src, len)
}
```

## 7. Platform Detection

### 7.1 Available @when Predicates

```tml
// Operating system
@when(target_os = "linux")
@when(target_os = "windows")
@when(target_os = "macos")
@when(target_os = "ios")
@when(target_os = "android")
@when(target_os = "freebsd")
@when(target_os = "none")  // bare metal / wasm

// CPU architecture
@when(target_arch = "x86_64")
@when(target_arch = "aarch64")
@when(target_arch = "i686")
@when(target_arch = "arm")
@when(target_arch = "wasm32")
@when(target_arch = "riscv64")

// Pointer width
@when(target_pointer_width = "32")
@when(target_pointer_width = "64")

// Endianness
@when(target_endian = "little")
@when(target_endian = "big")

// Environment
@when(target_env = "gnu")
@when(target_env = "msvc")
@when(target_env = "musl")

// Vendor
@when(target_vendor = "apple")
@when(target_vendor = "pc")
@when(target_vendor = "unknown")

// CPU features
@when(target_feature = "sse2")
@when(target_feature = "avx")
@when(target_feature = "neon")

// Family
@when(unix)     // linux, macos, bsd, etc.
@when(windows)
```

### 7.2 Runtime Detection

```tml
import std.arch

public func optimal_algorithm() {
    if arch.is_x86_feature_detected("avx2") {
        avx2_impl()
    } else if arch.is_x86_feature_detected("sse4.2") {
        sse42_impl()
    } else {
        generic_impl()
    }
}
```

## 8. Binary Formats

### 8.1 Executable Formats

| Platform | Format | Extension |
|----------|--------|-----------|
| Linux | ELF | (none) |
| Windows | PE/COFF | .exe |
| macOS | Mach-O | (none) |
| WebAssembly | Wasm | .wasm |

### 8.2 Library Formats

| Platform | Static | Dynamic |
|----------|--------|---------|
| Linux | .a | .so |
| Windows | .lib | .dll |
| macOS | .a | .dylib |
| WebAssembly | N/A | .wasm |

## 9. Toolchain Requirements

### 9.1 Linux Host

```bash
# For native compilation
sudo apt install build-essential

# For Windows cross-compilation
sudo apt install mingw-w64

# For ARM64 cross-compilation
sudo apt install gcc-aarch64-linux-gnu

# For WebAssembly
curl -L https://github.com/aspect-build/basm/releases/download/wasm32-wasi-sysroot/... | tar xz
```

### 9.2 Windows Host

```powershell
# Install Visual Studio Build Tools
winget install Microsoft.VisualStudio.2022.BuildTools

# For Linux cross-compilation (via WSL2)
wsl --install -d Ubuntu

# For WebAssembly
winget install aspect.basm
```

### 9.3 macOS Host

```bash
# Install Xcode Command Line Tools
xcode-select --install

# For Linux cross-compilation
brew install FiloSottile/musl-cross/musl-cross

# For Windows cross-compilation
brew install mingw-w64

# For WebAssembly
brew install aspect-build/basm/basm
```

## 10. Platform-Specific Notes

### 10.1 Linux

```tml
// Dynamic linking (default)
// Binary depends on libc, libm, libpthread, etc.

// Static linking with musl
// tml build --target x86_64-unknown-linux-musl
// Produces fully static binary, portable across Linux distros
```

### 10.2 Windows

```tml
// MSVC toolchain (default, recommended)
// Requires Visual Studio Build Tools

// MinGW toolchain (alternative)
// tml build --target x86_64-pc-windows-gnu
// Uses GCC-based toolchain, different runtime

// Windows subsystem
@when(windows)
@subsystem("windows")  // GUI app, no console
// or
@subsystem("console")  // Console app (default)
```

### 10.3 macOS

```tml
// Universal binaries (fat binaries)
// tml build --target universal-apple-darwin
// Creates binary for both x86_64 and aarch64

// Minimum deployment target
@when(target_os = "macos")
@macos_deployment_target("11.0")  // Big Sur minimum

// Code signing (required for distribution)
// codesign --sign "Developer ID" target/release/myapp
```

### 10.4 WebAssembly

```tml
// Standalone WASM (no_std)
module wasm_app
@no_std

@export("add")
public func add(a: I32, b: I32) -> I32 {
    return a + b
}

// WASI (standard I/O, file system, etc.)
module wasi_app
caps: [io.file, io.process.env]

public func main() {
    println("Hello from WASI!")
}
```

## 11. Build Profiles

### 11.1 Debug Profile

```toml
[profile.debug]
opt_level = 0
debug = true
overflow_checks = true
lto = false
panic = "unwind"
```

### 11.2 Release Profile

```toml
[profile.release]
opt_level = 3
debug = false
overflow_checks = false
lto = true
panic = "abort"  # Smaller binary
strip = true     # Remove symbols
```

### 11.3 Release with Debug

```toml
[profile.release-debug]
inherits = "release"
debug = true
strip = false
```

### 11.4 Minimum Size

```toml
[profile.min-size]
opt_level = "z"  # Optimize for size
lto = true
panic = "abort"
strip = true
codegen_units = 1
```

## 12. Distribution

### 12.1 Linux Distribution

```bash
# Static binary (portable)
tml build --release --target x86_64-unknown-linux-musl

# Package as .deb
tml package --format deb

# Package as .rpm
tml package --format rpm

# AppImage
tml package --format appimage
```

### 12.2 Windows Distribution

```bash
# Standalone .exe
tml build --release --target x86_64-pc-windows-msvc

# Installer (MSI)
tml package --format msi

# Installer (NSIS)
tml package --format nsis
```

### 12.3 macOS Distribution

```bash
# Universal binary
tml build --release --target universal-apple-darwin

# App bundle (.app)
tml package --format app

# Disk image (.dmg)
tml package --format dmg

# Notarization
tml notarize --apple-id $APPLE_ID
```

---

*Previous: [20-STDLIB.md](./20-STDLIB.md)*
*Next: [22-VERSIONING.md](./22-VERSIONING.md) — Versioning and Compatibility*
