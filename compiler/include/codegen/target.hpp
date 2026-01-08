//! # Target Configuration
//!
//! This module defines target platform specifications for cross-compilation.
//! A target includes architecture, operating system, ABI, and derived properties
//! like pointer sizes and type alignments.
//!
//! ## Target Triples
//!
//! Targets are identified by LLVM-style triple strings:
//! - `x86_64-pc-windows-msvc` - Windows 64-bit with MSVC
//! - `x86_64-unknown-linux-gnu` - Linux 64-bit with glibc
//! - `aarch64-apple-darwin` - macOS on Apple Silicon
//!
//! ## Usage
//!
//! ```cpp
//! auto target = Target::host();  // Current platform
//! auto triple = target.to_triple();
//! auto layout = target.to_data_layout();
//! ```

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::codegen {

/// Target processor architecture.
enum class Arch {
    X86_64,  ///< 64-bit x86 (AMD64/Intel 64).
    Aarch64, ///< 64-bit ARM (ARM64/Apple Silicon).
    X86,     ///< 32-bit x86.
    Arm,     ///< 32-bit ARM.
    Wasm32,  ///< WebAssembly 32-bit.
    Wasm64,  ///< WebAssembly 64-bit.
    Unknown  ///< Unknown architecture.
};

/// Target operating system.
enum class OS {
    Windows, ///< Microsoft Windows.
    Linux,   ///< Linux.
    MacOS,   ///< Apple macOS.
    FreeBSD, ///< FreeBSD.
    None,    ///< Bare metal / freestanding.
    Unknown  ///< Unknown operating system.
};

/// Target environment / ABI.
enum class Env {
    MSVC,   ///< Microsoft Visual C++ ABI.
    GNU,    ///< GNU/GCC ABI.
    Musl,   ///< Musl libc.
    None,   ///< No specific environment.
    Unknown ///< Unknown environment.
};

/// Object file format.
enum class ObjectFormat {
    COFF,   ///< Windows PE/COFF.
    ELF,    ///< Linux/BSD ELF.
    MachO,  ///< macOS Mach-O.
    Wasm,   ///< WebAssembly.
    Unknown ///< Unknown format.
};

/// Complete target platform specification.
///
/// Encapsulates all platform-specific details needed for code generation:
/// architecture, OS, ABI, and derived properties like type sizes.
struct Target {
    Arch arch = Arch::X86_64;                        ///< Target architecture.
    OS os = OS::Windows;                             ///< Target operating system.
    Env env = Env::MSVC;                             ///< Target ABI/environment.
    ObjectFormat object_format = ObjectFormat::COFF; ///< Object file format.

    // Derived properties
    int pointer_width = 64;       ///< Pointer size in bits.
    int pointer_align = 8;        ///< Pointer alignment in bytes.
    bool is_little_endian = true; ///< Endianness.

    // Type sizes (in bytes)
    int size_i8 = 1;    ///< Size of i8.
    int size_i16 = 2;   ///< Size of i16.
    int size_i32 = 4;   ///< Size of i32.
    int size_i64 = 8;   ///< Size of i64.
    int size_i128 = 16; ///< Size of i128.
    int size_f32 = 4;   ///< Size of f32.
    int size_f64 = 8;   ///< Size of f64.
    int size_ptr = 8;   ///< Pointer size in bytes.

    // Alignment (in bytes)
    int align_i8 = 1;    ///< Alignment of i8.
    int align_i16 = 2;   ///< Alignment of i16.
    int align_i32 = 4;   ///< Alignment of i32.
    int align_i64 = 8;   ///< Alignment of i64.
    int align_i128 = 16; ///< Alignment of i128.
    int align_f32 = 4;   ///< Alignment of f32.
    int align_f64 = 8;   ///< Alignment of f64.
    int align_ptr = 8;   ///< Alignment of pointers.

    /// Returns the LLVM target triple string (e.g., `x86_64-pc-windows-msvc`).
    [[nodiscard]] auto to_triple() const -> std::string;

    /// Returns the LLVM data layout string.
    [[nodiscard]] auto to_data_layout() const -> std::string;

    /// Parses a target triple string into a Target.
    static auto from_triple(const std::string& triple) -> std::optional<Target>;

    /// Returns the host platform target (current machine).
    static auto host() -> Target;

    // Predefined targets

    /// Windows 64-bit with MSVC.
    static auto x86_64_windows_msvc() -> Target;
    /// Linux 64-bit with GNU libc.
    static auto x86_64_linux_gnu() -> Target;
    /// Linux ARM64 with GNU libc.
    static auto aarch64_linux_gnu() -> Target;
    /// WebAssembly 32-bit.
    static auto wasm32_unknown() -> Target;
    /// macOS 64-bit.
    static auto x86_64_macos() -> Target;

    /// Returns a list of all known target triple names.
    static auto known_targets() -> std::vector<std::string>;

    /// Returns true if this target differs from the host.
    [[nodiscard]] auto is_cross_compile() const -> bool;
};

// ============================================================================
// Enum Conversion Utilities
// ============================================================================

/// Converts an Arch enum to its string representation.
auto arch_to_string(Arch arch) -> std::string;
/// Converts an OS enum to its string representation.
auto os_to_string(OS os) -> std::string;
/// Converts an Env enum to its string representation.
auto env_to_string(Env env) -> std::string;

/// Parses a string to an Arch enum.
auto string_to_arch(const std::string& s) -> Arch;
/// Parses a string to an OS enum.
auto string_to_os(const std::string& s) -> OS;
/// Parses a string to an Env enum.
auto string_to_env(const std::string& s) -> Env;

} // namespace tml::codegen
