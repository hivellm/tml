# Tasks: Cross-Compilation Support

## Progress: 0% (0/85 tasks complete)

## Phase 1: Target Triple Infrastructure

### 1.1 Core Files
- [ ] 1.1.1 Create `compiler/src/codegen/toolchain.hpp` with Toolchain struct
- [ ] 1.1.2 Create `compiler/src/codegen/toolchain.cpp` with toolchain discovery
- [ ] 1.1.3 Add toolchain files to CMakeLists.txt
- [ ] 1.1.4 Create unit tests for toolchain discovery

### 1.2 Enhance Target System
- [ ] 1.2.1 Extend `Target::from_triple()` to support all common triples
- [ ] 1.2.2 Add target validation (reject unsupported combinations)
- [ ] 1.2.3 Implement `Target::default_linker()` - return appropriate linker for target
- [ ] 1.2.4 Implement `Target::default_ar()` - return appropriate archiver for target
- [ ] 1.2.5 Implement `Target::object_extension()` - return `.o`, `.obj`, etc.
- [ ] 1.2.6 Implement `Target::exe_extension()` - return `.exe`, empty, `.wasm`, etc.
- [ ] 1.2.7 Implement `Target::static_lib_extension()` - return `.a`, `.lib`, etc.
- [ ] 1.2.8 Implement `Target::dynamic_lib_extension()` - return `.so`, `.dll`, `.dylib`, etc.
- [ ] 1.2.9 Implement `Target::llvm_triple()` - return LLVM target triple string
- [ ] 1.2.10 Implement `Target::data_layout()` - return LLVM data layout string

### 1.3 Add More Targets
- [ ] 1.3.1 Add `x86_64-apple-darwin` (macOS Intel)
- [ ] 1.3.2 Add `aarch64-apple-darwin` (macOS Apple Silicon)
- [ ] 1.3.3 Add `x86_64-unknown-linux-musl` (static Linux)
- [ ] 1.3.4 Add `aarch64-unknown-linux-gnu` (Linux ARM64)
- [ ] 1.3.5 Add `aarch64-unknown-linux-musl` (static Linux ARM64)
- [ ] 1.3.6 Add `x86_64-pc-windows-gnu` (Windows with MinGW)
- [ ] 1.3.7 Add `wasm32-unknown-unknown` (bare WebAssembly)
- [ ] 1.3.8 Add `wasm32-wasi` (WebAssembly with WASI)
- [ ] 1.3.9 Add `x86_64-unknown-freebsd` (FreeBSD)
- [ ] 1.3.10 Add `aarch64-linux-android` (Android ARM64)

## Phase 2: CLI Integration

### 2.1 Target Flag
- [ ] 2.1.1 Add `--target <triple>` flag to `tml build`
- [ ] 2.1.2 Add `--target <triple>` flag to `tml test` (for cross-testing with emulators)
- [ ] 2.1.3 Validate target triple on CLI and show helpful error if invalid

### 2.2 Targets Subcommand
- [ ] 2.2.1 Create `compiler/src/cli/cmd_targets.hpp` header
- [ ] 2.2.2 Create `compiler/src/cli/cmd_targets.cpp` implementation
- [ ] 2.2.3 Implement `tml targets list` - show all available targets by tier
- [ ] 2.2.4 Implement `tml targets info <triple>` - show target details
- [ ] 2.2.5 Implement `tml targets host` - show host target triple
- [ ] 2.2.6 Add targets subcommand to dispatcher

### 2.3 Output Directory Organization
- [ ] 2.3.1 Create target-specific output directories: `build/<target>/debug/`
- [ ] 2.3.2 Create target-specific output directories: `build/<target>/release/`
- [ ] 2.3.3 Keep separate caches per target for incremental compilation
- [ ] 2.3.4 Add `--target-dir` flag to override default output location

## Phase 3: LLVM Cross-Compilation

