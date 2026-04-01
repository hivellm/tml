# Cross-Compilation Specification

## Purpose

The Cross-Compilation system enables building TML programs for different target platforms from any host OS. This follows the Rust model of target triples and toolchain discovery.

## ADDED Requirements

### Requirement: Target Triple Parsing
The compiler SHALL parse target triples in the format `<arch>-<vendor>-<os>[-<env>]`.

#### Scenario: Parse Windows target triple
Given the target triple "x86_64-pc-windows-msvc"
When parsing the target
Then arch is "x86_64", vendor is "pc", os is "windows", env is "msvc"

#### Scenario: Parse Linux target triple
Given the target triple "x86_64-unknown-linux-gnu"
When parsing the target
Then arch is "x86_64", vendor is "unknown", os is "linux", env is "gnu"

#### Scenario: Parse macOS target triple
Given the target triple "aarch64-apple-darwin"
When parsing the target
Then arch is "aarch64", vendor is "apple", os is "macos", env is ""

#### Scenario: Parse musl target triple
Given the target triple "x86_64-unknown-linux-musl"
When parsing the target
Then arch is "x86_64", vendor is "unknown", os is "linux", env is "musl"

#### Scenario: Parse WebAssembly target triple
Given the target triple "wasm32-unknown-unknown"
When parsing the target
Then arch is "wasm32", vendor is "unknown", os is "none", env is ""

#### Scenario: Parse WASI target triple
Given the target triple "wasm32-wasi"
When parsing the target
Then arch is "wasm32", vendor is "unknown", os is "wasi", env is ""

#### Scenario: Invalid target triple
Given the target triple "invalid-triple"
When parsing the target
Then an error is returned with message containing "invalid target"

### Requirement: Host Target Detection
The compiler MUST auto-detect the host platform when no --target is specified.

#### Scenario: Windows host detection
Given the compiler is running on Windows x86_64
When no --target flag is provided
Then target is set to "x86_64-pc-windows-msvc"

#### Scenario: Linux host detection
Given the compiler is running on Linux x86_64
When no --target flag is provided
Then target is set to "x86_64-unknown-linux-gnu"

#### Scenario: macOS Intel host detection
Given the compiler is running on macOS x86_64
When no --target flag is provided
Then target is set to "x86_64-apple-darwin"

#### Scenario: macOS Apple Silicon host detection
Given the compiler is running on macOS ARM64
When no --target flag is provided
Then target is set to "aarch64-apple-darwin"

### Requirement: Target Extension Methods
The Target struct SHALL provide platform-specific extension methods.

#### Scenario: Object extension for Windows
Given target is x86_64-pc-windows-msvc
When calling target.object_extension()
Then result is ".obj"

#### Scenario: Object extension for Unix
Given target is x86_64-unknown-linux-gnu
When calling target.object_extension()
Then result is ".o"

#### Scenario: Executable extension for Windows
Given target is x86_64-pc-windows-msvc
When calling target.exe_extension()
Then result is ".exe"

#### Scenario: Executable extension for Unix
Given target is x86_64-unknown-linux-gnu
When calling target.exe_extension()
Then result is "" (empty)

#### Scenario: Executable extension for WebAssembly
Given target is wasm32-unknown-unknown
When calling target.exe_extension()
Then result is ".wasm"

#### Scenario: Static library extension for Windows MSVC
Given target is x86_64-pc-windows-msvc
When calling target.static_lib_extension()
Then result is ".lib"

#### Scenario: Static library extension for Unix
Given target is x86_64-unknown-linux-gnu
When calling target.static_lib_extension()
Then result is ".a"

#### Scenario: Dynamic library extension for Windows
Given target is x86_64-pc-windows-msvc
When calling target.dynamic_lib_extension()
Then result is ".dll"

#### Scenario: Dynamic library extension for Linux
Given target is x86_64-unknown-linux-gnu
When calling target.dynamic_lib_extension()
Then result is ".so"

#### Scenario: Dynamic library extension for macOS
Given target is x86_64-apple-darwin
When calling target.dynamic_lib_extension()
Then result is ".dylib"

