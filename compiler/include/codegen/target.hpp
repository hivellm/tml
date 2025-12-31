#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace tml::codegen {

// Target architecture
enum class Arch {
    X86_64,  // 64-bit x86 (AMD64/Intel 64)
    Aarch64, // 64-bit ARM (ARM64)
    X86,     // 32-bit x86
    Arm,     // 32-bit ARM
    Wasm32,  // WebAssembly 32-bit
    Wasm64,  // WebAssembly 64-bit
    Unknown
};

// Target operating system
enum class OS {
    Windows,
    Linux,
    MacOS,
    FreeBSD,
    None, // Bare metal / freestanding
    Unknown
};

// Target environment/ABI
enum class Env {
    MSVC, // Microsoft Visual C++
    GNU,  // GNU/GCC
    Musl, // Musl libc
    None, // No specific environment
    Unknown
};

// Object format
enum class ObjectFormat {
    COFF,  // Windows
    ELF,   // Linux, BSDs
    MachO, // macOS
    Wasm,  // WebAssembly
    Unknown
};

// Target specification
struct Target {
    Arch arch = Arch::X86_64;
    OS os = OS::Windows;
    Env env = Env::MSVC;
    ObjectFormat object_format = ObjectFormat::COFF;

    // Derived properties
    int pointer_width = 64; // Pointer size in bits
    int pointer_align = 8;  // Pointer alignment in bytes
    bool is_little_endian = true;

    // Type sizes (in bytes)
    int size_i8 = 1;
    int size_i16 = 2;
    int size_i32 = 4;
    int size_i64 = 8;
    int size_i128 = 16;
    int size_f32 = 4;
    int size_f64 = 8;
    int size_ptr = 8; // Pointer size in bytes

    // Alignment (in bytes)
    int align_i8 = 1;
    int align_i16 = 2;
    int align_i32 = 4;
    int align_i64 = 8;
    int align_i128 = 16;
    int align_f32 = 4;
    int align_f64 = 8;
    int align_ptr = 8;

    // Get the LLVM target triple string
    [[nodiscard]] auto to_triple() const -> std::string;

    // Get the LLVM data layout string
    [[nodiscard]] auto to_data_layout() const -> std::string;

    // Parse a target triple string (e.g., "x86_64-pc-windows-msvc")
    static auto from_triple(const std::string& triple) -> std::optional<Target>;

    // Get the host target (current platform)
    static auto host() -> Target;

    // Get predefined targets
    static auto x86_64_windows_msvc() -> Target;
    static auto x86_64_linux_gnu() -> Target;
    static auto aarch64_linux_gnu() -> Target;
    static auto wasm32_unknown() -> Target;
    static auto x86_64_macos() -> Target;

    // Get a list of all known target triple names
    static auto known_targets() -> std::vector<std::string>;

    // Check if target is valid for cross-compilation
    [[nodiscard]] auto is_cross_compile() const -> bool;
};

// Convert enum to string
auto arch_to_string(Arch arch) -> std::string;
auto os_to_string(OS os) -> std::string;
auto env_to_string(Env env) -> std::string;

// Parse string to enum
auto string_to_arch(const std::string& s) -> Arch;
auto string_to_os(const std::string& s) -> OS;
auto string_to_env(const std::string& s) -> Env;

} // namespace tml::codegen