### 3.1 LLVM Target Configuration
- [ ] 3.1.1 Set correct LLVM target triple in generated IR
- [ ] 3.1.2 Set correct data layout string for target
- [ ] 3.1.3 Generate target-appropriate calling conventions
- [ ] 3.1.4 Handle endianness differences in code generation
- [ ] 3.1.5 Handle pointer size differences (32-bit vs 64-bit)
- [ ] 3.1.6 Generate correct struct layouts for target ABI

### 3.2 Platform-Specific Intrinsics
- [ ] 3.2.1 Map TML intrinsics to correct LLVM intrinsics per target
- [ ] 3.2.2 Handle atomic operations per target capability
- [ ] 3.2.3 Handle SIMD intrinsics per target (x86 SSE/AVX, ARM NEON)
- [ ] 3.2.4 Conditional intrinsic availability based on target

## Phase 4: Toolchain Discovery

### 4.1 Cross-Compiler Detection
- [ ] 4.1.1 Implement toolchain discovery for host platform
- [ ] 4.1.2 Implement toolchain discovery for LLVM/Clang cross-compilation
- [ ] 4.1.3 Implement toolchain discovery for MinGW (Windows targets on Linux/macOS)
- [ ] 4.1.4 Implement toolchain discovery for musl-cross (static Linux targets)
- [ ] 4.1.5 Implement toolchain discovery for Android NDK
- [ ] 4.1.6 Implement toolchain discovery for Emscripten (WebAssembly)
- [ ] 4.1.7 Implement toolchain discovery for WASI SDK

### 4.2 Sysroot Management
- [ ] 4.2.1 Auto-detect sysroot location for installed toolchains
- [ ] 4.2.2 Add `--sysroot <path>` CLI flag for manual override
- [ ] 4.2.3 Support `TML_SYSROOT_<TARGET>` environment variables

## Phase 5: Linker Configuration

### 5.1 Cross-Linker Selection
- [ ] 5.1.1 Select correct linker based on target (ld, lld, link.exe, etc.)
- [ ] 5.1.2 Pass correct linker flags for target platform
- [ ] 5.1.3 Handle library search paths for cross-compilation
- [ ] 5.1.4 Support `--linker <path>` to override default linker
- [ ] 5.1.5 Support `-L <path>` for additional library paths

### 5.2 Platform-Specific Linking
- [ ] 5.2.1 Handle Windows import libraries (.lib) generation
- [ ] 5.2.2 Handle Linux shared object versioning
- [ ] 5.2.3 Handle macOS framework linking
- [ ] 5.2.4 Handle static linking with musl
- [ ] 5.2.5 Handle WebAssembly linking and bundling

## Phase 6: Runtime Library Cross-Compilation

### 6.1 Essential Runtime
- [ ] 6.1.1 Cross-compile `runtime/essential.c` for each target
- [ ] 6.1.2 Create pre-compiled runtime archives per target
- [ ] 6.1.3 Store runtime in `lib/<target>/` directory

### 6.2 Standard Library
- [ ] 6.2.1 Implement platform-specific modules in std library
- [ ] 6.2.2 Use conditional compilation for OS-specific code
- [ ] 6.2.3 Handle missing functionality gracefully (e.g., no file I/O in wasm32)

## Phase 7: Testing Cross-Compilation

### 7.1 Cross-Test Support
- [ ] 7.1.1 Detect available emulators (QEMU, Wine, etc.)
- [ ] 7.1.2 Add `--runner <command>` flag for custom test runners
- [ ] 7.1.3 Support running Windows tests on Linux with Wine
- [ ] 7.1.4 Support running Linux tests on Windows with WSL
- [ ] 7.1.5 Support running ARM tests with QEMU
- [ ] 7.1.6 Support running WASM tests with wasmtime/wasmer

## Phase 8: Configuration File

### 8.1 Project Configuration
- [ ] 8.1.1 Support `[target.<triple>]` sections in tml.toml
- [ ] 8.1.2 Per-target linker configuration
- [ ] 8.1.3 Per-target compiler flags