### Requirement: LLVM Triple Generation
The Target struct SHALL generate correct LLVM target triples.

#### Scenario: LLVM triple for Windows
Given target is x86_64-pc-windows-msvc
When calling target.llvm_triple()
Then result is "x86_64-pc-windows-msvc"

#### Scenario: LLVM triple for Linux
Given target is x86_64-unknown-linux-gnu
When calling target.llvm_triple()
Then result is "x86_64-unknown-linux-gnu"

#### Scenario: LLVM triple for macOS
Given target is aarch64-apple-darwin
When calling target.llvm_triple()
Then result is "aarch64-apple-darwin"

### Requirement: Data Layout Generation
The Target struct SHALL generate correct LLVM data layout strings.

#### Scenario: Data layout for x86_64 Linux
Given target is x86_64-unknown-linux-gnu
When calling target.data_layout()
Then result contains "e-m:e" (ELF mangling, little-endian)
And result contains "p:64:64" (64-bit pointers)

#### Scenario: Data layout for x86_64 Windows
Given target is x86_64-pc-windows-msvc
When calling target.data_layout()
Then result contains "e-m:w" (Windows mangling, little-endian)
And result contains "p:64:64" (64-bit pointers)

#### Scenario: Data layout for wasm32
Given target is wasm32-unknown-unknown
When calling target.data_layout()
Then result contains "p:32:32" (32-bit pointers)

### Requirement: CLI Target Flag
The compiler SHALL support --target flag for cross-compilation.

#### Scenario: Explicit target flag
Given the command `tml build file.tml --target x86_64-unknown-linux-gnu`
When compiling
Then target is set to x86_64-unknown-linux-gnu
And LLVM IR contains `target triple = "x86_64-unknown-linux-gnu"`

#### Scenario: Invalid target flag
Given the command `tml build file.tml --target invalid-target`
When parsing arguments
Then an error is returned listing valid targets

### Requirement: CLI Targets Subcommand
The compiler SHALL support `tml targets` subcommand.

#### Scenario: List all targets
Given the command `tml targets list`
When executing
Then output lists all supported targets grouped by tier

#### Scenario: Show target info
Given the command `tml targets info x86_64-unknown-linux-gnu`
When executing
Then output shows architecture, OS, environment, and default toolchain

#### Scenario: Show host target
Given the command `tml targets host`
When executing
Then output shows the auto-detected host target triple

### Requirement: Output Directory Organization
Build output SHALL be organized by target.

#### Scenario: Host target output directory
Given no --target flag
When building in debug mode
Then output goes to `build/debug/`

#### Scenario: Cross-compilation output directory
Given --target x86_64-unknown-linux-gnu
When building in debug mode
Then output goes to `build/x86_64-unknown-linux-gnu/debug/`

#### Scenario: Cross-compilation release directory
Given --target x86_64-unknown-linux-gnu and --release flag
When building
Then output goes to `build/x86_64-unknown-linux-gnu/release/`

#### Scenario: Target directory override
Given --target-dir /custom/path
When building
Then output goes to `/custom/path/`

### Requirement: Toolchain Discovery
The compiler SHALL discover cross-compilation toolchains.

#### Scenario: Find LLVM/Clang toolchain
Given LLVM/Clang is installed
When searching for toolchain for x86_64-unknown-linux-gnu
Then clang with --target flag is detected

#### Scenario: Find MinGW toolchain
Given MinGW is installed on Windows
When searching for toolchain for x86_64-unknown-linux-gnu
Then x86_64-linux-gnu-gcc is detected

#### Scenario: Find Android NDK toolchain
Given Android NDK is installed
When searching for toolchain for aarch64-linux-android
Then NDK clang is detected

#### Scenario: Toolchain not found
Given no cross-compiler is installed for target
When searching for toolchain
Then an error is returned with installation instructions

### Requirement: Linker Selection
The compiler SHALL select correct linker for target.

#### Scenario: Default linker for Windows target
Given target is x86_64-pc-windows-msvc
When selecting linker
Then link.exe is selected

