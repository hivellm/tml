// LLVM IR generator - Target specification
// Handles cross-compilation targets and platform-specific details

#include "codegen/target.hpp"

#include <algorithm>
#include <sstream>

namespace tml::codegen {

// ============================================================================
// String conversions
// ============================================================================

auto arch_to_string(Arch arch) -> std::string {
    switch (arch) {
    case Arch::X86_64:
        return "x86_64";
    case Arch::Aarch64:
        return "aarch64";
    case Arch::X86:
        return "i686";
    case Arch::Arm:
        return "arm";
    case Arch::Wasm32:
        return "wasm32";
    case Arch::Wasm64:
        return "wasm64";
    case Arch::Unknown:
    default:
        return "unknown";
    }
}

auto os_to_string(OS os) -> std::string {
    switch (os) {
    case OS::Windows:
        return "windows";
    case OS::Linux:
        return "linux";
    case OS::MacOS:
        return "darwin";
    case OS::FreeBSD:
        return "freebsd";
    case OS::None:
        return "none";
    case OS::Unknown:
    default:
        return "unknown";
    }
}

auto env_to_string(Env env) -> std::string {
    switch (env) {
    case Env::MSVC:
        return "msvc";
    case Env::GNU:
        return "gnu";
    case Env::Musl:
        return "musl";
    case Env::None:
        return "";
    case Env::Unknown:
    default:
        return "unknown";
    }
}

auto string_to_arch(const std::string& s) -> Arch {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "x86_64" || lower == "x86-64" || lower == "amd64")
        return Arch::X86_64;
    if (lower == "aarch64" || lower == "arm64")
        return Arch::Aarch64;
    if (lower == "i686" || lower == "i386" || lower == "x86")
        return Arch::X86;
    if (lower == "arm" || lower == "armv7")
        return Arch::Arm;
    if (lower == "wasm32")
        return Arch::Wasm32;
    if (lower == "wasm64")
        return Arch::Wasm64;

    return Arch::Unknown;
}

auto string_to_os(const std::string& s) -> OS {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "windows" || lower == "win32")
        return OS::Windows;
    if (lower == "linux")
        return OS::Linux;
    if (lower == "darwin" || lower == "macos" || lower == "macosx")
        return OS::MacOS;
    if (lower == "freebsd")
        return OS::FreeBSD;
    if (lower == "none" || lower == "unknown")
        return OS::None;

    return OS::Unknown;
}

auto string_to_env(const std::string& s) -> Env {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "msvc")
        return Env::MSVC;
    if (lower == "gnu")
        return Env::GNU;
    if (lower == "musl")
        return Env::Musl;
    if (lower.empty() || lower == "none")
        return Env::None;

    return Env::Unknown;
}

// ============================================================================
// Target methods
// ============================================================================

auto Target::to_triple() const -> std::string {
    std::string triple = arch_to_string(arch);

    // Add vendor (usually "pc" for x86, "unknown" for others)
    if (os == OS::Windows) {
        triple += "-pc";
    } else if (os == OS::MacOS) {
        triple += "-apple";
    } else {
        triple += "-unknown";
    }

    // Add OS
    triple += "-" + os_to_string(os);

    // Add environment if not empty
    std::string env_str = env_to_string(env);
    if (!env_str.empty()) {
        triple += "-" + env_str;
    }

    return triple;
}

auto Target::to_data_layout() const -> std::string {
    // LLVM data layout string
    // Format: endianness-mangling-pointer_size-alignments...

    std::ostringstream layout;

    // Endianness
    layout << (is_little_endian ? "e" : "E");

    // Mangling style
    if (os == OS::Windows) {
        layout << "-m:w"; // Windows COFF mangling
    } else if (os == OS::MacOS) {
        layout << "-m:o"; // Mach-O mangling
    } else {
        layout << "-m:e"; // ELF mangling
    }

    // Pointer size
    layout << "-p:" << pointer_width << ":" << (pointer_align * 8) << ":" << (pointer_align * 8);

    // Integer alignments
    layout << "-i1:8:8"; // i1 aligned to 8 bits
    layout << "-i8:8:8";
    layout << "-i16:16:16";
    layout << "-i32:32:32";
    layout << "-i64:64:64";
    if (arch == Arch::X86_64 || arch == Arch::Aarch64) {
        layout << "-i128:128:128";
    }

    // Float alignments
    layout << "-f32:32:32";
    layout << "-f64:64:64";

    // Vector alignments (common defaults)
    layout << "-v64:64:64";
    layout << "-v128:128:128";
    layout << "-v256:256:256";

    // Aggregate alignment
    layout << "-a:0:64";

    // Native integer widths
    if (pointer_width == 64) {
        layout << "-n8:16:32:64";
    } else {
        layout << "-n8:16:32";
    }

    // Stack alignment
    layout << "-S128";

    return layout.str();
}