### 8.2 User Configuration
- [ ] 8.2.1 Support global `~/.tml/config.toml` for toolchain paths
- [ ] 8.2.2 Support `TML_TARGET` environment variable for default target
- [ ] 8.2.3 Support `TML_LINKER_<TARGET>` environment variables

## Phase 9: Error Handling & Diagnostics

### 9.1 Helpful Error Messages
- [ ] 9.1.1 "Toolchain not found" error with installation instructions
- [ ] 9.1.2 "Sysroot not found" error with setup instructions
- [ ] 9.1.3 "Unsupported target" error listing similar valid targets
- [ ] 9.1.4 "Missing library" error for cross-dependencies

### 9.2 Doctor Command
- [ ] 9.2.1 Create `compiler/src/cli/cmd_doctor.cpp`
- [ ] 9.2.2 Implement `tml doctor` - show overall toolchain status
- [ ] 9.2.3 Show which targets are ready for cross-compilation
- [ ] 9.2.4 Show which targets need additional setup
- [ ] 9.2.5 Add `tml doctor --target <triple>` for target-specific diagnosis

## Phase 10: Documentation

### 10.1 User Documentation
- [ ] 10.1.1 Add cross-compilation section to docs/09-CLI.md
- [ ] 10.1.2 Create docs/15-CROSS-COMPILATION.md with full guide
- [ ] 10.1.3 Document toolchain installation for each platform
- [ ] 10.1.4 Document common cross-compilation scenarios
- [ ] 10.1.5 Troubleshooting guide for common issues

### 10.2 Examples
- [ ] 10.2.1 Example: Building Linux binary on Windows
- [ ] 10.2.2 Example: Building Windows binary on Linux
- [ ] 10.2.3 Example: Building macOS binary on Linux
- [ ] 10.2.4 Example: Building WebAssembly from any platform
- [ ] 10.2.5 Example: Building for Android

## Validation

### Functional Requirements
- [ ] V.1 Can compile for Linux from Windows with `--target x86_64-unknown-linux-gnu`
- [ ] V.2 Can compile for Windows from Linux with `--target x86_64-pc-windows-msvc`
- [ ] V.3 Can compile for macOS from Linux with `--target x86_64-apple-darwin`
- [ ] V.4 Can compile for WebAssembly from any platform
- [ ] V.5 Generated binaries run correctly on target platform
- [ ] V.6 Static linking works with musl targets
- [ ] V.7 Output directories are organized by target
- [ ] V.8 Incremental compilation works per-target

### Error Handling
- [ ] V.9 Clear error when toolchain not installed
- [ ] V.10 Helpful suggestions for fixing common issues
- [ ] V.11 `tml doctor` correctly identifies setup problems

### Documentation
- [ ] V.12 All supported targets documented
- [ ] V.13 Setup instructions for each host/target combination
- [ ] V.14 Examples compile and work as documented

## Priority Targets

### Tier 1 (Must Have)
1. `x86_64-pc-windows-msvc` (Windows 64-bit)
2. `x86_64-unknown-linux-gnu` (Linux 64-bit)
3. `x86_64-apple-darwin` (macOS Intel)
4. `aarch64-apple-darwin` (macOS Apple Silicon)
5. `wasm32-unknown-unknown` (WebAssembly)

### Tier 2 (Should Have)
1. `x86_64-unknown-linux-musl` (Static Linux)
2. `aarch64-unknown-linux-gnu` (Linux ARM64)
3. `x86_64-pc-windows-gnu` (Windows MinGW)
4. `wasm32-wasi` (WASM + WASI)

### Tier 3 (Nice to Have)
1. `aarch64-linux-android` (Android)
2. `x86_64-unknown-freebsd` (FreeBSD)
3. `riscv64gc-unknown-linux-gnu` (RISC-V)