#### Scenario: Default linker for Linux target
Given target is x86_64-unknown-linux-gnu
When selecting linker
Then ld or lld is selected

#### Scenario: Linker override
Given --linker /custom/linker flag
When linking
Then /custom/linker is used instead of default

### Requirement: Sysroot Configuration
The compiler SHALL support sysroot configuration for cross-compilation.

#### Scenario: Auto-detect sysroot
Given cross-compiler is installed with sysroot
When building for target
Then sysroot is auto-detected

#### Scenario: CLI sysroot override
Given --sysroot /path/to/sysroot flag
When building for target
Then specified sysroot is used

#### Scenario: Environment variable sysroot
Given TML_SYSROOT_X86_64_UNKNOWN_LINUX_GNU=/path/to/sysroot
When building for x86_64-unknown-linux-gnu
Then environment variable sysroot is used

### Requirement: LLVM IR Target Configuration
Generated LLVM IR SHALL include correct target configuration.

#### Scenario: Target triple in IR
Given target is aarch64-unknown-linux-gnu
When generating LLVM IR
Then IR contains `target triple = "aarch64-unknown-linux-gnu"`

#### Scenario: Data layout in IR
Given target is aarch64-unknown-linux-gnu
When generating LLVM IR
Then IR contains appropriate `target datalayout` for AArch64

#### Scenario: Calling convention for target
Given target is x86_64-pc-windows-msvc
When generating function calls
Then Windows x64 calling convention is used

### Requirement: Pointer Size Handling
The compiler SHALL handle different pointer sizes correctly.

#### Scenario: 64-bit pointers
Given target is x86_64-unknown-linux-gnu
When generating code for pointers
Then pointers are 8 bytes (64 bits)

#### Scenario: 32-bit pointers
Given target is wasm32-unknown-unknown
When generating code for pointers
Then pointers are 4 bytes (32 bits)

### Requirement: Error Messages
The compiler SHALL provide helpful error messages for cross-compilation issues.

#### Scenario: Toolchain not found error
Given toolchain for target is not installed
When attempting to build
Then error message includes installation instructions for the toolchain

#### Scenario: Sysroot not found error
Given sysroot for target is not found
When attempting to link
Then error message explains how to configure sysroot

#### Scenario: Invalid target error
Given invalid target triple "not-a-target"
When attempting to build
Then error lists similar valid targets

### Requirement: Doctor Command
The compiler SHALL provide diagnostic command for cross-compilation setup.

#### Scenario: Doctor shows available toolchains
Given the command `tml doctor`
When executing
Then output shows which targets have toolchains available

#### Scenario: Doctor shows missing toolchains
Given the command `tml doctor`
When executing
Then output shows which targets need toolchain installation

#### Scenario: Doctor shows target readiness
Given the command `tml doctor --target x86_64-unknown-linux-gnu`
When executing
Then output shows whether target is ready for cross-compilation

## Supported Targets Reference

### Tier 1 (Full Support)
- `x86_64-pc-windows-msvc` - Windows 64-bit (MSVC)
- `x86_64-unknown-linux-gnu` - Linux 64-bit (glibc)
- `x86_64-apple-darwin` - macOS Intel
- `aarch64-apple-darwin` - macOS Apple Silicon
- `wasm32-unknown-unknown` - WebAssembly

### Tier 2 (Standard Support)
- `x86_64-unknown-linux-musl` - Linux 64-bit (musl, static)
- `aarch64-unknown-linux-gnu` - Linux ARM64
- `x86_64-pc-windows-gnu` - Windows 64-bit (MinGW)
- `wasm32-wasi` - WebAssembly + WASI

### Tier 3 (Best Effort)
- `aarch64-linux-android` - Android ARM64
- `x86_64-unknown-freebsd` - FreeBSD 64-bit
- `riscv64gc-unknown-linux-gnu` - RISC-V 64-bit
- `aarch64-unknown-linux-musl` - Linux ARM64 (musl, static)
- `i686-unknown-linux-gnu` - Linux 32-bit
- `i686-pc-windows-msvc` - Windows 32-bit