auto Target::from_triple(const std::string& triple) -> std::optional<Target> {
    // Parse triple: arch-vendor-os-env
    std::vector<std::string> parts;
    std::istringstream iss(triple);
    std::string part;
    while (std::getline(iss, part, '-')) {
        parts.push_back(part);
    }

    if (parts.empty()) {
        return std::nullopt;
    }

    Target target;

    // Parse architecture
    target.arch = string_to_arch(parts[0]);
    if (target.arch == Arch::Unknown) {
        return std::nullopt;
    }

    // Set pointer size based on architecture
    switch (target.arch) {
    case Arch::X86_64:
    case Arch::Aarch64:
    case Arch::Wasm64:
        target.pointer_width = 64;
        target.pointer_align = 8;
        target.size_ptr = 8;
        target.align_ptr = 8;
        break;
    case Arch::X86:
    case Arch::Arm:
    case Arch::Wasm32:
        target.pointer_width = 32;
        target.pointer_align = 4;
        target.size_ptr = 4;
        target.align_ptr = 4;
        break;
    default:
        break;
    }

    // Skip vendor (parts[1] if present)

    // Parse OS (usually parts[2])
    if (parts.size() >= 3) {
        target.os = string_to_os(parts[2]);
    }

    // Parse environment (usually parts[3])
    if (parts.size() >= 4) {
        target.env = string_to_env(parts[3]);
    } else {
        // Default environment based on OS
        if (target.os == OS::Windows) {
            target.env = Env::MSVC;
        } else if (target.os == OS::Linux) {
            target.env = Env::GNU;
        } else {
            target.env = Env::None;
        }
    }

    // Set object format based on OS
    switch (target.os) {
    case OS::Windows:
        target.object_format = ObjectFormat::COFF;
        break;
    case OS::MacOS:
        target.object_format = ObjectFormat::MachO;
        break;
    case OS::Linux:
    case OS::FreeBSD:
        target.object_format = ObjectFormat::ELF;
        break;
    case OS::None:
        if (target.arch == Arch::Wasm32 || target.arch == Arch::Wasm64) {
            target.object_format = ObjectFormat::Wasm;
        } else {
            target.object_format = ObjectFormat::ELF;
        }
        break;
    default:
        target.object_format = ObjectFormat::Unknown;
        break;
    }

    return target;
}

auto Target::host() -> Target {
#if defined(_WIN32) || defined(_WIN64)
#if defined(_M_X64) || defined(__x86_64__)
    return x86_64_windows_msvc();
#elif defined(_M_ARM64)
    Target target;
    target.arch = Arch::Aarch64;
    target.os = OS::Windows;
    target.env = Env::MSVC;
    target.object_format = ObjectFormat::COFF;
    target.pointer_width = 64;
    target.pointer_align = 8;
    target.size_ptr = 8;
    target.align_ptr = 8;
    return target;
#else
    return x86_64_windows_msvc(); // Default
#endif
#elif defined(__linux__)
#if defined(__x86_64__)
    return x86_64_linux_gnu();
#elif defined(__aarch64__)
    return aarch64_linux_gnu();
#else
    return x86_64_linux_gnu(); // Default
#endif
#elif defined(__APPLE__)
    return x86_64_macos();
#else
    // Default to x86_64-linux-gnu for unknown platforms
    return x86_64_linux_gnu();
#endif
}

auto Target::x86_64_windows_msvc() -> Target {
    Target target;
    target.arch = Arch::X86_64;
    target.os = OS::Windows;
    target.env = Env::MSVC;
    target.object_format = ObjectFormat::COFF;
    target.pointer_width = 64;
    target.pointer_align = 8;
    target.size_ptr = 8;
    target.align_ptr = 8;
    target.is_little_endian = true;
    return target;
}

auto Target::x86_64_linux_gnu() -> Target {
    Target target;
    target.arch = Arch::X86_64;
    target.os = OS::Linux;
    target.env = Env::GNU;
    target.object_format = ObjectFormat::ELF;
    target.pointer_width = 64;
    target.pointer_align = 8;
    target.size_ptr = 8;
    target.align_ptr = 8;
    target.is_little_endian = true;
    return target;
}

auto Target::aarch64_linux_gnu() -> Target {
    Target target;
    target.arch = Arch::Aarch64;
    target.os = OS::Linux;
    target.env = Env::GNU;
    target.object_format = ObjectFormat::ELF;
    target.pointer_width = 64;
    target.pointer_align = 8;
    target.size_ptr = 8;
    target.align_ptr = 8;
    target.is_little_endian = true;
    return target;
}

auto Target::wasm32_unknown() -> Target {
    Target target;
    target.arch = Arch::Wasm32;
    target.os = OS::None;
    target.env = Env::None;
    target.object_format = ObjectFormat::Wasm;
    target.pointer_width = 32;
    target.pointer_align = 4;
    target.size_ptr = 4;
    target.align_ptr = 4;
    target.is_little_endian = true;
    return target;
}

auto Target::x86_64_macos() -> Target {
    Target target;
    target.arch = Arch::X86_64;
    target.os = OS::MacOS;
    target.env = Env::None;
    target.object_format = ObjectFormat::MachO;
    target.pointer_width = 64;
    target.pointer_align = 8;
    target.size_ptr = 8;
    target.align_ptr = 8;
    target.is_little_endian = true;
    return target;
}

auto Target::known_targets() -> std::vector<std::string> {
    return {"x86_64-pc-windows-msvc",    "x86_64-unknown-linux-gnu",   "x86_64-unknown-linux-musl",
            "aarch64-unknown-linux-gnu", "aarch64-unknown-linux-musl", "x86_64-apple-darwin",
            "aarch64-apple-darwin",      "wasm32-unknown-unknown",     "wasm64-unknown-unknown"};
}

auto Target::is_cross_compile() const -> bool {
    Target host_target = host();
    return arch != host_target.arch || os != host_target.os;
}

} // namespace tml::codegen
